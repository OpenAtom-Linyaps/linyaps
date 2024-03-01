#include "ocppi/cli/CommonCLI.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"
#include "ocppi/cli/CommandFailedError.hpp"
#include "ocppi/cli/ExecutableNotFoundError.hpp"
#include "ocppi/cli/Process.hpp"
#include "ocppi/runtime/ContainerID.hpp"
#include "ocppi/runtime/CreateOption.hpp"
#include "ocppi/runtime/DeleteOption.hpp"
#include "ocppi/runtime/ExecOption.hpp"
#include "ocppi/runtime/FeaturesOption.hpp"
#include "ocppi/runtime/GlobalOption.hpp"
#include "ocppi/runtime/KillOption.hpp"
#include "ocppi/runtime/ListOption.hpp"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/Signal.hpp"
#include "ocppi/runtime/StartOption.hpp"
#include "ocppi/runtime/StateOption.hpp"
#include "ocppi/runtime/features/types/Generators.hpp" // IWYU pragma: keep
#include "ocppi/runtime/state/types/Generators.hpp"    // IWYU pragma: keep
#include "ocppi/runtime/state/types/State.hpp"
#include "ocppi/types/ContainerListItem.hpp"
#include "ocppi/types/Generators.hpp" // IWYU pragma: keep

#ifdef OCPPI_WITH_SPDLOG
#include <memory>

#include "spdlog/sinks/null_sink.h"
#include "spdlog/spdlog.h"
#if !defined(SPDLOG_USE_STD_FORMAT)
#if !defined(SPDLOG_FMT_EXTERNAL)
#ifdef SPDLOG_HEADER_ONLY
#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif
#endif
#include "spdlog/fmt/bundled/ranges.h"
#else
#include "fmt/ranges.h"
#endif
#endif

namespace spdlog
{
class logger;
} // namespace spdlog
#endif

