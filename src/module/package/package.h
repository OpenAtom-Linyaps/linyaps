/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>

#include <string>

#include <QDBusArgument>
#include <QObject>
#include <QList>
#include <QFile>
#include <QDebug>
#include <QTemporaryDir>
#include <QFileInfo>

#include "module/uab/uap.h"
#include "module/util/fs.h"
#include "module/util/repo.h"
#include "module/util/retmessage.h"
#include "service/impl/dbus_retcode.h"
#include "module/util/runner.h"

#define CONFIGJSON "/uap.json"
#define INFOJSON "/info.json"
#define ENTRIESDIR "/entries"
#define FILEDIR "/files"

using format::uap::UAP;
using linglong::dbus::RetCode;
using linglong::repo::commitOstree;
using linglong::repo::makeOstree;
using linglong::util::checkSign;
using linglong::util::copyDir;
using linglong::util::createDir;
using linglong::util::dirExists;
using linglong::util::extractUap;
using linglong::util::extractUapData;
using linglong::util::fileExists;
using linglong::util::makeData;
using linglong::util::makeSign;
using linglong::util::removeDir;
using linglong::util::linkFile;

class Uap_Archive
{
public:
    struct archive *wb = nullptr;
    struct archive_entry *entry = nullptr;
    struct archive *wa = nullptr;
    struct archive *ext = nullptr;
    struct stat workdir_st, st;
    bool write_new()
    {
        stat(".", &workdir_st);
        wb = archive_write_new();
        if (wb == nullptr) {
            return false;
        }
        return true;
    }
    int write_open_filename(string filename)
    {
        archive_write_add_filter_none(wb);
        archive_write_set_format_pax_restricted(wb);
        archive_write_open_filename(wb, filename.c_str());
        return 0;
    }
    bool add_file(string uap_buffer, string metaname)
    {
        entry = archive_entry_new();
        if (entry == nullptr) {
            // #TODO
            return false;
        }
        archive_entry_copy_stat(entry, &workdir_st);
        archive_entry_set_pathname(entry, metaname.c_str());
        archive_entry_set_size(entry, uap_buffer.length());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(wb, entry);
        archive_write_data(wb, uap_buffer.c_str(), uap_buffer.length());
        archive_entry_free(entry);
        return true;
    }
    bool add_data_file(string dataPath, string dataname)
    {
        stat(dataPath.c_str(), &st);
        entry = archive_entry_new(); // Note 2
        if (entry == nullptr) {
            // #TODO
            return false;
        }
        archive_entry_copy_stat(entry, &st);
        archive_entry_set_pathname(entry, dataname.c_str());
        archive_write_header(wb, entry);
        char buff[40960] = {
            0,
        };
        int len = 0;
        int fd = -1;
        fd = open(dataPath.c_str(), O_RDONLY);
        len = read(fd, buff, sizeof(buff));
        while (len > 0) {
            archive_write_data(wb, buff, len);
            len = read(fd, buff, sizeof(buff));
        }
        close(fd);
        archive_entry_free(entry);
        return true;
    }
    bool write_free()
    {
        archive_write_close(wb); // Note 4
        archive_write_free(wb); // Note
        return true;
    }

    int copy_data(struct archive *ar, struct archive *aw)
    {
        int r;
        const void *buff;
        size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
        int64_t offset;
#else
        off_t offset;
#endif

        for (;;) {
            r = archive_read_data_block(ar, &buff, &size, &offset);
            if (r == ARCHIVE_EOF)
                return (ARCHIVE_OK);
            if (r != ARCHIVE_OK)
                return (r);
            r = archive_write_data_block(aw, buff, size, offset);
            if (r != ARCHIVE_OK) {
#ifdef DEBUG
                fprintf(stderr, "%s\n", archive_error_string(aw));
#endif
                return (r);
            }
            // fprintf(stdout, "%s\n", (char*)buff);
        }
    }

