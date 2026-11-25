// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "configure.h"
#include "linglong/cdi_drv/refresh.h"
#include "linglong/common/dir.h"
#include "linglong/utils/log/log.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <chrono>
#include <cstdlib>
#include <string>

namespace {

constexpr auto defaultConfigDir = LINGLONG_SYSCONFDIR "/cdi-drv";
constexpr auto defaultRuntimeDir = linglong::common::dir::cdiDriverRuntimeDir;

} // namespace

int main(int argc, char *argv[])
{
    CLI::App app{ "Manage CDI driver configurations", "ll-cdi-drv" };
    app.require_subcommand(1, 1);
    app.set_version_flag("--version", [] {
        return fmt::format("ll-cdi-drv {}", LINGLONG_VERSION);
    });

    std::string configDir{ defaultConfigDir };
    std::string runtimeDir{ defaultRuntimeDir };
    std::string name;
    std::string requestName;
    std::string unitDir{ "/run/systemd/system" };
    std::string systemctl{ "systemctl" };
    unsigned int idleTimeout{ 5 };
    const CLI::Validator validateName{
        [](std::string &value) -> std::string {
            if (!linglong::cdi_drv::validConfigName(value)) {
                return "Expected a configuration name containing only ASCII letters, digits, '.', "
                       "'_', or '-', without a leading dot";
            }
            return {};
        },
        "CONFIG_NAME"
    };

    auto *refresh = app.add_subcommand("refresh", "Regenerate and publish CDI driver files");
    auto *nameOption =
      refresh->add_option("name", name, "Configuration name without .json (omit to refresh all)")
        ->check(validateName);
    auto *request = refresh->add_option("--request", requestName, "Queue one configuration refresh")
                      ->check(validateName);
    auto *requestAll = refresh->add_flag("--request-all", "Queue a full refresh");
    requestAll->excludes(request, nameOption);
    request->excludes(nameOption);
    refresh->add_option("--config-dir", configDir, "CDI driver source configuration directory")
      ->capture_default_str()
      ->excludes(request, requestAll);
    refresh->add_option("--runtime-dir", runtimeDir, "CDI driver publication directory")
      ->capture_default_str();
    refresh->add_option("--unit-dir", unitDir, "Systemd runtime unit directory")
      ->capture_default_str();
    refresh->add_option("--systemctl", systemctl, "Systemctl executable")->capture_default_str();

    auto *pending =
      app.add_subcommand("refresh-pending", "Refresh queued configurations until idle");
    pending
      ->add_option("--idle-timeout", idleTimeout, "Idle timeout in seconds (0 exits when empty)")
      ->check(CLI::NonNegativeNumber)
      ->capture_default_str();
    pending->add_option("--config-dir", configDir, "CDI driver source configuration directory")
      ->capture_default_str();
    pending->add_option("--runtime-dir", runtimeDir, "CDI driver publication directory")
      ->capture_default_str();
    pending->add_option("--unit-dir", unitDir, "Systemd runtime unit directory")
      ->capture_default_str();
    pending->add_option("--systemctl", systemctl, "Systemctl executable")->capture_default_str();

    CLI11_PARSE(app, argc, argv);

    linglong::utils::log::setLogBackend(linglong::utils::log::LogBackend::Console);

    linglong::cdi_drv::RefreshOptions options{ configDir,
                                               runtimeDir,
                                               name,
                                               static_cast<bool>(*pending) };
    options.watches.unitDir = unitDir;
    options.watches.systemctl = systemctl;
    options.idleTimeout = std::chrono::seconds(idleTimeout);
    const auto result = *request || *requestAll
      ? linglong::cdi_drv::requestRefresh(runtimeDir, requestName)
      : linglong::cdi_drv::refresh(options);
    if (!result) {
        LogE("ll-cdi-drv: {}", result.error().message());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
