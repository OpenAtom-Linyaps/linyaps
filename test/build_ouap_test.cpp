/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     liujianqiang <liujianqiang@uniontech.com>
 *
 * Maintainer: liujianqiang <liujianqiang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "module/package/package.h"

#define UAPNAME "org.deepin.calculator-1.2.2-x86_64.uap"
#define OUAPNAME "org.deepin.calculator-1.2.2-x86_64.ouap"
#define CONFIGFILE "../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64/uap.json"
#define DATADIR "../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64"

TEST(Package, Make_ouap1)
{
    Package pkg01;
    QString uapName = UAPNAME;
    QString ouapName = UAPNAME;
    QFileInfo fs(uapName);

    //预处理文件
    if (fs.exists()) {
        QFile::remove(uapName);
    }
    if (QFileInfo::exists(ouapName)) {
        QFile::remove(ouapName);
    }

    //创建临时仓库目录
    QString pool;
    QTemporaryDir tempDir;
    if (tempDir.isValid()) {
        qInfo() << tempDir.path();
        pool = tempDir.path();
    }

    //生产离线包uap
    EXPECT_EQ(pkg01.InitUap(QString(CONFIGFILE), QString(DATADIR)), true);
    EXPECT_EQ(pkg01.MakeUap(), true);
    if (QFileInfo::exists(uapName)){
        //生成在线包ouap
        EXPECT_EQ(pkg01.MakeOuap(uapName, pool), true);
        EXPECT_EQ(fileExists(ouapName), true);
    }

    //处理临时文件
    if (fileExists(ouapName)) {
        QFile::remove(ouapName);
    }

    if (QFileInfo::exists(uapName)) {
        QFile::remove(uapName);
    }
    if (QFileInfo::exists(ouapName)) {
        QFile::remove(ouapName);
    }

    //移除临时目录
    tempDir.remove();
}