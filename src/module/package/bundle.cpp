/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bundle.h"
#include "bundle_p.h"

#include <curl/curl.h>

namespace linglong {
namespace package {

linglong::util::Error runner(const QString &program, const QStringList &args, int timeout)
{
    QProcess process;
    process.setProgram(program);

    process.setArguments(args);

    QProcess::connect(&process, &QProcess::readyReadStandardOutput,
                      [&]() { std::cout << process.readAllStandardOutput().toStdString().c_str(); });

    QProcess::connect(&process, &QProcess::readyReadStandardError,
                      [&]() { std::cout << process.readAllStandardError().toStdString().c_str(); });

    process.start();
    process.waitForStarted(timeout);
    process.waitForFinished(timeout);

    return NewError() << process.exitCode() << process.errorString();
}

Bundle::Bundle(QObject *parent)
    : dd_ptr(new BundlePrivate(this))
{
}

Bundle::~Bundle()
{
}

linglong::util::Error Bundle::load(const QString &path)
{
    return NoError();
}
linglong::util::Error Bundle::save(const QString &path)
{
    return NoError();
}

linglong::util::Error Bundle::make(const QString &dataPath, const QString &outputFilePath)
{
    Q_D(Bundle);
    auto ret = d->make(dataPath, outputFilePath);
    if (!ret.success()) {
        return NewError(ret);
    }
    return NewError();
}

linglong::util::Error Bundle::push(const QString &bundleFilePath, const QString &repoUrl, const QString &repoChannel,
                                   bool force)
{
    Q_D(Bundle);
    auto ret = d->push(bundleFilePath, repoUrl, repoChannel, force);
    if (!ret.success()) {
        return NewError(ret, -1);
    }
    return NoError();
}

linglong::util::Error BundlePrivate::make(const QString &dataPath, const QString &outputFilePath)
{
    // 获取存储文件父目录路径
    QString bundleFileDirPath;
    if (outputFilePath.isEmpty()) {
        bundleFileDirPath = QDir("./").absolutePath();
    } else {
        bundleFileDirPath = QFileInfo(outputFilePath).path();
    }
    // 创建目录
    util::createDir(bundleFileDirPath);

    // 数据目录路径赋值
    this->bundleDataPath = QDir(dataPath).absolutePath();

    // 判断数据目录是否存在
    if (!util::dirExists(this->bundleDataPath)) {
        return NewError() << STATUS_CODE(kDataDirNotExists) << this->bundleDataPath + " don't exists!";
    }
    // 判断info.json是否存在
    if (!util::fileExists(this->bundleDataPath + QString(configJson))) {
        return NewError() << STATUS_CODE(kUapJsonFileNotExists)
                          << this->bundleDataPath + QString("/info.json don't exists!!!");
    }

    // 转换info.json为Info对象
    QScopedPointer<package::Info> info(util::loadJSON<package::Info>(this->bundleDataPath + QString(configJson)));

    // 获取编译机器架构
    this->buildArch = QSysInfo::buildCpuArchitecture();

    // 判断架构是否支持
    if (!info->arch.contains(this->buildArch)) {
        return NewError() << info->appid + QString(" : ") + this->buildArch + QString(" don't support!");
    }

    // 赋值bundleFilePath
    if (outputFilePath.isEmpty()) {
        this->bundleFilePath =
            bundleFileDirPath + "/" + info->appid + "_" + info->version + "_" + this->buildArch + ".uab";
    } else {
        this->bundleFilePath = outputFilePath;
    }

    // 赋值squashfsFilePath
    QString erofsName = (QStringList {info->appid, info->version, this->buildArch}.join("_")) + QString(".erofs");
    this->erofsFilePath = bundleFileDirPath + "/" + erofsName;

    // 清理erofs文件
    if (util::fileExists(this->erofsFilePath)) {
        QFile::remove(this->erofsFilePath);
    }

    // 清理bundle文件
    if (util::fileExists(this->bundleFilePath)) {
        QFile::remove(this->bundleFilePath);
    }

    // 制作squashfs文件
    auto resultMkfs = runner("mkfs.erofs", {"-zlz4hc,9", this->erofsFilePath, this->bundleDataPath}, 15 * 60 * 1000);
    if (!resultMkfs.success()) {
        return NewError(resultMkfs) << "call mkfs.erofs failed";
    }

    // 生产bundle文件
    auto resultObjcopy = runner("objcopy", {"--add-section", QStringList {".bundle=", this->erofsFilePath}.join(""),
                                            "--set-section-flags", ".bundle=noload,readonly", this->linglongLoader,
                                            this->bundleFilePath});
    if (!resultObjcopy.success()) {
        return NewError(resultObjcopy) << "call objcopy failed";
    }

    // 清理squashfs文件
    if (util::fileExists(this->erofsFilePath)) {
        QFile::remove(this->erofsFilePath);
    }

    // 设置执行权限
    QFile(this->bundleFilePath)
        .setPermissions(QFileDevice::ExeOwner | QFileDevice::WriteOwner | QFileDevice::ReadOwner);

    return NewError();
}

// read elf64
auto BundlePrivate::readElf64(FILE *fd, Elf64_Ehdr &ehdr) -> decltype(ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum))
{
    Elf64_Ehdr ehdr64;
    off_t ret = -1;

    fseeko(fd, 0, SEEK_SET);
    ret = fread(&ehdr64, 1, sizeof(ehdr64), fd);
    if (ret < 0 || (size_t)ret != sizeof(ehdr64)) {
        return -1;
    }

    ehdr.e_shoff = file64ToCpu<Elf64_Ehdr>(ehdr64.e_shoff, ehdr);
    ehdr.e_shentsize = file16ToCpu<Elf64_Ehdr>(ehdr64.e_shentsize, ehdr);
    ehdr.e_shnum = file16ToCpu<Elf64_Ehdr>(ehdr64.e_shnum, ehdr);

    return (ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum));
}

