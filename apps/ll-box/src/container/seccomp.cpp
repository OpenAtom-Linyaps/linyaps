/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "seccomp.h"

#include "util/logger.h"

#include <seccomp.h>

namespace {

#define SYSCALL_PAIR(SYSCALL)                   \
    {                                           \
        LL_TOSTRING(SYSCALL), SCMP_SYS(SYSCALL) \
    }

std::vector<struct scmp_arg_cmp> toScmpArgCmpArray(const std::vector<linglong::SyscallArg> &args)
{
    static const std::map<std::string, scmp_compare> seccompArgOpMap = {
        { "_SCMP_CMP_MIN", _SCMP_CMP_MIN }, { "SCMP_CMP_NE", SCMP_CMP_NE },
        { "SCMP_CMP_LT", SCMP_CMP_LT },     { "SCMP_CMP_LE", SCMP_CMP_LE },
        { "SCMP_CMP_EQ", SCMP_CMP_EQ },     { "SCMP_CMP_GE", SCMP_CMP_GE },
        { "SCMP_CMP_GT", SCMP_CMP_GT },     { "SCMP_CMP_MASKED_EQ", SCMP_CMP_MASKED_EQ },
        { "_SCMP_CMP_MAX", _SCMP_CMP_MAX },
    };

    std::vector<struct scmp_arg_cmp> scmpArgs;

    unsigned int index = 0;
    for (auto const &arg : args) {
        scmpArgs.push_back({
          .arg = index,
          .op = seccompArgOpMap.at(arg.op),
          .datum_a = arg.value,
          .datum_b = arg.valueTwo,
        });
        ++index;
    }

    return scmpArgs;
}

int toSyscallNumber(const std::string &name)
{
    // unistd.h for example: __NR_read

    //  FIXME: need full support
    static const std::map<std::string, int> syscallNameMap = {
        SYSCALL_PAIR(read),          SYSCALL_PAIR(write),         SYSCALL_PAIR(open),
        SYSCALL_PAIR(stat),          SYSCALL_PAIR(getcwd),        SYSCALL_PAIR(chmod),
        SYSCALL_PAIR(syslog),        SYSCALL_PAIR(uselib),        SYSCALL_PAIR(acct),
        SYSCALL_PAIR(modify_ldt),    SYSCALL_PAIR(quotactl),      SYSCALL_PAIR(add_key),
        SYSCALL_PAIR(keyctl),        SYSCALL_PAIR(request_key),   SYSCALL_PAIR(move_pages),
        SYSCALL_PAIR(mbind),         SYSCALL_PAIR(get_mempolicy), SYSCALL_PAIR(set_mempolicy),
        SYSCALL_PAIR(migrate_pages), SYSCALL_PAIR(unshare),       SYSCALL_PAIR(mount),
        SYSCALL_PAIR(pivot_root),    SYSCALL_PAIR(clone),         SYSCALL_PAIR(ioctl),
    };

    return syscallNameMap.at(name);
}

} // namespace

namespace linglong {

int ConfigSeccomp(const std::optional<linglong::Seccomp> &seccomp)
{
    if (!seccomp.has_value()) {
        return 0;
    }

    static const std::map<std::string, int> seccompActionMap = {
        { "SCMP_ACT_KILL", SCMP_ACT_KILL },          { "SCMP_ACT_TRAP", SCMP_ACT_TRAP },
        { "SCMP_ACT_ERRNO", SCMP_ACT_ERRNO(EPERM) }, { "SCMP_ACT_TRACE", SCMP_ACT_TRACE(EPERM) },
        { "SCMP_ACT_ALLOW", SCMP_ACT_ALLOW },
    };

    // FIXME: need full support
    static const std::map<std::string, int> seccompArchMap = {
        { "SCMP_ARCH_X86", SCMP_ARCH_X86 },
        { "SCMP_ARCH_X86_64", SCMP_ARCH_X86_64 },
        { "SCMP_ARCH_X32", SCMP_ARCH_X32 },

        { "SCMP_ARCH_ARM", SCMP_ARCH_ARM },
        { "SCMP_ARCH_AARCH64", SCMP_ARCH_AARCH64 },

        { "SCMP_ARCH_MIPS", SCMP_ARCH_MIPS },
        { "SCMP_ARCH_MIPS64", SCMP_ARCH_MIPS64 },
        { "SCMP_ARCH_MIPS64N32", SCMP_ARCH_MIPS64N32 },
        { "SCMP_ARCH_MIPSEL", SCMP_ARCH_MIPSEL },
        { "SCMP_ARCH_MIPSEL64", SCMP_ARCH_MIPSEL64 },
        { "SCMP_ARCH_MIPSEL64N32", SCMP_ARCH_MIPSEL64N32 },
    };

    int ret;
    scmp_filter_ctx ctx = nullptr;

    try {
        auto defaultAction = seccompActionMap.at(seccomp->defaultAction);

        ctx = seccomp_init(defaultAction);
        if (ctx == nullptr) {
            throw std::runtime_error(util::errnoString()
                                     + " seccomp_init=" + seccomp->defaultAction);
        }
        for (auto const &architecture : seccomp->architectures) {
            auto scmpArch = seccompArchMap.at(architecture);
            if (seccomp_arch_exist(ctx, scmpArch) == -EEXIST) {
                ret = seccomp_arch_add(ctx, scmpArch);
                if (ret != 0) {
                    throw std::runtime_error(util::errnoString() + " architecture=" + architecture);
                }
            }
        }

        for (auto const &syscall : seccomp->syscalls) {
            auto action = seccompActionMap.at(syscall.action);
            auto argc = syscall.args.size();
            auto args = toScmpArgCmpArray(syscall.args);

            for (auto const &name : syscall.names) {
                auto sysNumber = toSyscallNumber(name);

                ret = seccomp_rule_add_array(ctx, action, sysNumber, argc, args.data());
                if (ret != 0) {
                    throw std::runtime_error(util::errnoString() + " syscall.name=" + name);
                }
            }
        }
        ret = seccomp_load(ctx);
    } catch (const std::exception &e) {
        logErr() << "config seccomp failed:" << e.what();
        ret = -1;
    } catch (...) {
        logErr() << "unknown error";
        ret = -1;
    }

    if (!ctx) {
        seccomp_release(ctx);
    }
    return ret;
}

} // namespace linglong
