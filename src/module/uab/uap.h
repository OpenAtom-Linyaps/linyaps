/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <iostream>
#include <string>
#include <QString>
#include <json-struct/json_struct.h>

using std::string;
using std::vector;

namespace format {
namespace uap {

/*!
 * Uniontech App Bundle
 */
class UAP;

/*!
 * UAP Package Ext Info
 */
class PkgExt
{
public:
    int type = 0;
    int hashtype = 1;
    string ostree;
    string url;
    string tag;
    string uuid;
    string maintainer;
    string license;
    string sdk;

    JS_OBJ(type, hashtype, ostree, url, tag, uuid, maintainer, license, sdk);

public:
    PkgExt() {}
    ~PkgExt() {}
};

/*!
 * UAP Package Owner Info
 */
class Owner
{
public:
    string name;
    string website;
    string ostree;
    string info;
    string license;
    string keyinfo;

    JS_OBJ(name, website, ostree, info, license, keyinfo);

public:
    Owner() {}
    ~Owner() {}
};

/*!
 * UAP Package Meta Sign Info
 */
class MetaSign
{
public:
    string sign;

    JS_OBJ(sign);

public:
    MetaSign() {}
    ~MetaSign() {}
};

/*!
 * UAP Package Data Sign Info
 */
class DataSign
{
public:
    string sign;

    JS_OBJ(sign);

public:
    DataSign() {}
    ~DataSign() {}
};


/*!
 * UAP Permission
 */
class Permission
{
public:
    bool autostart = false;
    bool notification = false;
    bool trayicon = false;
    bool clipboard = false;
    bool account = false;
    bool bluetooth = false;
    bool camera = false;
    bool audio_record = false;
    bool installed_apps = false;

    JS_OBJ(autostart, notification, trayicon, clipboard, account, bluetooth, camera, audio_record, installed_apps);
};

/*!
 * UAP meta
 */
class Meta
{
public:
    string _uap_version = "1";
    string _uap_name = "uap";
    // PkgInfo pkginfo;

    string name;
    string version;
    string appid;
    string arch;
    string runtime;
    string description;

    vector<string> support_plugins;
    vector<string> plugins;

    PkgExt pkgext;
    Owner owner;
    Permission permissions;
    MetaSign metasign;
    DataSign datasign;

    JS_OBJ(name, version, appid, arch, runtime, description, support_plugins, plugins, pkgext, owner, permissions,
           metasign, datasign);

public:
    Meta() {}
    Meta(const string version, const string name)
        : _uap_version(version)
        , _uap_name(name)
    {
    }
    ~Meta() {}
    // get meta file name
    string getMetaName() const { return this->_uap_name + "-" + this->_uap_version; }

    // get meta sign file name
    string getMetaSignName() const { return "." + this->_uap_name + "-" + this->_uap_version + ".sig"; }

    // set meta sign
    void setMetaSign()
    {
        std::string meta_json = "abc";
        //     JS::serializeStruct(this->pkginfo) + JS::serializeStruct(this->pkgext) +
        //     JS::serializeStruct(this->owner);
        // if (meta_json != "")
    }
};

/*!
 * UAP Package Struct
 */
class UAP
{
public:
    Meta meta;

    inline bool isOnlineUab() { return this->meta.pkgext.type == 0 ? true : false; }

    inline bool isFullUab() { return this->meta.pkgext.type == 1 ? true : false; }

    // set uap meta info from json file
    bool setFromJson(const string buff)
    {
        if (!buff.empty()) {
            JS::ParseContext parseObj(buff);
            if (parseObj.parseTo(this->meta) != JS::Error::NoError) {
                std::string errorStr = parseObj.makeErrorString();
                std::cout << "ERR:" << errorStr.c_str() << std::endl;
                return false;
            }
            // check meta data
            if (this->meta.name == "" || this->meta.owner.name == "") {
                return false;
            }
            // set meta sign
            this->meta.setMetaSign();
            return true;
        }
        return false;
    }

    string dumpJson() const { return JS::serializeStruct(this->meta); }

    string getUapName()
    {
        if (isFullUab()) {
            return this->meta.appid + "-" + this->meta.version + "-" + this->meta.arch + "." + this->meta._uap_name;
        } else {
            return this->meta.appid + "-" + this->meta.version + "-" + this->meta.arch + "." + string("o")
                   + this->meta._uap_name;
        }
    }
    //获取分之名称
    QString getBranchName() const
    {
        string tmp_name =
            string("app/") + this->meta.appid + string("/") + this->meta.arch + string("/") + this->meta.version;
        return QString::fromStdString(tmp_name);
    }
    QString getSquashfsName() const
    {
        return QString::fromStdString(this->meta.appid + "-" + this->meta.version + "-" + this->meta.arch + "."
                                      + "squashfs");
    }
};

/*!
 * UAP OffLine Struct
 */
class UapOffLine : public UAP
{
public:
    UapOffLine() {}
    ~UapOffLine() {}
};

/*!
 * UAP OnLine Struct
 */
class UapOnLine : public UAP
{
public:
    UapOnLine() {}
    ~UapOnLine() {}
};

} // namespace uap

} // namespace format
