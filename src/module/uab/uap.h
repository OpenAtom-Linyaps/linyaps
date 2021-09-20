/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
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

#include <iostream>
#include <string>
#include <json-struct/json_struct.h>

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <crypto++/sha.h>
#include <crypto++/filters.h>
#include <crypto++/hex.h>

using std::string;
using std::vector;

using namespace CryptoPP;

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
    PkgExt() { }
    ~PkgExt() { }
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
    Owner() { }
    ~Owner() { }
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
    MetaSign() { }
    ~MetaSign() { }
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
    DataSign() { }
    ~DataSign() { }
};

/*!
 * String to Hash256
 */
inline const string StringToHash256(const string &str)
{
    SHA256 hash;
    std::string digest;
    if (str != "")
        StringSource s(str, true, new HashFilter(hash, new HexEncoder(new StringSink(digest))));
    if (digest != "") {
        return digest;
    }
}

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
    //PkgInfo pkginfo;

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

    JS_OBJ(name, version, appid, arch, runtime, description,
           support_plugins, plugins, pkgext, owner,
           permissions, metasign, datasign);

public:
    Meta() { }
    Meta(const string version, const string name)
        : _uap_version(version)
        , _uap_name(name)
    {
    }
    ~Meta() { }
    // get meta file name
    string getMetaName() const { return this->_uap_name + "-" + this->_uap_version; }

    // get meta sign file name
    string getMetaSignName() const { return "." + this->_uap_name + "-" + this->_uap_version + ".sig"; }

    // set meta sign
    void setMetaSign()
    {
        std::string meta_json = "abc";
        //     JS::serializeStruct(this->pkginfo) + JS::serializeStruct(this->pkgext) + JS::serializeStruct(this->owner);
        // if (meta_json != "")
        this->metasign.sign = StringToHash256(meta_json);
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
            return this->meta.name + "-" + this->meta.version + "-" + this->meta.arch + "."
                   + this->meta._uap_name;
        } else {
            return this->meta.name + "-" + this->meta.version + "-" + this->meta.arch + "."
                   + string("o") + this->meta._uap_name;
        }
    }
};

/*!
 * UAP OffLine Struct
 */
class UapOffLine : public UAP
{
public:
    UapOffLine() { }
    ~UapOffLine() { }
};

/*!
 * UAP OnLine Struct
 */
class UapOnLine : public UAP
{
public:
    UapOnLine() { }
    ~UapOnLine() { }
};

} // namespace uap

} // namespace format
