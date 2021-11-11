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

namespace linglong {
namespace package {

// FIXME: there is some problem that in module/util/runner.h, replace later
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

class BundlePrivate
{
public:
    //Bundle *q_ptr = nullptr;
    QString bundleFilePath;
    QString squashfsFilePath;
    QString bundleDataPath;
    const QString linglongLoader = "/usr/libexec/linglong-loader";

    explicit BundlePrivate(Bundle *parent)
    //    : q_ptr(parent)
    {
    }

    util::Result make(const QString &dataPath, const QString &outputFilePath)
    {
        //获取存储文件父目录路径
        QString bundleFileDirPath = QFileInfo(outputFilePath).path();
        //创建目录
        util::createDir(bundleFileDirPath);
        //赋值bundleFilePath
        this->bundleFilePath = outputFilePath;
        //数据目录路径赋值
        this->bundleDataPath = QDir(dataPath).absolutePath();

        //判断数据目录是否存在
        if (!util::dirExists(this->bundleDataPath)) {
            return dResultBase() << RetCode(RetCode::DataDirNotExists) << this->bundleDataPath + " don't exists!";
        }
        //判断uap.json是否存在
        if (!util::fileExists(this->bundleDataPath + QString(CONFIGJSON))) {
            return dResultBase() << RetCode(RetCode::UapJsonFileNotExists)
                                 << this->bundleDataPath + QString("/uap.json don't exists!!!");
        }

        //初始化uap.json
        Package package;
        if (!package.initConfig(this->bundleDataPath + "/uap.json")) {
            return dResultBase() << RetCode(RetCode::UapJsonFormatError)
                                 << this->bundleDataPath + QString(CONFIGJSON) + QString(" file format error!!!");
        }

        //赋值squashfsFilePath
        QString squashfsName = package.uap->getSquashfsName();
        this->squashfsFilePath = bundleFileDirPath + "/" + squashfsName;

        //清理squashfs文件
        if (util::fileExists(this->squashfsFilePath)) {
            QFile::remove(this->squashfsFilePath);
        }

        //清理bundle文件
        if (util::fileExists(this->bundleFilePath)) {
            QFile::remove(this->squashfsFilePath);
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
};

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

} // namespace package
} // namespace linglong
