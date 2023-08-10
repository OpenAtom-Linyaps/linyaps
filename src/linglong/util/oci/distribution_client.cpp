/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "distribution_client.h"

#include "linglong/util/file.h"
#include "linglong/util/http/http_client.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/util/sysinfo.h"

#include <QUrlQuery>

namespace linglong::oci {

QSERIALIZER_IMPL(OciDistributionClientManifestItemPlatform);
QSERIALIZER_IMPL(OciDistributionClientManifestListItem);
QSERIALIZER_IMPL(OciDistributionClientManifestList);
QSERIALIZER_IMPL(OciDistributionClientManifestFsLayer);
QSERIALIZER_IMPL(OciDistributionClientManifestLayer);
QSERIALIZER_IMPL(OciDistributionClientManifest);

static auto kSupportManifestVersion = 2;

/*!
 * package::Ref to oci image name and reference
 * for more repo/channel:id/version/arch/module:
 *  name => Ref.id
 *  FIXME: give an encode/decode algorithm
 *  reference = Ref.channel_Ref.version_Ref.module
 * @param ref
 * @return
 */
static std::tuple<QString, QString> toOciNameReference(const package::Ref &ref)
{
    auto name = ref.appId;
    auto reference = QStringList{ ref.channel, ref.version, ref.module }.join("_");
    return { name, reference };
}

static const QString &toOciArchitecture(const QString &arch)
{
    // see https://doc.qt.io/qt-5/qsysinfo.html#currentCpuArchitecture for more.
    static QMap<QString, QString> archMap = {
        { "arm64", "arm64" },
        { "x86_64", "amd64" },
    };

    return archMap[arch];
}

QString digestFile(QIODevice &device)
{
    if (!device.isOpen()) {
        device.open(QIODevice::ReadOnly);
    }
    device.seek(0);
    auto sha256 = util::fileHash(device, QCryptographicHash::Sha256);
    return QString("sha256:%1").arg(sha256);
}

QString digestContent(const QByteArray &data)
{
    return QString("sha256:%1")
            .arg(QString(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()));
}

/*!
 * PUT /v2/<name>/manifests/<reference>
 * https://github.com/opencontainers/distribution-spec/blob/v1.0.1/spec.md#pulling-manifests
 * https://github.com/distribution/distribution/blob/release/2.8/docs/spec/api.md#pushing-an-image-manifest
 * @tparam Content
 * @param endpoint
 * @param name
 * @param reference
 * @param contentType
 * @param object
 * @return
 */
template<typename Content>
std::tuple<QString, quint64, util::Error> putManifest(const QString &endpoint,
                                                      const QString &name,
                                                      const QString &reference,
                                                      const QString &contentType,
                                                      QSharedPointer<Content> object)
{
    auto url = QString("%1/v2/%2/manifests/%3").arg(endpoint, name, reference);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType.toUtf8());
    auto [content, err] = util::toJSON(object);
    qDebug().noquote() << "put manifest" << content;

    util::HttpRestClient httpClient;
    auto reply = httpClient.put(request, content);
    return { digestContent(content), content.size(), WarpNetworkError(reply) };
}

/*!
 * GET /v2/<name>/manifests/<reference>
 * https://github.com/opencontainers/distribution-spec/blob/v1.0.1/spec.md#pulling-manifests
 * https://github.com/distribution/distribution/blob/release/2.8/docs/spec/api.md#pulling-an-image-manifest
 * @tparam Ret
 * @param endpoint
 * @param name
 * @param reference
 * @param contentType
 * @return
 */
template<typename Ret>
std::tuple<QSharedPointer<Ret>, util::Error> pullManifest(const QString &endpoint,
                                                          const QString &name,
                                                          const QString &reference,
                                                          const QString &contentType)
{
    auto url = QString("%1/v2/%2/manifests/%3").arg(endpoint, name, reference);

    QNetworkRequest request(url);
    util::HttpRestClient httpClient;
    request.setRawHeader("Accept", contentType.toUtf8());
    auto reply = httpClient.get(request);
    if (reply->error()) {
        return { nullptr, WarpNetworkError(reply) };
    }
    auto data = reply->readAll();
    qDebug().noquote() << "get manifest:" << data;
    return { util::loadJsonBytes<Ret>(data), Success() };
}

