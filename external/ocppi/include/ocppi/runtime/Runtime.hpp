#pragma once

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "ocppi/runtime/ContainerID.hpp"
#include "ocppi/runtime/SpecRuntime.hpp"
#include "tl/expected.hpp"

namespace ocppi
{
namespace types
{
struct ContainerListItem;
} // namespace types

namespace runtime
{
struct ExecOption;
struct ListOption;
struct RunOption;
} // namespace runtime
} // namespace ocppi

namespace ocppi::runtime
{

class Runtime : public virtual SpecRuntime {
    public:
        [[nodiscard]]
        virtual auto run(const ContainerID &id,
                         const std::filesystem::path &pathToBundle) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto run(const ContainerID &id,
                         const std::filesystem::path &pathToBundle,
                         const RunOption &option) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;

        [[nodiscard]]
        virtual auto exec(const ContainerID &id, const std::string &executable,
                          const std::vector<std::string> &command) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto exec(const ContainerID &id, const std::string &executable,
                          const std::vector<std::string> &command,
                          const ExecOption &option) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;

        [[nodiscard]]
        virtual auto list() noexcept
                -> tl::expected<std::vector<types::ContainerListItem>,
                                std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto list(const ListOption &option) noexcept
                -> tl::expected<std::vector<types::ContainerListItem>,
                                std::exception_ptr> = 0;
};

}
