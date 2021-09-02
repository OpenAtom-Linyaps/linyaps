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

#include <string>

using std::string;

namespace format {
namespace uap {

/*!
 * Uniontech App Bundle
 */
class UAP;

/*!
 * UAP PackageInfo
 */
class PkgInfo {
 public:
    string name;
    string version;
    string appname;
    string uuid;
    string arch;
    string sdk;
    string runtime;
    string description;

 public:
    PkgInfo() {}
    ~PkgInfo() {}
};

/*!
 * UAP Package Ext Info
 */
class PkgExt {
 public:
    int type = 0;
    int hashtype = 1;
    string ostree;
    string url;
    string tag;
    string maintainer;

 public:
    PkgExt() {}
    ~PkgExt() {}
};

/*!
 * UAP Package Owner Info
 */
class Owner {
 public:
    string name;
    string website;
    string ostree;
    string info;
    string license;
    string keyinfo;

 public:
    Owner() {}
    ~Owner() {}
};

/*!
 * UAP Package Meta Sign Info
 */
class MetaSign {
 public:
    string sign;

 public:
    MetaSign() {}
    ~MetaSign() {}
};

/*!
 * UAP Package Data Sign Info
 */
class DataSign {
 public:
    string sign;

 public:
    DataSign() {}
    ~DataSign() {}
};

/*!
 * UAP meta
 */
class Meta {
 public:
    string version = "1";
    string name = "uap";
    PkgInfo pkgInfo;
    PkgExt pkgExt;
    Owner owner;
    MetaSign metaSign;
    DataSign dataSign;

 public:
    Meta() {}
    Meta(const string version, const string name)
        : version(version)
        , name(name) {}
    ~Meta() {}
    string getMetaName() const { return this->name + "-" + this->version; }
    string getMetaSignName() const { return "." + this->name + "-" + this->version + ".sig"; }
};

/*!
 * UAP Package Struct
 */
class UAP {
 public:
    Meta *meta;

    bool isOnlineUab() { return this->meta->pkgExt.type == 0 ? true : false; }

    bool isFullUab() { return this->meta->pkgExt.type == 1 ? true : false; }

    string getUapName() const {
        return this->meta->pkgInfo.name + "-" + this->meta->pkgInfo.version + "-" + this->meta->pkgInfo.arch + "."
               + this->meta->name;
    }
};

/*!
 * UAP OffLine Struct
 */
class UapOffLine : public UAP {
 public:
    UapOffLine() {}
    ~UapOffLine() {}
};

/*!
 * UAP OnLine Struct
 */
class UapOnLine : public UAP {
 public:
    UapOnLine() {}
    ~UapOnLine() {}
};

}  // namespace uap

}  // namespace format
