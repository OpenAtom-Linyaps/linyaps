#include "ocppi/cli/crun/Crun.hpp"

#ifdef OCPPI_WITH_SPDLOG
#include "spdlog/spdlog.h"
#endif

namespace ocppi::cli::crun
{

#ifdef OCPPI_WITH_SPDLOG
auto Crun::New(const std::filesystem::path &bin) noexcept
        -> tl::expected<std::unique_ptr<Crun>, std::exception_ptr>
try {
        return std::unique_ptr<Crun>(new Crun(bin, spdlog::default_logger()));
} catch (...) {
        return tl::unexpected(std::current_exception());
}

auto Crun::New(const std::filesystem::path &bin,
               const std::shared_ptr<spdlog::logger> &logger) noexcept
        -> tl::expected<std::unique_ptr<Crun>, std::exception_ptr>
try {
        return std::unique_ptr<Crun>(new Crun(bin, logger));
} catch (...) {
        return tl::unexpected(std::current_exception());
}

#else

auto Crun::New(const std::filesystem::path &bin) noexcept
        -> tl::expected<std::unique_ptr<Crun>, std::exception_ptr>
try {
        return std::unique_ptr<Crun>(new Crun(bin));
} catch (...) {
        return tl::unexpected(std::current_exception());
}

#endif
}
