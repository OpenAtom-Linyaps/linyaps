/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/util/oci/distribution_client.h"
#include "object.h"

#include <qdbusargument.h>
#include <qjsondocument.h>
#include <qjsonobject.h>

const char *kTestOciDistributionManifestListJsonString = R"({
   "schemaVersion": 2,
   "mediaType": "application/vnd.docker.distribution.manifest.list.v2+json",
   "manifests": [
      {
         "mediaType": "application/vnd.docker.distribution.manifest.v2+json",
         "size": 1569,
         "digest": "sha256:668c78547b869678d6be5ba9121b95975eab6a2db78dd722c8fba346d15ed723",
         "platform": {
            "architecture": "amd64",
            "os": "linux"
         }
      },
      {
         "mediaType": "application/vnd.docker.distribution.manifest.v2+json",
         "size": 1569,
         "digest": "sha256:78f2e9d57e10cba0a3ed764c57592d5596c9c36446239975f39350507d02e1a4",
         "platform": {
            "architecture": "arm64",
            "os": "linux",
            "variant": "v8"
         }
      }
   ]
})";

TEST(QSerializer, ManifestV1)
{
    auto sourceJson = R"({
    "schemaVersion": 1,
    "name": "nginx",
    "tag": "mainline-alpine3.17-slim",
    "architecture": "amd64",
    "fsLayers": [
        {
        "blobSum": "sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4"
        },
        {
        "blobSum": "sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4"
        }
    ]
})";

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(sourceJson, &err);
    ASSERT_EQ(err.error, QJsonParseError::NoError);

    QVariant v = doc.object().toVariantMap();
    auto obj = v.value<QSharedPointer<linglong::oci::OciDistributionClientManifest>>();

    // NOTE: would not support v1 manifest
    //    ASSERT_NE(obj, nullptr);
    //    ASSERT_EQ(obj->name, "nginx");
    //    ASSERT_EQ(obj->fsLayers.length(), 2);
    //    ASSERT_EQ(obj->fsLayers[0]->blobSum,
    //    "sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4");
}

TEST(QSerializer, ManifestList)
{
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(kTestOciDistributionManifestListJsonString, &err);
    ASSERT_EQ(err.error, QJsonParseError::NoError);

    QVariant v = doc.object().toVariantMap();
    auto obj = v.value<QSharedPointer<linglong::oci::OciDistributionClientManifest>>();
}

TEST(QSerializer, ManifestV2)
{
    auto sourceJson = R"(
{
    "schemaVersion": 2,
    "mediaType": "application/vnd.docker.distribution.manifest.v2+json",
    "config": {
        "mediaType": "application/vnd.docker.container.image.v1+json",
        "digest": "sha256:b5b2b2c507a0944348e0303114d8d93aaaa081732b86451d9bce1f432a537bc7",
        "size": 7023
    },
    "layers": [
        {
            "mediaType": "application/vnd.docker.image.rootfs.diff.tar.gzip",
            "digest": "sha256:e692418e4cbaf90ca69d05a66403747baa33ee08806650b51fab815ad7fc331f",
            "size": 32654
        },
        {
            "mediaType": "application/vnd.docker.image.rootfs.diff.tar.gzip",
            "digest": "sha256:3c3a4604a545cdc127456d94e421cd355bca5b528f4a9c1905b15da2eb4a4c6b",
            "size": 16724
        },
        {
            "mediaType": "application/vnd.docker.image.rootfs.diff.tar.gzip",
            "digest": "sha256:ec4b8955958665577945c89419d1af06b5f7636b4ac3da7f12184802ad867736",
            "size": 73109
        }
    ]
})";

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(sourceJson, &err);
    ASSERT_EQ(err.error, QJsonParseError::NoError);

    QVariant v = doc.object().toVariantMap();
    auto obj = v.value<QSharedPointer<linglong::oci::OciDistributionClientManifest>>();
}