    bool extract_archive(const char *filename, const char *outdir)
    {
        int flags;
        int r;

        /* Select which attributes we want to restore. */
        flags = ARCHIVE_EXTRACT_TIME;
        flags |= ARCHIVE_EXTRACT_PERM;
        flags |= ARCHIVE_EXTRACT_ACL;
        flags |= ARCHIVE_EXTRACT_FFLAGS;

        wa = archive_read_new();
        archive_read_support_format_all(wa);
        archive_read_support_filter_all(wa);
        ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, flags);
        archive_write_disk_set_standard_lookup(ext);
        if ((r = archive_read_open_filename(wa, filename, 10240))) {
#ifdef DEBUG
            perror("error:");
#endif
            goto release;
            return false;
            // exit(1);
        }
        for (;;) {
            r = archive_read_next_header(wa, &entry);
            if (r == ARCHIVE_EOF)
                break;
            if (r < ARCHIVE_OK)
                fprintf(stderr, "%s\n", archive_error_string(wa));
            if (r < ARCHIVE_WARN) {
                // exit(1);
                goto release;
                return false;
            }

            const char *path = archive_entry_pathname(entry);
            char newPath[255 + 1];
            snprintf(newPath, 255, "%s/%s", outdir, path);
#ifdef DEBUG
            fprintf(stdout, "entry old path:%s, newPath:%s\n", path, newPath);
#endif
            archive_entry_set_pathname(entry, newPath);
            r = archive_write_header(ext, entry);
            if (r < ARCHIVE_OK) {
#ifdef DEBUG
                fprintf(stderr, "%s\n", archive_error_string(ext));
#endif
            } else if (archive_entry_size(entry) > 0) {
                r = copy_data(wa, ext);
                if (r < ARCHIVE_OK) {
#ifdef DEBUG
                    fprintf(stderr, "%s\n", archive_error_string(ext));
#endif
                }
                if (r < ARCHIVE_WARN) {
                    // exit(1);
                    goto release;
                    return false;
                }
            }
            r = archive_write_finish_entry(ext);
            if (r < ARCHIVE_OK) {
#ifdef DEBUG
                fprintf(stderr, "%s\n", archive_error_string(ext));
#endif
            }
            if (r < ARCHIVE_WARN) {
                // exit(1);
                goto release;
                return false;
            }
        }
        archive_read_close(wa);
        archive_read_free(wa);
        archive_write_close(ext);
        archive_write_free(ext);
        // exit(0);
        return true;
    release:
        if (NULL != wa) {
            archive_read_free(wa);
        }
        if (NULL != ext) {
            archive_write_free(ext);
        }
        return false;
    }
};

class Package
{
public:
    QString ID;
    QString name;
    QString configJson;
    QString dataDir;
    QString dataPath;

protected:
    UAP *uap;

public:
    Package() { this->uap = new UAP(); }
    ~Package()
    {
        if (this->uap) {
            delete this->uap;
        }
        if (this->dataPath != "" && fileExists(this->dataPath)) {
            QFile::remove(this->dataPath);
            if (dirExists(QFileInfo(this->dataPath).dir().path())) {
                //删除临时解压目录
                removeDir(QFileInfo(this->dataPath).dir().path());
            }
            this->dataPath = nullptr;
        }
    }
    bool InitUap(const QString &config, const QString &data = "")
    {
        if (!this->initConfig(config)) {
            qInfo() << "initconfig failed!!!";
            return false;
        }
        if (this->uap->isFullUab() && !data.isEmpty())
            this->initData(data);
        // this->initDataSing()
        // this->getSign()
        return true;
    }

    // TODO(RD): 创建package meta info
    bool initConfig(const QString config)
    {
        if (!fileExists(config)) {
            return false;
        }
        this->configJson = config;
        QFile jsonFile(this->configJson);
        jsonFile.open(QIODevice::ReadOnly);
        auto qbt = jsonFile.readAll();
        jsonFile.close();
        if (!uap->setFromJson(std::string(qbt.constData(), qbt.length()))) {
            return false;
        }
        return true;
    }

