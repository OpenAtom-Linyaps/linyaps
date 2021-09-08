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
#include "json_struct.h"

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <crypto++/sha.h>
#include <crypto++/filters.h>
#include <crypto++/hex.h>

using std::string;

using namespace CryptoPP;

namespace format {
namespace uap {

/*!
 * Uniontech App Bundle
 */
class UAP;

/*!
 * UAP PackageInfo
 */
class PkgInfo
{
public:
    string name;
    string version;
    string appname;
    string uuid;
    string arch;
    string sdk;
    string runtime;
    string description;

    JS_OBJ(name, version, appname, uuid, arch, sdk, runtime, description);

public:
    PkgInfo() {}
    ~PkgInfo() {}
};

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
    string maintainer;

    JS_OBJ(type, hashtype, ostree, url, tag, maintainer);

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
 * UAP meta
 */
class Meta
{
public:
    string version = "1";
    string name = "uap";
    PkgInfo pkginfo;
    PkgExt pkgext;
    Owner owner;
    MetaSign metasign;
    DataSign datasign;

    JS_OBJ(pkginfo, pkgext, owner, metasign, datasign);

public:
    Meta() {}
    Meta(const string version, const string name)
        : version(version)
        , name(name)
    {
    }
    ~Meta() {}
    // get meta file name
    string getMetaName() const { return this->name + "-" + this->version; }

    // get meta sign file name
    string getMetaSignName() const { return "." + this->name + "-" + this->version + ".sig"; }

    // set meta sign
    void setMetaSign()
    {
        std::string meta_json =
            JS::serializeStruct(this->pkginfo) + JS::serializeStruct(this->pkgext) + JS::serializeStruct(this->owner);
        if (meta_json != "")
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
            if (this->meta.pkginfo.name == "" || this->meta.owner.name == "") {
                return false;
            }
            // set meta sign
            this->meta.setMetaSign();
            return true;
        }
        return false;
    }

    string dumpJson() const { return JS::serializeStruct(this->meta); }

    string getUapName() const
    {
        return this->meta.pkginfo.name + "-" + this->meta.pkginfo.version + "-" + this->meta.pkginfo.arch + "."
               + this->meta.name;
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