// get elf offset size
auto BundlePrivate::getElfSize(const QString elfFilePath) -> decltype(-1)
{
    FILE *fd = nullptr;
    off_t size = -1;
    Elf64_Ehdr ehdr;

    fd = fopen(elfFilePath.toStdString().c_str(), "rb");
    if (fd == nullptr) {
        return -1;
    }
    auto ret = fread(ehdr.e_ident, 1, EI_NIDENT, fd);
    if (ret != EI_NIDENT) {
        return -1;
    }
    if ((ehdr.e_ident[EI_DATA] != ELFDATA2LSB) && (ehdr.e_ident[EI_DATA] != ELFDATA2MSB)) {
        return -1;
    }
    if (ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
        // size = read_elf32(fd);
    } else if (ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
        size = readElf64(fd, ehdr);
    } else {
        return -1;
    }
    fclose(fd);
    return size;
}

linglong::util::Error BundlePrivate::push(const QString &bundleFilePath, const QString &repoUrl,
                                          const QString &repoChannel, bool force)
{
    auto userInfo = util::getUserInfo();

    // 从配置文件获取服务器域名url
    QString configUrl = repoUrl;
    if (configUrl.isEmpty()) {
        int statusCode = linglong::util::getLocalConfig("appDbUrl", configUrl);

        if (STATUS_CODE(kSuccess) != statusCode) {
            if (util::dirExists(this->tmpWorkDir)) {
                util::removeDir(this->tmpWorkDir);
            }
            return NewError() << "call getLocalConfig api failed";
        }
    }
    configUrl = configUrl.endsWith("/") ? configUrl : (configUrl + "/");
    auto token = G_HTTPCLIENT->getToken(configUrl, userInfo);

    if (token.isEmpty()) {
        qCritical() << "get token failed!";
        return NewError() << -1 << "get token failed!";
    }

    qInfo() << "start upload ...";
    // 判断uab文件是否存在
    if (!util::fileExists(bundleFilePath)) {
        return NewError() << STATUS_CODE(kBundleFileNotExists) << bundleFilePath + " don't exists!";
    }
    // 创建临时目录
    this->tmpWorkDir = util::ensureUserDir({".linglong", QFileInfo(bundleFilePath).fileName()});
    if (util::dirExists(this->tmpWorkDir)) {
        util::removeDir(this->tmpWorkDir);
    }
    util::createDir(this->tmpWorkDir);

    // 转换成绝对路径
    this->bundleFilePath = QFileInfo(bundleFilePath).absoluteFilePath();

    // 获取offset值
    this->offsetValue = getElfSize(this->bundleFilePath);

    // 导出squashfs文件
    this->erofsFilePath = this->tmpWorkDir + "/squashfsFile";
    QFile bundleFile(this->bundleFilePath);
    QFile squashfsFile(this->erofsFilePath);
    bundleFile.open(QIODevice::ReadOnly);
    bundleFile.seek(this->offsetValue);
    squashfsFile.open(QIODevice::WriteOnly);
    squashfsFile.write(bundleFile.readAll());

    bundleFile.close();
    squashfsFile.close();

    // 解压squashfs文件
    this->bundleDataPath = this->tmpWorkDir + "/unsquashfs";
    if (util::dirExists(this->bundleDataPath)) {
        util::removeDir(this->bundleDataPath);
    }
    auto resultUnsquashfs = runner("unsquashfs", {"-dest", this->bundleDataPath, "-f", this->erofsFilePath});

    if (!resultUnsquashfs.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return NewError(resultUnsquashfs) << "call unsquashfs failed";
    }

    // 转换info.json为Info对象
    QScopedPointer<package::Info> runtimeInfo(
        util::loadJSON<package::Info>(QStringList {this->bundleDataPath, "runtime", configJson}.join("/")));
    QScopedPointer<package::Info> develInfo(
        util::loadJSON<package::Info>(QStringList {this->bundleDataPath, "devel", configJson}.join("/")));

    // 建立临时仓库
    auto resultMakeRepo = runner("ostree", {"--repo=" + this->tmpWorkDir + "/repo", "init", "--mode=archive"}, 3000);
    if (!resultMakeRepo.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return NewError(resultMakeRepo) << "call ostree init failed";
    }

    // 推送数据到临时仓库
    // TODO: remove later
    QStringList arguments;
    arguments << QString("--repo=") + this->tmpWorkDir + "/repo" << QString("commit") << QString("-s")
              << QString("update ") + runtimeInfo->version << QString("--canonical-permissions") << QString("-m")
              << QString("Name: ") + runtimeInfo->appid << QString("-b")
              << (QStringList {repoChannel, runtimeInfo->appid, runtimeInfo->version, runtimeInfo->arch[0],
                               runtimeInfo->module}
                      .join(QDir::separator()))
              << QString("--tree=dir=") + this->bundleDataPath + "/runtime";

    QStringList commitArgs;
    commitArgs
        << QString("--repo=") + this->tmpWorkDir + "/repo" << QString("commit") << QString("-s")
        << QString("update ") + develInfo->version << QString("--canonical-permissions") << QString("-m")
        << QString("Name: ") + develInfo->appid << QString("-b")
        << (QStringList {repoChannel, develInfo->appid, develInfo->version, develInfo->arch[0], develInfo->module}.join(
               QDir::separator()))
        << QString("--tree=dir=") + this->bundleDataPath + "/devel";

    auto resultCommit = runner("ostree", arguments);
    resultCommit = runner("ostree", commitArgs);
    if (!resultCommit.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return NewError(resultCommit) << "call ostree commit failed";
    }

    // 压缩仓库为repo.tar
    arguments.clear();
    arguments << "-cvpf" << this->tmpWorkDir + "/repo.tar"
              << "-C" + this->tmpWorkDir << "repo";
    auto resultTar = runner("pwd", arguments);
    if (!resultTar.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return NewError(resultTar) << "call tar cvf failed";
    }

    // 上传repo.tar文件
    auto retUploadRepo = G_HTTPCLIENT->uploadFile(this->tmpWorkDir + "/repo.tar", configUrl, "ostree", token);
    if (STATUS_CODE(kSuccess) != retUploadRepo) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        std::cout << "upload repo.tar failed, please check and try again!" << std::endl;
        return NewError() << "upload repo.tar failed";
    }

    // 上传bundle文件
    auto retUploadBundle = G_HTTPCLIENT->uploadFile(this->bundleFilePath, configUrl, "bundle", token);
    if (STATUS_CODE(kSuccess) != retUploadBundle) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        std::cout << "Upload bundle failed, please check and try again!" << std::endl;
        return NewError() << "upload bundle failed";
    }

    // 上传bundle信息到服务器
    auto runtimeJson = QJsonDocument::fromJson(package::Info::dump(runtimeInfo.data()));
    auto develJson = QJsonDocument::fromJson(package::Info::dump(develInfo.data()));

    auto runtimeJsonObject = runtimeJson.object();
    auto develJsonObject = develJson.object();

    const QString runtimeAppId = runtimeJsonObject.value("appid").toString();
    const QString develAppId = develJsonObject.value("appid").toString();

    runtimeJsonObject.remove("appid");
    develJsonObject.remove("appid");

    runtimeJsonObject.insert("appId", runtimeAppId);
    develJsonObject.insert("appId", develAppId);

    runtimeJsonObject.insert("channel", repoChannel);
    develJsonObject.insert("channel", repoChannel);

    runtimeJsonObject["arch"] = runtimeInfo->arch[0];
    develJsonObject["arch"] = develInfo->arch[0];

    QJsonDocument runtimeDoc;
    QJsonDocument develDoc;
    runtimeDoc.setObject(runtimeJsonObject);
    develDoc.setObject(develJsonObject);

    auto runtimeRet = G_HTTPCLIENT->pushServerBundleData(runtimeDoc.toJson(), configUrl, token);
    auto develRet = G_HTTPCLIENT->pushServerBundleData(develDoc.toJson(), configUrl, token);
    if (STATUS_CODE(kSuccess) != runtimeRet || STATUS_CODE(kSuccess) != develRet) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        std::cout << "upload bundle info failed, please check and try again!" << std::endl;
        return NewError() << "upload bundle info failed";
    }

    if (util::dirExists(this->tmpWorkDir)) {
        util::removeDir(this->tmpWorkDir);
    }
    std::cout << "Upload success" << std::endl;
    return NewError();
}

} // namespace package
} // namespace linglong