    // TODO(RD): 创建package的数据包
    bool initData(const QString data)
    {
        if (!dirExists(data)) {
            return false;
        }
        this->dataDir = data;

        // check entries desktop
        if (!dirExists(this->dataDir + QString("/entries"))) {
            // copy entries
            qInfo() << "need: entries of desktop file !";
            return false;
        }

        // check files list
        if (!dirExists(this->dataDir + QString("/files"))) {
            // copy files
            qInfo() << "need: entries of desktop file !";
            return false;
        }
        // check permission info.json
        if (!fileExists(this->dataDir + QString("/info.json"))) {
            // copy default permission info.json file to linglong
            qInfo() << "need: info.json of permission !";
            return false;
        }

        // make data.gz
        // tar -C /path/to/directory -cf - . | gzip --rsyncable >data.tgz
        // TODO:(fix) set temp directory
        makeData(this->dataDir, this->dataPath);
        return true;
    }

    // make uap
    bool MakeUap(QString uap_path = "./")
    {
        Uap_Archive uap_archive;

        auto uap_buffer = this->uap->dumpJson();

        // create uap
        uap_archive.write_new();
        uap_archive.write_open_filename(this->uap->getUapName());

        // add uap-1
        uap_archive.add_file(uap_buffer, this->uap->meta.getMetaName());

        /*
        //get uap sign string
        string uap_buffer_str = uap_buffer;
        QString data_input = uap_buffer_str.c_str();
        QString sign_data;
        if (!makeSign(data_input, "/usr/share/ca-certificates/deepin/private/priv.crt",
        "/usr/share/ca-certificates/deepin/private/priv.key", sign_data)) { qInfo() << "sign uap failed!!"; return
        false;
        }
        // add  .uap-1.sign
        uap_archive.add_file(sign_data.toStdString(), this->uap->meta.getMetaSignName());
        */
        // add  .uap-1.sign
        uap_archive.add_file(uap_buffer, this->uap->meta.getMetaSignName());

        /*
        //get data.tgz sign string
        sign_data.clear();
        QFile data_file(this->dataPath);
        data_file.open(QIODevice::ReadOnly);
        auto qbt = data_file.readAll();
        if (!makeSign(qbt, "/usr/share/ca-certificates/deepin/private/priv.crt",
        "/usr/share/ca-certificates/deepin/private/priv.key", sign_data)) { qInfo() << "sign data.tgz failed!!"; return
        false;
        }
        data_file.close();
        uap_archive.add_file(sign_data.toStdString(), ".data.tgz.sig");
        */
        // add .data.tgz.sig
        uap_archive.add_file(uap_buffer, ".data.tgz.sig");

        // add data.tgz
        if (this->uap->isFullUab()) {
            uap_archive.add_data_file(this->dataPath.toStdString(), "data.tgz");
        }

        // create uap
        uap_archive.write_free();

        // move uap to uap_path
        if ((fileExists(QString::fromStdString(this->uap->getUapName()))) && (uap_path != QString("./"))
            && (uap_path != QString("."))) {
            QString uap_name = QString::fromStdString(this->uap->getUapName());
            QString new_uap_name = uap_path + QString("/") + uap_name;
            createDir(uap_path);
            if (fileExists(new_uap_name)) {
                QFile::remove(new_uap_name);
            }
            QFile::rename(uap_name, new_uap_name);
        }
        return true;
    }

