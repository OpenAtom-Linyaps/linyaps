/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_OCI_DISTRIBUTION_CLIENT_H_
#define LINGLONG_SRC_MODULE_UTIL_OCI_DISTRIBUTION_CLIENT_H_

#include "linglong/package/info.h"
#include "linglong/package/ref.h"
#include "linglong/util/error.h"

#include <tuple>

namespace linglong::oci {

inline const char *kMediaTypeManifestList =
  "application/vnd.docker.distribution.manifest.list.v2+json";
inline const char *kMediaTypeManifest = "application/vnd.docker.distribution.manifest.v2+json";
inline const char *kMediaTypeImageConfigV1 = "application/vnd.linglong.container.image.v1+json";
// erofs image type for linglong
inline const char *kMediaTypeBlobErofs = "application/vnd.linglong.image.rootfs.erofs";

class OciDistributionClientManifestItemPlatform : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(OciDistributionClientManifestItemPlatform)
    Q_SERIALIZE_PROPERTY(QString, architecture);
    Q_SERIALIZE_PROPERTY(QString, os);
    Q_SERIALIZE_PROPERTY(QString, variant);
    Q_SERIALIZE_PROPERTY(QStringList, features);
};

class OciDistributionClientManifestListItem : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(OciDistributionClientManifestListItem)
    Q_SERIALIZE_PROPERTY(QString, mediaType);
    Q_SERIALIZE_PROPERTY(QString, digest);
    Q_SERIALIZE_PROPERTY(quint64, size);
    Q_JSON_PTR_PROPERTY(OciDistributionClientManifestItemPlatform, platform);
};

class OciDistributionClientManifestList : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(OciDistributionClientManifestList)
    Q_SERIALIZE_PROPERTY(quint64, schemaVersion);
    Q_SERIALIZE_PROPERTY(QString, mediaType);
    Q_SERIALIZE_PROPERTY(QList<QSharedPointer<OciDistributionClientManifestListItem>>, manifests);
};

class OciDistributionClientManifestFsLayer : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(OciDistributionClientManifestFsLayer)
    Q_SERIALIZE_PROPERTY(QString, blobSum);
};

class OciDistributionClientManifestLayer : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(OciDistributionClientManifestLayer)
    Q_SERIALIZE_PROPERTY(QString, mediaType);
    Q_SERIALIZE_PROPERTY(QString, digest);
    Q_SERIALIZE_PROPERTY(quint64, size);
};

class OciDistributionClientManifest : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(OciDistributionClientManifest)
    Q_SERIALIZE_PROPERTY(quint64, schemaVersion);
    Q_SERIALIZE_PROPERTY(QString, mediaType);
    Q_SERIALIZE_PROPERTY(QSharedPointer<OciDistributionClientManifestLayer>, config);
    Q_SERIALIZE_PROPERTY(QList<QSharedPointer<OciDistributionClientManifestLayer>>, layers);

    //    application/vnd.docker.distribution.manifest.v1+json not support now
    //    Q_SERIALIZE_PROPERTY(QString, name);
    //    Q_SERIALIZE_PROPERTY(QString, tag);
    //    Q_SERIALIZE_PROPERTY(QString, architecture);
    //    Q_SERIALIZE_PROPERTY(QList<QSharedPointer<OciDistributionClientManifestFsLayer>>,
    //    fsLayers);
};

/*!
 * Support V2 api and V2 manifest only
 */
class OciDistributionClient
{
public:
    explicit OciDistributionClient(const QString &endpoint)
        : endpoint(endpoint)
    {
    }

    util::Error pull(const package::Ref &ref);

    util::Error putManifest(const package::Ref &ref,
                            QSharedPointer<OciDistributionClientManifest> manifest);

    std::tuple<QSharedPointer<OciDistributionClientManifestLayer>, util::Error>
    pushBlob(const package::Ref &ref, QIODevice &device);

private:
    QString endpoint;
};

QSERIALIZER_DECLARE(OciDistributionClientManifestItemPlatform);
QSERIALIZER_DECLARE(OciDistributionClientManifestListItem);
QSERIALIZER_DECLARE(OciDistributionClientManifestList);
QSERIALIZER_DECLARE(OciDistributionClientManifestFsLayer);
QSERIALIZER_DECLARE(OciDistributionClientManifestLayer);
QSERIALIZER_DECLARE(OciDistributionClientManifest);

} // namespace linglong::oci

#endif // LINGLONG_SRC_MODULE_UTIL_OCI_DISTRIBUTION_CLIENT_H_
