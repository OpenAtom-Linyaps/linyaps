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

util::Result runner(const QString &program, const QStringList &args, int timeout)
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

    return dResultBase() << process.exitCode() << process.errorString();
}

Bundle::Bundle(QObject *parent)
    : dd_ptr(new BundlePrivate(this))
{
}

Bundle::~Bundle()
{
}

util::Result Bundle::load(const QString &path)
{
    return dResultBase();
}
util::Result Bundle::save(const QString &path)
{
    return dResultBase();
}

util::Result Bundle::make(const QString &dataPath, const QString &outputFilePath)
{
    Q_D(Bundle);
    auto ret = d->make(dataPath, outputFilePath);
    if (!ret.success()) {
        return dResult(ret);
    }
    return dResultBase();
}

util::Result Bundle::push(const QString &bundleFilePath, bool force)
{
    Q_D(Bundle);
    auto ret = d->push(bundleFilePath, force);
    if (!ret.success()) {
        return dResult(ret);
    }
    return dResultBase();
}

util::Result BundlePrivate::make(const QString &dataPath, const QString &outputFilePath)
{
    //获取存储文件父目录路径
    QString bundleFileDirPath;
    if (outputFilePath.isEmpty()) {
        bundleFileDirPath = QDir("./").absolutePath();
    } else {
        bundleFileDirPath = QFileInfo(outputFilePath).path();
    }
    //创建目录
    util::createDir(bundleFileDirPath);

    //数据目录路径赋值
    this->bundleDataPath = QDir(dataPath).absolutePath();

    //判断数据目录是否存在
    if (!util::dirExists(this->bundleDataPath)) {
        return dResultBase() << RetCode(RetCode::DataDirNotExists) << this->bundleDataPath + " don't exists!";
    }
    //判断info.json是否存在
    if (!util::fileExists(this->bundleDataPath + QString(configJson))) {
        return dResultBase() << RetCode(RetCode::UapJsonFileNotExists)
                             << this->bundleDataPath + QString("/info.json don't exists!!!");
    }

    //转换info.json为Info对象
    auto info = util::loadJSON<package::Info>(this->bundleDataPath + QString(configJson));

    //获取编译机器架构
    this->buildArch = QSysInfo::buildCpuArchitecture();

    //判断架构是否支持
    if (!info->arch.contains(this->buildArch)) {
        return dResultBase() << info->appid + QString(" : ") + this->buildArch + QString(" don't support!");
    }

    //赋值bundleFilePath
    if (outputFilePath.isEmpty()) {
        this->bundleFilePath =
            bundleFileDirPath + "/" + info->appid + "_" + info->version + "_" + this->buildArch + ".uab";
    } else {
        this->bundleFilePath = outputFilePath;
    }

    //赋值squashfsFilePath
    QString squashfsName = (QStringList {info->appid, info->version, this->buildArch}.join("_")) + QString(".squashfs");
    this->squashfsFilePath = bundleFileDirPath + "/" + squashfsName;

    //清理squashfs文件
    if (util::fileExists(this->squashfsFilePath)) {
        QFile::remove(this->squashfsFilePath);
    }

    //清理bundle文件
    if (util::fileExists(this->bundleFilePath)) {
        QFile::remove(this->bundleFilePath);
    }

    //制作squashfs文件
    auto resultMakeSquashfs =
        runner("mksquashfs", {this->bundleDataPath, this->squashfsFilePath, "-comp", "xz"}, 15 * 60 * 1000);
    if (!resultMakeSquashfs.success()) {
        return dResult(resultMakeSquashfs) << "call mksquashfs failed";
    }

    //生产bundle文件
    QFile outputFile(this->bundleFilePath);
    outputFile.open(QIODevice::Append);
    QFile linglongLoaderFile(this->linglongLoader);
    linglongLoaderFile.open(QIODevice::ReadOnly);
    QFile squashfsFile(this->squashfsFilePath);
    squashfsFile.open(QIODevice::ReadOnly);

    outputFile.write(linglongLoaderFile.readAll());
    outputFile.write(squashfsFile.readAll());

    linglongLoaderFile.close();
    squashfsFile.close();
    outputFile.close();

    //清理squashfs文件
    if (util::fileExists(this->squashfsFilePath)) {
        QFile::remove(this->squashfsFilePath);
    }

    //设置执行权限
    QFile(this->bundleFilePath)
        .setPermissions(QFileDevice::ExeOwner | QFileDevice::WriteOwner | QFileDevice::ReadOwner);

    return dResultBase();
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

util::Result BundlePrivate::push(const QString &bundleFilePath, bool force)
{
    //判断uab文件是否存在
    if (!util::fileExists(bundleFilePath)) {
        return dResultBase() << RetCode(RetCode::BundleFileNotExists) << bundleFilePath + " don't exists!";
    }
    //创建临时目录
    this->tmpWorkDir = util::ensureUserDir({".linglong", QFileInfo(bundleFilePath).fileName()});
    if (util::dirExists(this->tmpWorkDir)) {
        util::removeDir(this->tmpWorkDir);
    }
    util::createDir(this->tmpWorkDir);

    //转换成绝对路径
    this->bundleFilePath = QFileInfo(bundleFilePath).absoluteFilePath();

    //获取offset值
    this->offsetValue = getElfSize(this->bundleFilePath);

    //导出squashfs文件
    this->squashfsFilePath = this->tmpWorkDir + "/squashfsFile";
    // TODO:liujianqiang
    //后续改成QFile处理
    auto resultOutput = runner("dd",
                               {"if=" + this->bundleFilePath, "of=" + this->squashfsFilePath,
                                "bs=" + QString().setNum(this->offsetValue), "skip=1"},
                               15 * 60 * 1000);
    if (!resultOutput.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return dResult(resultOutput) << "call dd failed";
    }

    //解压squashfs文件
    this->bundleDataPath = this->tmpWorkDir + "/unsquashfs";
    if (util::dirExists(this->bundleDataPath)) {
        util::removeDir(this->bundleDataPath);
    }
    auto resultUnsquashfs = runner("unsquashfs", {"-dest", this->bundleDataPath, "-f", this->squashfsFilePath});

    if (!resultUnsquashfs.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return dResult(resultUnsquashfs) << "call unsquashfs failed";
    }

    //转换info.json为Info对象
    auto info = util::loadJSON<package::Info>(this->bundleDataPath + QString(configJson));

    //建立临时仓库
    auto resultMakeRepo = runner("ostree", {"--repo=" + this->tmpWorkDir + "/repo", "init", "--mode=archive"}, 3000);
    if (!resultMakeRepo.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return dResult(resultMakeRepo) << "call ostree init failed";
    }

    //推送数据到临时仓库
    QStringList arguments;
    arguments << QString("--repo=") + this->tmpWorkDir + "/repo" << QString("commit") << QString("-s")
              << QString("update ") + info->version << QString("-m") << QString("Name: ") + info->appid << QString("-b")
              << (QStringList {info->appid, info->version, info->arch[0]}.join(QDir::separator()))
              << QString("--tree=dir=") + this->bundleDataPath;

    auto resultCommit = runner("ostree", arguments);

    if (!resultCommit.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return dResult(resultCommit) << "call ostree commit failed";
    }

    //压缩仓库为repo.tar
    arguments.clear();
    arguments << "-cvpf" << this->tmpWorkDir + "/repo.tar"
              << "-C" + this->tmpWorkDir << "repo";
    auto resultTar = runner("tar", arguments);
    if (!resultTar.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return dResult(resultTar) << "call tar cvf failed";
    }

    //上传repo.tar文件
    // TODO:刘建强，后续整改为api调用
    arguments.clear();
    arguments << "-location"
              << "--request"
              << "POST"
              << "10.20.54.2:8888/linglong/app/upload"
              << "--form"
              << "file=@" + this->tmpWorkDir + "/repo.tar"
              << "--form"
              << "uploadSubPath=ostree";
    auto resultPushRepo = runner("curl", arguments);

    if (!resultPushRepo.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return dResult(resultPushRepo) << "call curl push repo.tar failed";
    }

    //上传bundle文件
    // TODO:刘建强，后续整改为api调用
    arguments.clear();
    arguments << "-location"
              << "--request"
              << "POST"
              << "10.20.54.2:8888/linglong/app/upload"
              << "--form"
              << "file=@" + this->bundleFilePath << "--form"
              << "uploadSubPath=bundle";

    auto resultPushBundle = runner("curl", arguments);

    if (!resultPushBundle.success()) {
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return dResult(resultPushBundle) << "call curl push bundle failed";
    }

    //上传bundle信息到服务器
    auto infoJson = QJsonDocument::fromJson(package::Info::dump(info));
    auto infoJsonObject = infoJson.object();
    const QString infoAppId = infoJsonObject.value("appid").toString();
    infoJsonObject.remove("appid");
    infoJsonObject.insert("appId", infoAppId);
    infoJsonObject["arch"] = info->arch[0];

    QJsonDocument doc;
    doc.setObject(infoJsonObject);

    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    QString retMsg = "";
    bool ret = httpClient->pushServerBundleData(doc.toJson(), retMsg);
    httpClient->release();
    if (!ret) {
        QString err = "pushServerBundleData err";
        qCritical() << err;
    }

    std::cout << doc.toJson().toStdString() << std::endl;
    QJsonObject retMesgJsonObject = QJsonDocument::fromJson(retMsg.toUtf8()).object();

    if (retMesgJsonObject["msg"].toString() != "操作成功") {
        std::cout << "Upload failed, please upload again！" << std::endl;
        if (util::dirExists(this->tmpWorkDir)) {
            util::removeDir(this->tmpWorkDir);
        }
        return dResultBase() << "Upload bundle failed";
    }
    std::cout << "Upload success" << std::endl;

    if (util::dirExists(this->tmpWorkDir)) {
        util::removeDir(this->tmpWorkDir);
    }
    return dResultBase();
}

} // namespace package
} // namespace linglong