    // make ouap
    bool MakeOuap(const QString &uap_path, const QString &ostree_repo, QString ouap_path = "./")
    {
        QString uapFile = QFileInfo(uap_path).fileName();
        QString extract_dir = QString("/tmp/") + uapFile;
        QString dest_path = uapFile + QString(".dir");
        QString out_commit = nullptr;
        //解压uap
        if (!Extract(uap_path, extract_dir)) {
            qInfo() << "extract uap failed!!!";
            return false;
        }
        if (!fileExists(extract_dir + QString("/uap-1"))) {
            qInfo() << "uap-1 does not exist!!!";
            return false;
        }
        //初始化uap
        if (!this->initConfig(extract_dir + QString("/uap-1"))) {
            qInfo() << "init uapconfig failed!!!";
            return false;
        }

        //解压data.tgz
        if (!Extract(extract_dir + QString("/data.tgz"), dest_path)) {
            qInfo() << "extract data.tgz failed!!!";
            return false;
        }

        //制作数据仓库repo
        if (!makeOstree(ostree_repo, "archive")) {
            qInfo() << "make ostree repo failed!!!";
            return false;
        }

        //导入数据到repo
        if (!commitOstree(ostree_repo, QString("update ") + QString::fromStdString(this->uap->meta.version),
                          QString("Name: ") + QString::fromStdString(this->uap->meta.appid), this->uap->getBranchName(),
                          dest_path, out_commit)) {
            qInfo() << "commit data failed!!!";
            return false;
        }

        //导入commit值到uap-1
        this->uap->meta.pkgext.ostree = out_commit.toStdString();

        //初始化config为在线包
        this->uap->meta.pkgext.type = 0;
        auto uap_buffer = this->uap->dumpJson();
        QString info = uap_buffer.c_str();
        if (info.isEmpty()) {
            qInfo() << "no info for uap !!!";
        }
        Uap_Archive uap_archive;
        // create ouap
        uap_archive.write_new();
        uap_archive.write_open_filename(this->uap->getUapName());

        // add uap-1
        uap_archive.add_file(uap_buffer, this->uap->meta.getMetaName());
        // add  .uap-1.sign
        uap_archive.add_file(uap_buffer, this->uap->meta.getMetaSignName());
        // add .data.tgz.sig
        uap_archive.add_file(uap_buffer, ".data.tgz.sig");
        // create ouap
        uap_archive.write_free();

        removeDir(extract_dir);
        removeDir(dest_path);

        // move ouap to ouap_path
        if ((fileExists(QString::fromStdString(this->uap->getUapName()))) && (ouap_path != QString("./"))
            && (ouap_path != QString("."))) {
            QString ouap_name = QString::fromStdString(this->uap->getUapName());
            QString new_ouap_name = ouap_path + QString("/") + ouap_name;
            createDir(ouap_path);
            if (fileExists(new_ouap_name)) {
                QFile::remove(new_ouap_name);
            }
            QFile::rename(ouap_name, new_ouap_name);
        }

        return true;
    }

    //解压uap
    bool Extract(const QString filename, const QString outdir)
    {
        Uap_Archive uap_archive;
        if (!fileExists(filename)) {
            return false;
        }
        createDir(outdir);
        return (uap_archive.extract_archive(filename.toStdString().c_str(), outdir.toStdString().c_str()));
    }

    //校验uap
    bool Check(const QString dirpath)
    {
        if (!dirExists(dirpath)) {
            qInfo() << "dirpath does not exist!!!";
            return false;
        }
        if (!fileExists(dirpath + QString("/uap-1"))) {
            qInfo() << "uap-1 does not exist!!!";
            return false;
        }
        if (!fileExists(dirpath + QString("/.uap-1.sig"))) {
            qInfo() << ".uap-1.sign does not exist!!!";
            return false;
        }

        //校验uap-1
        QFile uap_file(dirpath + QString("/uap-1"));
        uap_file.open(QIODevice::ReadOnly);
        auto qbt_uap = uap_file.readAll();

        QFile uap_sign_file(dirpath + QString("/.uap-1.sig"));
        uap_sign_file.open(QIODevice::ReadOnly);
        auto qbt_uap_sign = uap_sign_file.readAll();

        if (!checkSign(qbt_uap, qbt_uap_sign)) {
            qInfo() << "check uap-1 failed!!!";
            uap_file.close();
            uap_sign_file.close();
            return false;
        } else {
            qInfo() << "check uap-1 successed!!!";
            uap_file.close();
            uap_sign_file.close();
        }
        //初始化uap
        this->initConfig(dirpath + QString("/uap-1"));

        if (this->uap->isFullUab()) {
            if (!fileExists(dirpath + QString("/.data.tgz.sig"))) {
                qInfo() << ".data.tgz.sign does not exist!!!";
                return false;
            }
            if (!fileExists(dirpath + QString("/data.tgz"))) {
                qInfo() << "data.tgz does not exist!!!";
                return false;
            }
            QFile data_file(dirpath + QString("/data.tgz"));
            data_file.open(QIODevice::ReadOnly);
            auto qbt_data = data_file.readAll();

            QFile data_sign_file(dirpath + QString("/.data.tgz.sig"));
            data_sign_file.open(QIODevice::ReadOnly);
            auto qbt_data_sign = data_sign_file.readAll();

            if (!checkSign(qbt_data, qbt_data_sign)) {
                qInfo() << "check data.tgz failed!!!";
                data_file.close();
                data_sign_file.close();
                return false;
            } else {
                qInfo() << "check data.tgz successed!!!";
                data_file.close();
                data_sign_file.close();
                return true;
            }
        } else {
            return true;
        }
    }

