#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ocppi/runtime/ContainerID.hpp"
#include "ocppi/runtime/CreateOption.hpp"
#include "ocppi/runtime/DeleteOption.hpp"
#include "ocppi/runtime/FeaturesOption.hpp"
#include "ocppi/runtime/KillOption.hpp"
#include "ocppi/runtime/Signal.hpp"
#include "ocppi/runtime/StartOption.hpp"
#include "ocppi/runtime/StateOption.hpp"
#include "ocppi/runtime/features/types/Features.hpp"
#include "ocppi/runtime/state/types/State.hpp"
#include "tl/expected.hpp"

namespace ocppi::runtime
{

namespace state::types
{
struct State;
}

class SpecRuntime {
    public:
        SpecRuntime() = default;
        SpecRuntime(const SpecRuntime &) = delete;
        SpecRuntime(SpecRuntime &&) = delete;
        auto operator=(const SpecRuntime &) -> SpecRuntime & = delete;
        auto operator=(SpecRuntime &&) -> SpecRuntime & = delete;
        virtual ~SpecRuntime() = default;

        [[nodiscard]]
        virtual auto features() const noexcept
                -> tl::expected<features::types::Features,
                                std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto features(const FeaturesOption &option) const noexcept
                -> tl::expected<features::types::Features,
                                std::exception_ptr> = 0;

        [[nodiscard]]
        virtual auto state(const ContainerID &id) const noexcept
                -> tl::expected<state::types::State, std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto state(const ContainerID &id,
                           const StateOption &option) const noexcept
                -> tl::expected<state::types::State, std::exception_ptr> = 0;

        [[nodiscard]]
        virtual auto create(const ContainerID &id,
                            const std::filesystem::path &pathToBundle) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto create(const ContainerID &id,
                            const std::filesystem::path &pathToBundle,
                            const CreateOption &option) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;

        [[nodiscard]]
        virtual auto start(const ContainerID &id) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto start(const ContainerID &id,
                           const StartOption &option) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;

        [[nodiscard]]
        virtual auto kill(const ContainerID &id, const Signal &signal) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto kill(const ContainerID &id, const Signal &signal,
                          const KillOption &option) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;

        [[nodiscard]]
        virtual auto delete_(const ContainerID &id) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;
        [[nodiscard]]
        virtual auto delete_(const ContainerID &id,
                             const DeleteOption &option) noexcept
                -> tl::expected<void, std::exception_ptr> = 0;
};

}