/*!
 * Starting an upload request
 * POST /v2/<name>/blobs/uploads/
 * https://github.com/distribution/distribution/blob/release/2.8/docs/spec/api.md#starting-an-upload
 * @param endpoint
 * @param name
 * @return
 */
std::tuple<QString, QString, util::Error> createUpload(const QString &endpoint, const QString &name)
{
    auto url = QString("%1/v2/%2/blobs/uploads/").arg(endpoint, name);

    QNetworkRequest request(url);
    util::HttpRestClient httpClient;
    auto reply = httpClient.post(request, nullptr);
    if (reply->error()) {
        return { "", "", NewNetworkError(reply) };
    }

    // 202 Accepted
    // Location: /v2/<name>/blobs/uploads/<uuid>
    // Range: bytes=0-<offset>
    // Content-Length: 0
    // Docker-Upload-UUID: <uuid>
    auto location = reply->header(QNetworkRequest::LocationHeader).toString();
    auto uuid = QString(reply->rawHeader("Docker-Upload-UUID"));

    return { location, uuid, Success() };
}

/*!
 * HEAD /v2/<name>/blobs/<digest>
 * https://github.com/distribution/distribution/blob/release/2.8/docs/spec/api.md#existing-layers
 * @param uploadUrl is generated by createUpload
 * @param device
 * @return
 */
std::tuple<bool, util::Error> blobExist(const QString &endpoint,
                                        const QString &name,
                                        const QString &digest)
{

    auto url = QString("%1/v2/%2/blobs/%3").arg(endpoint, name, digest);

    QNetworkRequest request(url);
    util::HttpRestClient httpClient;
    auto reply = httpClient.head(request);
    if (reply->error()) {
        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            return { false, Success() };
        }
        return { false, NewNetworkError(reply) };
    }

    return { true, Success() };
}

/*!
 * PUT /v2/<name>/blobs/uploads/<uuid>?digest=<digest>
 * https://github.com/distribution/distribution/blob/release/2.8/docs/spec/api.md#monolithic-upload
 * @param uploadUrl is generated by createUpload
 * @param device
 * @return
 */
std::tuple<QSharedPointer<OciDistributionClientManifestLayer>, util::Error> monolithicUpload(
        const QString &endpoint, const QString &name, const QString &uploadUrl, QIODevice &device)
{
    QSharedPointer<OciDistributionClientManifestLayer> layer(
            new OciDistributionClientManifestLayer);
    layer->mediaType = kMediaTypeBlobErofs;
    layer->digest = digestFile(device);
    layer->size = device.size();

    auto [exist, err] = blobExist(endpoint, name, layer->digest);
    if (err) {
        return { nullptr, WrapError(err, "check blob exist failed") };
    }
    if (exist) {
        qDebug() << "layer" << name << layer->digest << "exist";
        return { layer, Success() };
    }

    QUrl url(uploadUrl);
    QUrlQuery query(url);
    query.addQueryItem("digest", layer->digest);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      util::HttpRestClient::kContentTypeBinaryStream);

    util::HttpRestClient httpClient;

    device.seek(0);
    qDebug() << "start push blob" << url;
    auto reply = httpClient.put(request, &device);
    qDebug() << "finish push blob" << reply->error() << reply->errorString() << reply->readAll()
             << reply->request().header(QNetworkRequest::ContentLengthHeader);

    return { layer, WarpNetworkError(reply) };
}

/*!
 * GET GET /v2/<name>/blobs/<digest>
 * https://github.com/opencontainers/distribution-spec/blob/v1.0.1/spec.md#pulling-blobs
 * https://github.com/distribution/distribution/blob/release/2.8/docs/spec/api.md#pulling-a-layer
 * FIXME: memory issue
 * TODO: support cache and range
 * @param endpoint
 * @param name
 * @param digest
 * @return
 */
