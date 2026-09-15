// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "device_detector.h"
#include "device_detector_registry.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

namespace {

using namespace linglong::ctk::detect;

class FakeDetector final : public DeviceDetector
{
public:
    FakeDetector(linglong::utils::error::Result<std::optional<std::string>> result)
        : result_(std::move(result))
    {
    }

    linglong::utils::error::Result<std::optional<std::string>> detect() override
    {
        return std::move(result_);
    }

private:
    linglong::utils::error::Result<std::optional<std::string>> result_;
};

} // namespace

TEST(DeviceDetectorRegistryTest, CollectsDevicesAndSkipsFailures)
{
    DeviceDetectorRegistry registry;
    registry.add(std::make_unique<FakeDetector>(std::string{ "nvidia" }));
    registry.add(std::make_unique<FakeDetector>(std::nullopt));
    registry.add(std::make_unique<FakeDetector>(
      tl::unexpected(linglong::utils::error::Error::Err(__FILE__, __LINE__, "test", "failed"))));
    registry.add(std::make_unique<FakeDetector>(std::string{ "amdgpu" }));

    auto detected = registry.detectDevices();

    EXPECT_EQ(detected, (std::set<std::string>{ "nvidia", "amdgpu" }));
}
