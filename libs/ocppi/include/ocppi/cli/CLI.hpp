#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "ocppi/runtime/Runtime.hpp"

namespace ocppi
{
namespace runtime
{
struct CreateOption;
struct DeleteOption;
struct ExecOption;
struct FeaturesOption;
struct GlobalOption;
struct KillOption;
struct ListOption;
struct RunOption;
struct StartOption;
struct StateOption;
} // namespace runtime
} // namespace ocppi

namespace ocppi::cli
{

class CLI : public virtual runtime::Runtime {
    public:
        [[nodiscard]]
        virtual auto bin() const noexcept -> const std::filesystem::path & = 0;

    protected:
        [[nodiscard]]
        virtual auto generateGlobalOptions(const runtime::GlobalOption &option)
                const noexcept -> std::vector<std::string> = 0;

        [[nodiscard]]
        virtual auto generateSubcommandOptions(
                const runtime::FeaturesOption &option) const noexcept
                -> std::vector<std::string> = 0;
        [[nodiscard]]
        virtual auto generateSubcommandOptions(
                const runtime::CreateOption &option) const noexcept
                -> std::vector<std::string> = 0;
        [[nodiscard]]
        virtual auto generateSubcommandOptions(
                const runtime::DeleteOption &option) const noexcept
                -> std::vector<std::string> = 0;
        [[nodiscard]]
        virtual auto generateSubcommandOptions(
                const runtime::ExecOption &option) const noexcept
                -> std::vector<std::string> = 0;
        [[nodiscard]]
        virtual auto generateSubcommandOptions(
                const runtime::KillOption &option) const noexcept
                -> std::vector<std::string> = 0;
        [[nodiscard]]
        virtual auto generateSubcommandOptions(
                const runtime::ListOption &option) const noexcept
                -> std::vector<std::string> = 0;
        [[nodiscard]]
        virtual auto generateSubcommandOptions(
                const runtime::StartOption &option) const noexcept
                -> std::vector<std::string> = 0;
        [[nodiscard]]
        virtual auto generateSubcommandOptions(
                const runtime::StateOption &option) const noexcept
                -> std::vector<std::string> = 0;
        [[nodiscard]]
        virtual auto generateSubcommandOptions(const runtime::RunOption &option)
                const noexcept -> std::vector<std::string> = 0;
};

}