namespace ocppi::cli
{

namespace
{

template <typename Result>
auto doCommand(const std::string &bin,
#ifdef OCPPI_WITH_SPDLOG
               [[maybe_unused]] const std::shared_ptr<spdlog::logger> &logger,
#endif
               std::vector<std::string> &&globalOption,
               const std::string &command, std::vector<std::string> &&options,
               std::vector<std::string> &&arguments) -> Result
{
        auto &args = globalOption;
        args.insert(args.end(), command);
        args.insert(args.end(), std::make_move_iterator(options.begin()),
                    std::make_move_iterator(options.end()));
        args.insert(args.end(), std::make_move_iterator(arguments.begin()),
                    std::make_move_iterator(arguments.end()));

#ifdef OCPPI_WITH_SPDLOG
        SPDLOG_LOGGER_DEBUG(logger, R"(Executing "{}" with arguments: {})", bin,
                            args);
#endif

        if constexpr (std::is_void_v<Result>) {
                auto ret = runProcess(bin, args);
                if (ret != 0) {
                        throw CommandFailedError(ret, bin);
                }
                return;
        } else {
                std::string output;
                auto ret = runProcess(bin, args, output);
                if (ret != 0) {
                        throw CommandFailedError(ret, bin);
                }

                auto json_result = nlohmann::json::parse(output);
                return json_result.get<Result>();
        }
}

}
#ifdef OCPPI_WITH_SPDLOG

CommonCLI::CommonCLI(std::filesystem::path bin,
                     const std::shared_ptr<spdlog::logger> &logger)
        : bin_(std::move(bin))
        , logger_(logger != nullptr ?
                          logger :
                          spdlog::create<spdlog::sinks::null_sink_st>(""))
{
        if (std::filesystem::exists(bin_)) {
                return;
        }
        throw ExecutableNotFoundError(bin_);
}

auto CommonCLI::logger() const -> const std::shared_ptr<spdlog::logger> &
{
        assert(this->logger_ != nullptr);
        return this->logger_;
}

#else
CommonCLI::CommonCLI(std::filesystem::path bin)
        : bin_(std::move(bin))
{
        if (std::filesystem::exists(bin_)) {
                return;
        }
        throw ExecutableNotFoundError(bin_);
}
#endif

auto CommonCLI::bin() const noexcept -> const std::filesystem::path &
{
        return this->bin_;
}

auto CommonCLI::state(const runtime::ContainerID &id) const noexcept
        -> tl::expected<runtime::state::types::State, std::exception_ptr>
{
        return this->state(id, {});
}

auto CommonCLI::state(const runtime::ContainerID &id,
                      const runtime::StateOption &option) const noexcept
        -> tl::expected<runtime::state::types::State, std::exception_ptr>
try {
        return doCommand<runtime::state::types::State>(
                this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                this->logger(),
#endif
                this->generateGlobalOptions(option), "state",
                this->generateSubcommandOptions(option), { id });
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::create(const runtime::ContainerID &id,
                       const std::filesystem::path &pathToBundle) noexcept
        -> tl::expected<void, std::exception_ptr>
{
        return this->create(id, pathToBundle, {});
}

auto CommonCLI::create(const runtime::ContainerID &id,
                       const std::filesystem::path &pathToBundle,
                       const runtime::CreateOption &option) noexcept
        -> tl::expected<void, std::exception_ptr>
try {
        auto opt = option;
        opt.extra.emplace_back("-b");
        opt.extra.emplace_back(pathToBundle);

        doCommand<void>(this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                        this->logger(),
#endif
                        this->generateGlobalOptions(opt), "create",
                        this->generateSubcommandOptions(opt), { id });
        return {};
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::start(const runtime::ContainerID &id) noexcept
        -> tl::expected<void, std::exception_ptr>
{
        return this->start(id, {});
}

auto CommonCLI::start(const runtime::ContainerID &id,
                      const runtime::StartOption &option) noexcept
        -> tl::expected<void, std::exception_ptr>
try {
        doCommand<void>(this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                        this->logger(),
#endif
                        this->generateGlobalOptions(option), "start",
                        this->generateSubcommandOptions(option), { id });
        return {};
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::kill(const runtime::ContainerID &id,
                     const runtime::Signal &signal) noexcept
        -> tl::expected<void, std::exception_ptr>
{
        return this->kill(id, signal, {});
}

auto CommonCLI::kill(const runtime::ContainerID &id,
                     const runtime::Signal &signal,
                     const runtime::KillOption &option) noexcept
        -> tl::expected<void, std::exception_ptr>
try {
        doCommand<void>(this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                        this->logger(),
#endif
                        this->generateGlobalOptions(option), "kill",
                        this->generateSubcommandOptions(option),
                        { id, signal });
        return {};

} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::delete_(const runtime::ContainerID &id) noexcept
        -> tl::expected<void, std::exception_ptr>
{
        return this->delete_(id, {});
}

auto CommonCLI::delete_(const runtime::ContainerID &id,
                        const runtime::DeleteOption &option) noexcept
        -> tl::expected<void, std::exception_ptr>
try {
        doCommand<void>(this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                        this->logger(),
#endif
                        this->generateGlobalOptions(option), "delete",
                        this->generateSubcommandOptions(option), { id });
        return {};
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::exec(const runtime::ContainerID &id,
                     const std::string &executable,
                     const std::vector<std::string> &command) noexcept
        -> tl::expected<void, std::exception_ptr>
{
        return this->exec(id, executable, command, {});
}

auto CommonCLI::exec(const runtime::ContainerID &id,
                     const std::string &executable,
                     const std::vector<std::string> &command,
                     const runtime::ExecOption &option) noexcept
        -> tl::expected<void, std::exception_ptr>
try {
        std::vector<std::string> arguments;
        arguments.push_back(id);
        arguments.push_back(executable);
        arguments.insert(arguments.end(), command.begin(), command.end());

        doCommand<void>(this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                        this->logger(),
#endif
                        this->generateGlobalOptions(option), "exec",
                        this->generateSubcommandOptions(option),
                        std::move(arguments));
        return {};
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::list() noexcept
        -> tl::expected<std::vector<types::ContainerListItem>,
                        std::exception_ptr>
{
        return this->list({});
}

auto CommonCLI::list(const runtime::ListOption &option) noexcept
        -> tl::expected<std::vector<types::ContainerListItem>,
                        std::exception_ptr>
try {
        runtime::ListOption new_option = option;
        new_option.format = runtime::ListOption::OutputFormat::Json;

        return doCommand<std::vector<types::ContainerListItem>>(
                this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                this->logger(),
#endif
                this->generateGlobalOptions(option), "list",
                this->generateSubcommandOptions(option), {});
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::run(const runtime::ContainerID &id,
                    const std::filesystem::path &pathToBundle) noexcept
        -> tl::expected<void, std::exception_ptr>
{
        return this->run(id, pathToBundle, {});
}

auto CommonCLI::run(const runtime::ContainerID &id,
                    const std::filesystem::path &pathToBundle,
                    const runtime::RunOption &option) noexcept
        -> tl::expected<void, std::exception_ptr>
try {
        auto opt = option;
        opt.extra.emplace_back("-b");
        opt.extra.emplace_back(pathToBundle);

        doCommand<void>(this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                        this->logger(),
#endif
                        this->generateGlobalOptions(opt), "run",
                        this->generateSubcommandOptions(opt), { id });
        return {};
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::features() const noexcept
        -> tl::expected<runtime::features::types::Features, std::exception_ptr>
{
        return this->features({});
}

auto CommonCLI::features(const runtime::FeaturesOption &option) const noexcept
        -> tl::expected<runtime::features::types::Features, std::exception_ptr>
try {
        return doCommand<runtime::features::types::Features>(
                this->bin(),
#ifdef OCPPI_WITH_SPDLOG
                this->logger(),
#endif
                this->generateGlobalOptions(option), "features",
                this->generateSubcommandOptions(option), {});
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto CommonCLI::generateGlobalOptions(const runtime::GlobalOption &option)
        const noexcept -> std::vector<std::string>
{
        auto ret = option.extra;
        if (option.root) {
                ret.emplace_back("--root");
                ret.push_back(option.root->string());
        }
        return ret;
}

auto CommonCLI::generateSubcommandOptions(const runtime::CreateOption &option)
        const noexcept -> std::vector<std::string>
{
        return option.extra;
}

auto CommonCLI::generateSubcommandOptions(const runtime::DeleteOption &option)
        const noexcept -> std::vector<std::string>
{
        return option.extra;
}

auto CommonCLI::generateSubcommandOptions(const runtime::ExecOption &option)
        const noexcept -> std::vector<std::string>
{
        std::vector<std::string> ret = option.extra;
        if (option.tty) {
                ret.emplace_back("--tty");
        }
        if (option.cwd) {
                ret.emplace_back("--cwd");
                ret.push_back(option.cwd->string());
        }
        for (const auto &env : option.env) {
                ret.emplace_back("--env");
                ret.push_back(env.first + "=" + env.second);
        }
        if (option.uid) {
                ret.emplace_back("--user");
                ret.push_back(
                        std::to_string(*option.uid) +
                        (option.gid ? ":" + std::to_string(*option.gid) : ""));
        }
        return ret;
}

auto CommonCLI::generateSubcommandOptions(const runtime::KillOption &option)
        const noexcept -> std::vector<std::string>
{
        return option.extra;
}

auto CommonCLI::generateSubcommandOptions(const runtime::ListOption &option)
        const noexcept -> std::vector<std::string>
{
        std::vector<std::string> ret = option.extra;

        if (option.format == runtime::ListOption::OutputFormat::Json) {
                ret.emplace_back("-f");
                ret.emplace_back("json");
        }

        return ret;
}

auto CommonCLI::generateSubcommandOptions(const runtime::StartOption &option)
        const noexcept -> std::vector<std::string>
{
        return option.extra;
}

auto CommonCLI::generateSubcommandOptions(const runtime::StateOption &option)
        const noexcept -> std::vector<std::string>
{
        return option.extra;
}

auto CommonCLI::generateSubcommandOptions(const runtime::RunOption &option)
        const noexcept -> std::vector<std::string>
{
        return option.extra;
}

auto CommonCLI::generateSubcommandOptions(const runtime::FeaturesOption &option)
        const noexcept -> std::vector<std::string>
{
        return option.extra;
}

}
