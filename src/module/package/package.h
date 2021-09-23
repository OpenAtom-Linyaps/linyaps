/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QDBusArgument>
#include <QObject>
#include <QList>
#include <string>
#include <QFile>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <QDebug>

#include "module/uab/uap.h"
#include "module/util/fs.h"

using format::uap::UAP;
using linglong::util::fileExists;
using linglong::util::dirExists;
using linglong::util::makeData;
using linglong::util::extractUap;
using linglong::util::createDir;
using linglong::util::removeDir;
using linglong::util::extractUapData;
using linglong::util::makeSign;
using linglong::util::checkSign;

class Uap_Archive
{
public:
    struct archive *wb = nullptr;
    struct archive_entry *entry = nullptr;
    struct archive *a = nullptr;
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

        a = archive_read_new();
        archive_read_support_format_all(a);
        archive_read_support_compression_all(a);
        ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, flags);
        archive_write_disk_set_standard_lookup(ext);
        if ((r = archive_read_open_filename(a, filename, 10240))) {
#ifdef DEBUG
            perror("error:");
#endif
            goto release;
            return false;
            // exit(1);
        }
        for (;;) {
            r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF)
                break;
            if (r < ARCHIVE_OK)
                fprintf(stderr, "%s\n", archive_error_string(a));
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
                r = copy_data(a, ext);
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
        archive_read_close(a);
        archive_read_free(a);
        archive_write_close(ext);
        archive_write_free(ext);
        // exit(0);
        return true;
    release:
        if (NULL != a) {
            archive_read_free(a);
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
            this->dataPath = nullptr;
        }
    }
    bool InitUap(const QString &config, const QString &data = "")
    {
        this->initConfig(config);
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

    //make uap
    bool MakeUap()
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
        if (!makeSign(data_input, "/usr/share/ca-certificates/deepin/private/priv.crt", "/usr/share/ca-certificates/deepin/private/priv.key", sign_data)) {
            qInfo() << "sign uap failed!!";
            return false;
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
        if (!makeSign(qbt, "/usr/share/ca-certificates/deepin/private/priv.crt", "/usr/share/ca-certificates/deepin/private/priv.key", sign_data)) {
            qInfo() << "sign data.tgz failed!!";
            return false;
        }
        data_file.close();
        uap_archive.add_file(sign_data.toStdString(), ".data.tgz.sig");
        */
        //add .data.tgz.sig
        uap_archive.add_file(uap_buffer, ".data.tgz.sig");

        // add data.tgz
        if (this->uap->isFullUab()) {
            uap_archive.add_data_file(this->dataPath.toStdString(), "data.tgz");
        }
        // create uap
        uap_archive.write_free();
        return true;
    }

    //make ouap
    bool MakeOuap(const QString uap_path,QString ostree_repo = "/deepin/linglong/repo"){
        QString uapFile = QFileInfo(uap_path).fileName();
        QString extract_dir = QString("/tmp/") + uapFile;
        QString dest_path = uapFile + QString(".dir");
        //解压uap
        if(! Extract(uap_path, extract_dir)){
            qInfo() << "extract uap failed!!!";
            return false;
        }
        if (! fileExists(extract_dir + QString("/uap-1"))) {
            qInfo() << "uap-1 does not exist!!!";
            return false;
        }
         //初始化uap
        if(! this->initConfig(extract_dir + QString("/uap-1"))){
            qInfo() << "init uapconfig failed!!!";
            return false;
        }
        //初始化config为在线包
        this->uap->meta.pkgext.type = 0;
        auto uap_buffer = this->uap->dumpJson();
        QString info = uap_buffer.c_str();
        if(info.isEmpty()){
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
         //add .data.tgz.sig
        uap_archive.add_file(uap_buffer, ".data.tgz.sig");
        // create ouap
        uap_archive.write_free();
        //解压data.tgz
        if(! Extract(extract_dir + QString("/data.tgz"), dest_path)){
            qInfo() << "extract data.tgz failed!!!";
            return false;
        }

        removeDir(extract_dir);
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
    bool GetInfo(const QString uapFile_path){
        QString uapFile = QFileInfo(uapFile_path).fileName();
        QString extract_dir = QString("/tmp/") + uapFile;
        //解压uap
        if(! Extract(uapFile_path, extract_dir)){
            qInfo() << "extract uap failed!!!";
            return false;
        }
        if (! fileExists(extract_dir + QString("/uap-1"))) {
            qInfo() << "uap-1 does not exist!!!";
            return false;
        }
         //初始化uap
        if(! this->initConfig(extract_dir + QString("/uap-1"))){
            qInfo() << "init uapconfig failed!!!";
            return false;
        }
        auto uap_buffer = this->uap->dumpJson();
        QString info = uap_buffer.c_str();
        if(info.isEmpty()){
            qInfo() << "no info for uap !!!";
        }
        removeDir(extract_dir);
        QFile newFile(uapFile + QString(".info"));
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
        QString pkg_install_path =
            QString::fromStdString("/deepin/linglong/layers/" + this->uap->meta.appid + "/"
                                   + this->uap->meta.version + "/" + this->uap->meta.arch);
        std::cout << pkg_install_path.toStdString() << std::endl;
        if (!createDir(pkg_install_path)) {
            return false;
        }
        if (this->uap->isFullUab()) {
            extractUapData(this->dataPath, pkg_install_path);
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
