/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_PACKAGE_BUNDLE_P_H_
#define LINGLONG_SRC_MODULE_PACKAGE_BUNDLE_P_H_

#include "bundle.h"

namespace linglong {
namespace package {

class BundlePrivate
{
public:
    // Bundle *q_ptr = nullptr;
    QString bundleFilePath;
    QString erofsFilePath;
    QString bundleDataPath;
    int offsetValue;
    QString tmpWorkDir;
    QString buildArch;
    const QString linglongLoader = "/usr/libexec/linglong-loader";
    const QString configJson = "/info.json";

    explicit BundlePrivate(Bundle *parent)
    //    : q_ptr(parent)
    {
    }

    linglong::util::Error make(const QString &dataPath, const QString &outputFilePath);

    template<typename P>
    inline uint16_t file16ToCpu(uint16_t val, const P &ehdr)
    {
        if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
            val = bswap16(val);
        return val;
    }

    template<typename P>
    uint32_t file32ToCpu(uint32_t val, const P &ehdr)
    {
        if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
            val = bswap32(val);
        return val;
    }

    template<typename P>
    uint64_t file64ToCpu(uint64_t val, const P &ehdr)
    {
        if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
            val = bswap64(val);
        return val;
    }

    // read elf64
    auto readElf64(FILE *fd, Elf64_Ehdr &ehdr)
            -> decltype(ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum));

    // get elf offset size
    auto getElfSize(const QString elfFilePath) -> decltype(-1);

    linglong::util::Error push(const QString &bundleFilePath,
                               const QString &repoUrl,
                               const QString &repoChannel,
                               bool force);
};
} // namespace package
} // namespace linglong
#endif