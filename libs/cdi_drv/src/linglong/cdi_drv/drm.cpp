// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "drm.h"

#include "configure.h"
#include "linglong/cdi_drv/types/DrmSelector.hpp"
#include "linglong/cdi_drv/types/Node.hpp"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"

#include <fmt/format.h>

#if HAVE_SYSTEMD
#  include <systemd/sd-device.h>
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

namespace linglong::cdi_drv {
namespace {

namespace fs = std::filesystem;
using utils::error::Result;

bool validPciId(std::string_view id)
{
    if (id.size() != 6 && id.size() != 9) {
        return false;
    }
    constexpr std::string_view hexDigits = "0123456789abcdefABCDEF";
    return id[4] == ':' && id.substr(0, 4).find_first_not_of(hexDigits) == std::string_view::npos
      && (id.substr(5) == "*"
          || (id.size() == 9
              && id.substr(5).find_first_not_of(hexDigits) == std::string_view::npos));
}

Result<void> validateSelector(const types::DrmSelector &selector)
{
    LINGLONG_TRACE("validate DRM selector");

    if (selector.pciIDS
        && !std::all_of(selector.pciIDS->begin(), selector.pciIDS->end(), validPciId)) {
        return LINGLONG_ERR("invalid PCI IDs in DRM selector");
    }
    if (selector.kernelDriver && selector.kernelDriver->empty()) {
        return LINGLONG_ERR("empty DRM kernelDriver");
    }
    return LINGLONG_OK;
}

std::optional<types::Node> nodeType(std::string_view name)
{
    const auto prefix =
      name.substr(0, 7) == "renderD" ? std::string_view("renderD") : std::string_view("card");
    if (name.substr(0, prefix.size()) != prefix || name.size() == prefix.size()
        || name.find_first_not_of("0123456789", prefix.size()) != std::string_view::npos) {
        return std::nullopt;
    }
    return prefix == "card" ? types::Node::Primary : types::Node::Render;
}

bool matchesPciIds(std::string_view vendor,
                   std::string_view device,
                   const std::vector<std::string> &ids)
{
    if (vendor.size() != 6 || device.size() != 6) {
        return false;
    }
    const std::string vendorId(vendor.substr(2));
    const std::string fullId = vendorId + ":" + std::string(device.substr(2));
    return std::any_of(ids.begin(), ids.end(), [&](std::string id) {
        std::transform(id.begin(), id.end(), id.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return id == fullId || id == vendorId + ":*";
    });
}

bool matchesPci(const fs::path &parent, const std::optional<std::vector<std::string>> &ids)
{
    if (!ids || ids->empty()) {
        return true;
    }
    std::error_code ec;
    const auto subsystem = fs::read_symlink(parent / "subsystem", ec);
    if (ec || subsystem.filename() != "pci") {
        return false;
    }
    std::string vendor;
    std::string device;
    std::ifstream vendorFile(parent / "vendor");
    std::ifstream deviceFile(parent / "device");
    return (vendorFile >> vendor) && (deviceFile >> device) && matchesPciIds(vendor, device, *ids);
}

Result<std::vector<fs::path>> enumerateDrmNodesSysfs(const fs::path &classDirectory)
{
    LINGLONG_TRACE("enumerate DRM devices using sysfs");

    std::error_code ec;
    fs::directory_iterator node(classDirectory, ec);
    if (ec == std::errc::no_such_file_or_directory) {
        return std::vector<fs::path>{};
    }
    if (ec) {
        return LINGLONG_ERR("enumerate DRM nodes", ec);
    }

    std::vector<fs::path> devices;
    for (; node != fs::directory_iterator(); node.increment(ec)) {
        devices.push_back(node->path());
    }
    if (ec) {
        return LINGLONG_ERR("enumerate DRM nodes", ec);
    }
    return devices;
}

#if HAVE_SYSTEMD
Result<std::vector<fs::path>> enumerateDrmNodesSystemd()
{
    LINGLONG_TRACE("enumerate DRM devices using systemd");

    sd_device_enumerator *enumerator = nullptr;
    const auto result = sd_device_enumerator_new(&enumerator);
    if (result < 0) {
        return LINGLONG_ERR("create DRM enumerator",
                            std::error_code(-result, std::generic_category()));
    }
    const auto unrefEnumerator = utils::finally::finally([enumerator] {
        sd_device_enumerator_unref(enumerator);
    });
    for (const auto status : { sd_device_enumerator_add_match_subsystem(enumerator, "drm", 1),
                               sd_device_enumerator_allow_uninitialized(enumerator) }) {
        if (status < 0) {
            return LINGLONG_ERR("configure DRM enumerator",
                                std::error_code(-status, std::generic_category()));
        }
    }

    std::vector<fs::path> devices;
    for (auto *node = sd_device_enumerator_get_device_first(enumerator); node != nullptr;
         node = sd_device_enumerator_get_device_next(enumerator)) {
        const char *path = nullptr;
        if (sd_device_get_syspath(node, &path) >= 0) {
            devices.emplace_back(path);
        }
    }
    return devices;
}
#endif

template <typename Enumerate>
Result<std::set<std::string>> selectDrmNodes(const types::DrmSelector &selector,
                                             Enumerate enumerate,
                                             const fs::path &deviceDirectory = "/dev/dri")
{
    LINGLONG_TRACE("select DRM device nodes");

    if (selector.nodes && selector.nodes->empty()) {
        return std::set<std::string>{};
    }
    auto validated = validateSelector(selector);
    if (!validated) {
        return LINGLONG_ERR(validated);
    }

    auto devices = enumerate();
    if (!devices) {
        return LINGLONG_ERR(devices);
    }

    std::set<std::string> paths;
    for (const auto &node : *devices) {
        const auto name = node.filename().string();
        const auto type = nodeType(name);
        if (!type
            || (selector.nodes
                && std::find(selector.nodes->begin(), selector.nodes->end(), *type)
                  == selector.nodes->end())) {
            continue;
        }
        std::error_code ec;
        const auto parent = node / "device";
        if (selector.kernelDriver) {
            const auto driver = fs::read_symlink(parent / "driver", ec);
            if (ec || driver.filename() != *selector.kernelDriver) {
                continue;
            }
        }
        if (!matchesPci(parent, selector.pciIDS)) {
            continue;
        }
        const auto devname = deviceDirectory / name;
        const auto status = fs::status(devname, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return LINGLONG_ERR(fmt::format("check DRM device {}", devname), ec);
        }
        if (fs::is_character_file(status)) {
            paths.emplace(devname.string());
        }
    }
    return paths;
}

} // namespace

utils::error::Result<std::set<std::string>> drmNodesSysfs(const types::DrmSelector &selector,
                                                          const fs::path &classDirectory,
                                                          const fs::path &deviceDirectory)
{
    return selectDrmNodes(
      selector,
      [&] {
          return enumerateDrmNodesSysfs(classDirectory);
      },
      deviceDirectory);
}

utils::error::Result<std::set<std::string>> drmNodes(const types::DrmSelector &selector)
{
#if HAVE_SYSTEMD
    return selectDrmNodes(selector, enumerateDrmNodesSystemd);
#else
    return drmNodesSysfs(selector);
#endif
}

} // namespace linglong::cdi_drv