std::tuple<QByteArray, util::Error> pullBlob(const QString &endpoint,
                                             const QString &name,
                                             const QString &digest)
{
    auto url = QString("%1/v2/%2/blobs/%3").arg(endpoint, name, digest);

    QNetworkRequest request(url);
    util::HttpRestClient httpClient;
    auto reply = httpClient.get(request);
    if (reply->error()) {
        return { "", NewNetworkError(reply) };
    }

    auto data = reply->readAll();
    return { data, Success() };
}

/*!
 * pull ref form oci distribution
 * @param ref
 * @return
 */
util::Error OciDistributionClient::pull(const package::Ref &ref)
{
    auto [name, reference] = toOciNameReference(ref);

    auto [manifestList, err] =
            pullManifest<OciDistributionClientManifestList>(endpoint,
                                                            name,
                                                            reference,
                                                            kMediaTypeManifestList);
    if (err) {
        return WrapError(err, "pull manifest list failed");
    }

    // get matched arch digest from manifest list
    auto digest = reference;
    for (auto const &manifest : manifestList->manifests) {
        if (manifest->platform->architecture == toOciArchitecture(ref.arch)) {
            digest = manifest->digest;
        }
    }

    QSharedPointer<OciDistributionClientManifest> manifest;
    std::tie(manifest, err) =
            pullManifest<OciDistributionClientManifest>(endpoint, name, digest, kMediaTypeManifest);
    if (err) {
        return WrapError(err, "pull arch manifest failed");
    }

    QByteArray blob;
    for (const auto &layer : manifest->layers) {
        std::tie(blob, err) = pullBlob(endpoint, name, layer->digest);
        if (err) {
            return WrapError(err, QString("pull blob %1 failed").arg(layer->digest));
        }
        qDebug() << "pull blob" << layer->digest << "size:" << blob.size();
    }

    return {};
}

std::tuple<QSharedPointer<OciDistributionClientManifestLayer>, util::Error>
OciDistributionClient::pushBlob(const package::Ref &ref, QIODevice &device)
{
    auto [name, _] = toOciNameReference(ref);
    auto [location, uuid, err] = createUpload(endpoint, ref.appId);
    if (err) {
        return { nullptr, WrapError(err, "createUpload fail") };
    }
    qDebug() << "createUpload" << location << uuid;
    return monolithicUpload(endpoint, name, location, device);
}

util::Error OciDistributionClient::putManifest(
        const package::Ref &ref, QSharedPointer<OciDistributionClientManifest> manifest)
{

    auto [name, reference] = toOciNameReference(ref);

    // FIXME: use default value;
    // update arch manifest
    manifest->schemaVersion = kSupportManifestVersion;
    manifest->mediaType = kMediaTypeManifest;

    auto [digest, size, err] =
            oci::putManifest(endpoint, name, reference, kMediaTypeManifest, manifest);

    QSharedPointer<OciDistributionClientManifestItemPlatform> platform(
            new OciDistributionClientManifestItemPlatform);
    platform->architecture = toOciArchitecture(util::hostArch());
    // only support linux os now
    platform->os = "linux";

    QSharedPointer<OciDistributionClientManifestListItem> item(
            new OciDistributionClientManifestListItem);

    item->mediaType = kMediaTypeManifest;
    item->digest = digest;
    item->size = size;
    item->platform = platform;

    // FIXME: update all arch manifest list?
    QSharedPointer<OciDistributionClientManifestList> manifestList(
            new OciDistributionClientManifestList);
    manifestList->mediaType = kMediaTypeManifestList;
    manifestList->schemaVersion = kSupportManifestVersion;
    manifestList->manifests.push_back(item);

    return std::get<2>(
            oci::putManifest(endpoint, name, reference, kMediaTypeManifestList, manifestList));
}

} // namespace linglong::oci