    //获取信息uap
    bool GetInfo(const QString uapFile_path, const QString savePath = "")
    {
        QString uapFile = QFileInfo(uapFile_path).fileName();
        QString extract_dir = QString("/tmp/") + uapFile;
        //解压uap
        if (!Extract(uapFile_path, extract_dir)) {
            qInfo() << "extract uap failed!!!";
            return false;
        }
        if (!fileExists(extract_dir + QString("/uap-1"))) {
            qInfo() << "uap-1 does not exist!!!";
            return false;
        }
        //初始化uap
        if (!this->initConfig(extract_dir + QString("/uap-1"))) {
            qInfo() << "init uapconfig failed!!!";
            return false;
        }
        auto uap_buffer = this->uap->dumpJson();
        QString info = uap_buffer.c_str();
        //打印获取信息
        qInfo().noquote() << info;

        if (info.isEmpty()) {
            qInfo() << "no info for uap !!!";
        }
        removeDir(extract_dir);
        // QFile newFile(uapFile + QString(".info"));
        QFile newFile;
        if (savePath.isNull() || savePath.isEmpty()) {
            newFile.setFileName(uapFile + QString(".info"));
        } else {
            newFile.setFileName(savePath + "/" + uapFile + QString(".info"));
            createDir(savePath);
        }
        newFile.open(QIODevice::WriteOnly | QIODevice::Text);
        newFile.write(info.toUtf8());
        newFile.close();
        return true;
    }

    //  init uap info from uap file
    bool InitUapFromFile(const QString &uapFile)
    {
        if (!fileExists(uapFile)) {
            return false;
        }
        QString uap_file_extract_dir;
        extractUap(uapFile, uap_file_extract_dir);
        this->initConfig(uap_file_extract_dir + "/uap-1");
        if (this->uap->isFullUab()) {
            this->dataPath = uap_file_extract_dir + "/data.tgz";
        }
        createDir(QString("/deepin/linglong/layers/"));
        QString pkg_install_path = QString::fromStdString("/deepin/linglong/layers/" + this->uap->meta.appid + "/"
                                                          + this->uap->meta.version + "/" + this->uap->meta.arch);
        std::cout << pkg_install_path.toStdString() << std::endl;
        if (!createDir(pkg_install_path)) {
            return false;
        }
        if (this->uap->isFullUab()) {
            extractUapData(this->dataPath, pkg_install_path);

            // TODO: fix it ( will remove this )
            QString yaml_path = QString::fromStdString("/deepin/linglong/layers/" + this->uap->meta.appid + "/"
                                                       + this->uap->meta.version + "/" + this->uap->meta.arch + "/"
                                                       + this->uap->meta.appid + ".yaml");
            QString new_path =
                QString::fromStdString("/deepin/linglong/layers/" + this->uap->meta.appid + "/"
                                       + this->uap->meta.version + "/" + this->uap->meta.appid + ".yaml");
            if (linglong::util::fileExists(yaml_path)) {
                // link to file
                linglong::util::linkFile(yaml_path, new_path);
            }
            // TODO: fix it ( will remove this )
            QString info_path =
                QString::fromStdString("/deepin/linglong/layers/" + this->uap->meta.appid + "/"
                                       + this->uap->meta.version + "/" + this->uap->meta.arch + "/info.json");
            QString info_new_path =
                QString::fromStdString("/deepin/linglong/layers/" + this->uap->meta.appid + "/"
                                       + this->uap->meta.version + "/" + this->uap->meta.arch + "/info");

            if (linglong::util::fileExists(info_path)) {
                // link to file
                linglong::util::linkFile(info_path, info_new_path);
            }
        }
        return true;
    }

