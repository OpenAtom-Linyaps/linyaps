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

#include "module/uab/uap.h"
#include "module/util/fs.h"

using format::uap::UAP;
using linglong::util::fileExists;
using linglong::util::dirExists;
using linglong::util::makeData;
using linglong::util::extractUap;
using linglong::util::createDir;

class Package
{
public:
    QString ID;
    QString name;
    QString appName;
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

        // make data.gz
        // tar -C /path/to/directory -cf - . | gzip --rsyncable >data.tgz
        // TODO:(fix) set temp directory
        makeData(this->dataDir, this->dataPath);
        return true;
    }

    bool MakeUap()
    {
        // create uap-1
        auto uap_buffer = this->uap->dumpJson();

        struct archive *wb = nullptr;
        struct archive_entry *entry = nullptr;
        struct stat workdir_st, st;
        stat(".", &workdir_st);
        wb = archive_write_new();
        if (wb == nullptr) {
            // #TODO
            return false;
        }
        archive_write_add_filter_none(wb);
        archive_write_set_format_pax_restricted(wb);
        archive_write_open_filename(wb, this->uap->getUapName().c_str());

        // add uap-1
        entry = archive_entry_new();
        if (entry == nullptr) {
            // #TODO
            return false;
        }
        archive_entry_copy_stat(entry, &workdir_st);
        archive_entry_set_pathname(entry, this->uap->meta.getMetaName().c_str());
        archive_entry_set_size(entry, uap_buffer.length());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(wb, entry);
        archive_write_data(wb, uap_buffer.c_str(), uap_buffer.length());
        archive_entry_free(entry);

        // add sign file
        entry = archive_entry_new();
        if (entry == nullptr) {
            // #TODO
            return false;
        }
        archive_entry_copy_stat(entry, &workdir_st);
        archive_entry_set_pathname(entry, this->uap->meta.getMetaSignName().c_str());
        archive_entry_set_size(entry, uap_buffer.length());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(wb, entry);
        archive_write_data(wb, uap_buffer.c_str(), uap_buffer.length());
        archive_entry_free(entry);

        // add data.tgz
        if (this->uap->isFullUab()) {
            stat(this->dataPath.toStdString().c_str(), &st);
            entry = archive_entry_new(); // Note 2
            if (entry == nullptr) {
                // #TODO
                return false;
            }
            archive_entry_copy_stat(entry, &st);
            archive_entry_set_pathname(entry, "data.tgz");
            archive_write_header(wb, entry);
            char buff[40960] = {
                0,
            };
            int len = 0;
            int fd = -1;
            fd = open(this->dataPath.toStdString().c_str(), O_RDONLY);
            len = read(fd, buff, sizeof(buff));
            while (len > 0) {
                archive_write_data(wb, buff, len);
                len = read(fd, buff, sizeof(buff));
            }
            close(fd);
            archive_entry_free(entry);
        }
        // create uap
        archive_write_close(wb); // Note 4
        archive_write_free(wb); // Note

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
        QString pkg_install_path = QString::fromStdString("/deepin/linglong/layers/" + this->uap->meta.pkginfo.appname + "/" + this->uap->meta.pkginfo.version
                  + "/" + this->uap->meta.pkginfo.arch + "/entries");
        std::cout<<pkg_install_path.toStdString()<<std::endl;
        createDir(pkg_install_path);
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
