/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/util/oci/distribution_client.h"
#include "module/util/qserializer/json.h"
#include "module/util/sysinfo.h"
#include "module/util/test/tool.h"

using namespace linglong;

extern const char *kTestOciDistributionManifestListJsonString;

TEST(Module_Util_Oci, Distribution)
{
    if (!qEnvironmentVariableIsSet("LINGLONG_TEST_ALL")) {
        return;
    }

    auto testDataDir = qEnvironmentVariable("LINGLONG_TEST_DISTRIBUTION_DATADIR");

    test::runQApplication([&]() {
        util::Error err = Success();
        oci::OciDistributionClient ociClient(
                qEnvironmentVariable("LINGLONG_TEST_DISTRIBUTION_ENDPOINT",
                                     "http://127.0.0.1:5000"));

        auto ref = package::Ref("",
                                "main",
                                "org.deepin.music",
                                "7.0.2.67",
                                util::hostArch(),
                                "binary");
        auto runtimeRef = package::Ref("",
                                       "main",
                                       "org.deepin.runtime",
                                       "23.0.0.8",
                                       util::hostArch(),
                                       "binary");

        QSharedPointer<oci::OciDistributionClientManifestLayer> layer;
        QSharedPointer<oci::OciDistributionClientManifest> manifest(
                new oci::OciDistributionClientManifest);

        QFile appBlob(testDataDir + "/org.deepin.music_7.0.2.67_x86_64_binary.erofs");
        appBlob.open(QIODevice::ReadOnly);
        std::tie(layer, err) = ociClient.pushBlob(ref, appBlob);
        appBlob.close();
        ASSERT_EQ(err, Success());
        manifest->layers.push_back(layer);

        QFile runtimeBlob(testDataDir + "/org.deepin.runtime_23.0.0.8_x86_64_binary.erofs");
        runtimeBlob.open(QIODevice::ReadOnly);
        std::tie(layer, err) = ociClient.pushBlob(ref, runtimeBlob);
        runtimeBlob.close();
        ASSERT_EQ(err, Success());
        manifest->layers.push_back(layer);

        QSharedPointer<package::AppMetaInfo> config(new package::AppMetaInfo);

        QByteArray content;
        std::tie(content, err) = util::toJSON(config);

        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        buffer.write(content);
        buffer.seek(0);

        std::tie(layer, err) = ociClient.pushBlob(ref, buffer);
        buffer.close();
        ASSERT_EQ(err, Success());
        layer->mediaType = oci::kMediaTypeImageConfigV1;

        manifest->config = layer;
        manifest->config->mediaType = layer->mediaType;
        manifest->config->size = layer->size;
        manifest->config->digest = layer->digest;

        err = ociClient.putManifest(ref, manifest);
        ASSERT_EQ(err, Success());

        err = ociClient.pull(ref);
        ASSERT_EQ(err, Success());
    });
}