    // TODO: liujianqiang
    // se: 后续整改为返回统一的错误类
    bool makeSquashfsFromDataDir(const QString &dataPath, RetMessageFromUab &retMessage, const QString runTimePath = "",
                                 const QString savePath = "")
    {
        //转换目录为绝对路径
        QString absoluteDataPath = QDir(dataPath).absolutePath();

        //判断目录是否存在
        if (!dirExists(absoluteDataPath)) {
            qInfo() << dataPath + QString(" don't exists!!!");
            retMessage.setState(false);
            retMessage.setMessage(dataPath);
            retMessage.setCode(RetCode(RetCode::DataDirNotExists));
            return false;
        }

        //判断uap.json是否存在
        if (!fileExists(absoluteDataPath + QString(CONFIGJSON))) {
            qInfo() << dataPath + QString("/uap.json don't exists!!!");
            retMessage.setState(false);
            retMessage.setMessage(dataPath + QString(CONFIGJSON));
            retMessage.setCode(RetCode(RetCode::UapJsonFileNotExists));
            return false;
        }

        //初始化uap.json文件
        const QString uapJsonFilePath = absoluteDataPath + QString(CONFIGJSON);
        if (!this->initConfig(uapJsonFilePath)) {
            qInfo() << dataPath + QString(CONFIGJSON) + QString(" file format error!!!");
            retMessage.setState(false);
            retMessage.setMessage(dataPath + QString(CONFIGJSON));
            retMessage.setCode(RetCode(RetCode::UapJsonFormatError));
            return false;
        }

        //检查目录结构
        // check entries
        if (!dirExists(absoluteDataPath + QString(ENTRIESDIR))) {
            qInfo() << QString("need: entries of desktop file !");
            retMessage.setState(false);
            retMessage.setMessage(absoluteDataPath + QString(ENTRIESDIR));
            retMessage.setCode(RetCode(RetCode::DataEntriesDirNotExists));
            return false;
        }

        // check files dir
        if (!dirExists(absoluteDataPath + QString(FILEDIR))) {
            qInfo() << "need: files of bin file !";
            retMessage.setState(false);
            retMessage.setMessage(absoluteDataPath + QString(FILEDIR));
            retMessage.setCode(RetCode(RetCode::DataFilesDirNotExists));
            return false;
        }

        // check permission info.json
        if (!fileExists(absoluteDataPath + QString(INFOJSON))) {
            qInfo() << "need: info.json of permission !";
            retMessage.setState(false);
            retMessage.setMessage(absoluteDataPath + QString(INFOJSON));
            retMessage.setCode(RetCode(RetCode::DataInfoJsonNotExists));
            return false;
        }

        //获取存储文件路径
        QString absoluteSavePath;
        if (savePath.isNull() || savePath.isEmpty()) {
            absoluteSavePath = QDir(QString("./")).absolutePath() + QString("/") + this->uap->getSquashfsName();
        } else {
            createDir(QDir(QString(savePath)).absolutePath());
            absoluteSavePath = QDir(QString(savePath)).absolutePath() + QString("/") + this->uap->getSquashfsName();
        }

        //创建临时工作目录
        QString tmpWork;
        QTemporaryDir tempDir;
        if (tempDir.isValid()) {
            qInfo() << tempDir.path();
            tmpWork = tempDir.path();
        } else {
            qInfo() << tempDir.errorString();
            retMessage.setState(false);
            retMessage.setMessage(tempDir.errorString());
            retMessage.setCode(RetCode(RetCode::MakeTempWorkDirError));
            return false;
        }

        //创建对应的安装目录与runtime目录并复制对应的安装数据与runtime数据
        QString runtimeDirPath = tmpWork + QString("/runtime");
        QString installFilePath = tmpWork + QString("/")
                                  + QString::fromStdString(this->uap->meta.appid + "/" + this->uap->meta.version + "/"
                                                           + this->uap->meta.arch);
        createDir(installFilePath);
        createDir(runtimeDirPath);

        if (runTimePath.isNull() || runTimePath.isEmpty()) {
            copyDir(absoluteDataPath, installFilePath);
        } else {
            copyDir(QDir(runTimePath).absolutePath(), runtimeDirPath);
            copyDir(absoluteDataPath, installFilePath);
        }

        //创建info链接文件
        QString newInfoFilePath = installFilePath + QString("/info");
        linkFile(QString(".") + QString(INFOJSON), newInfoFilePath);

        //创建yaml链接文件
        QString newYamlFilePath = tmpWork + QString("/")
                                  + QString::fromStdString(this->uap->meta.appid + "/" + this->uap->meta.version + "/"
                                                           + this->uap->meta.appid + ".yaml");
        QString yamlFilePath =
            QString("./") + QString::fromStdString(this->uap->meta.arch + "/" + this->uap->meta.appid + ".yaml");
        linkFile(yamlFilePath, newYamlFilePath);

        // mksquashfs
        // TODO : 大应用mksquashfs时间较长，此处timeout 15min
        auto ret = Runner("mksquashfs", {tmpWork, absoluteSavePath}, 15 * 60 * 1000);
        if (!ret) {
            qCritical() << "mksquashfs failed:" << ret << "with call Runner"
                        << "mksquashfs" << tmpWork << absoluteSavePath;
            retMessage.setState(false);
            retMessage.setMessage(absoluteDataPath);
            retMessage.setCode(RetCode(RetCode::DataMakeSquashfsError));
            tempDir.remove();
            return false;
        } else {
            qInfo() << "mksquashfs " + absoluteSavePath + " successed!!!";
            retMessage.setState(true);
            retMessage.setMessage(absoluteSavePath);
            retMessage.setCode(RetCode(RetCode::DataMakeSquashfsSuccess));
            tempDir.remove();
            return true;
        }
    }

    //推包或者runtime到服务器接口
    bool pushOuapOrRuntimeToServer(const QString &repoPath, const QString ouapPath = "", const QString uapPath = "",
                                   const QString force = "false")
    {
        //判断仓库路径是否存在
        if (!dirExists(repoPath) && !fileExists(repoPath)) {
            qInfo() << repoPath + QString(" don't exists!!!");
            return false;
        }
        //判断是否强制执行
        if (force == "true") {
            //判断是否传入ouap路径,执行runtime导入操作
            if (ouapPath.isNull() || ouapPath.isEmpty()) {
                if (QFileInfo(repoPath).isDir()) {
                    //执行runtime导入操作， -d repo -f
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -d " + repoPath + " -f";
                    auto ret = Runner("import_app2repo.py", {"-d", repoPath, "-f"}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-d" << repoPath << "-f";
                        return false;
                    }
                } else {
                    //执行runtime导入操作，-r repo.tar -f
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -r " + repoPath + " -f";
                    auto ret = Runner("import_app2repo.py", {"-r", repoPath, "-f"}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-r" << repoPath << "-f";
                        return false;
                    }
                }
                //执行ouap导入操作
            } else if (uapPath.isNull() || uapPath.isEmpty()) {
                if (QFileInfo(repoPath).isDir()) {
                    //执行导入ouap包，-d repo -o ouapPath -f
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -d " + repoPath + " -o " + ouapPath + " -f";
                    auto ret = Runner("import_app2repo.py", {"-d", repoPath, "-o", ouapPath, "-f"}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-d" << repoPath << "-o" << ouapPath << "-f";
                        return false;
                    }
                } else {
                    //执行导入ouap包，-r repo.tar -o ouapPath -f
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -r " + repoPath + " -o " + ouapPath + " -f";
                    auto ret = Runner("import_app2repo.py", {"-d", repoPath, "-o", ouapPath, "-f"}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-r" << repoPath << "-o" << ouapPath << "-f";
                        return false;
                    }
                }
            } else {
                if (QFileInfo(repoPath).isDir()) {
                    //执行导入ouap包， -d repo -o ouapPath -u uapPath -f
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -d " + repoPath + " -o " + ouapPath + " -u " + uapPath + " -f";
                    auto ret = Runner("import_app2repo.py", {"-d", repoPath, "-o", ouapPath, "-u", uapPath, "-f"},
                                      20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-d" << repoPath << "-o" << ouapPath << "-u" << uapPath << "-f";
                        return false;
                    }
                } else {
                    //执行导入ouap包，-r repo.tar -o ouapPath -u uapPath -f
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -r " + repoPath + " -o " + ouapPath + " -u " + uapPath + " -f";
                    auto ret = Runner("import_app2repo.py", {"-r", repoPath, "-o", ouapPath, "-u", uapPath, "-f"},
                                      20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-r" << repoPath << "-o" << ouapPath << "-u" << uapPath << "-f";
                        return false;
                    }
                }
            }
        } else {
            //判断是否传入ouap路径,执行runtime导入操作
            if (ouapPath.isNull() || ouapPath.isEmpty()) {
                if (QFileInfo(repoPath).isDir()) {
                    //执行runtime导入操作， -d repo
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -d " + repoPath;
                    auto ret = Runner("import_app2repo.py", {"-d", repoPath}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-d" << repoPath;
                        return false;
                    }
                } else {
                    //执行runtime导入操作， -r repo.tar
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -r " + repoPath;
                    auto ret = Runner("import_app2repo.py", {"-r", repoPath}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-r" << repoPath;
                        return false;
                    }
                }
                //执行ouap导入操作
            } else if (uapPath.isNull() || uapPath.isEmpty()) {
                if (QFileInfo(repoPath).isDir()) {
                    //执行导入ouap包，-d repo -o ouapPath
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -d " + repoPath + " -o " + ouapPath;
                    auto ret = Runner("import_app2repo.py", {"-d", repoPath, "-o", ouapPath}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-d" << repoPath << "-o" << ouapPath;
                        return false;
                    }
                } else {
                    //执行导入ouap包，-r repo.tar -o ouapPath
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -r " + repoPath + " -o " + ouapPath;
                    auto ret = Runner("import_app2repo.py", {"-d", repoPath, "-o", ouapPath}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-r" << repoPath << "-o" << ouapPath;
                        return false;
                    }
                }
            } else {
                if (QFileInfo(repoPath).isDir()) {
                    //执行导入ouap包，-d repo -o ouapPath -u uapPath
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -d " + repoPath + " -o " + ouapPath + " -u " + uapPath;
                    auto ret =
                        Runner("import_app2repo.py", {"-d", repoPath, "-o", ouapPath, "-u", uapPath}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-d" << repoPath << "-o" << ouapPath << "-u" << uapPath;
                        return false;
                    }
                } else {
                    //执行导入ouap包，-r repo.tar -o ouapPath -u uapPath
                    qInfo() << "imoirt_app2repo.py whit call Runner: "
                            << "import_app2repo.py -r " + repoPath + " -o " + ouapPath + " -u " + uapPath;
                    auto ret =
                        Runner("import_app2repo.py", {"-r", repoPath, "-o", ouapPath, "-u", uapPath}, 20 * 60 * 1000);
                    if (!ret) {
                        qInfo() << "import_app2repo.py run failed: " << ret << "with call Runner"
                                << "import_app2repo.py"
                                << "-r" << repoPath << "-o" << ouapPath << "-u" << uapPath;
                        return false;
                    }
                }
            }
        }
        return true;
    }
};

typedef QList<Package> PackageList;

Q_DECLARE_METATYPE(Package)

Q_DECLARE_METATYPE(PackageList)

inline QDBusArgument &operator<<(QDBusArgument &argument, const Package &message)
{
    argument.beginStructure();
    argument << message.ID;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, Package &message)
{
    argument.beginStructure();
    argument >> message.ID;
    argument.endStructure();

    return argument;
}
