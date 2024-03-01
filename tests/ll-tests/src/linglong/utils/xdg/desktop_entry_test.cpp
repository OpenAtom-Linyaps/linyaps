/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/utils/xdg/desktop_entry.h"

#include <QTemporaryFile>

using namespace linglong::utils::xdg;

TEST(UtilsXDGDesktopEntry, Groups)
{
    auto entry = DesktopEntry::New("data/utils/xdg/org.deepin.calculator.desktop");
    ASSERT_EQ(entry.has_value(), true) << entry.error().message().toStdString();

    auto groups = entry->groups();
    ASSERT_EQ(groups.contains(DesktopEntry::MainSection), true) << groups.join(", ").toStdString();
    ASSERT_EQ(groups.size(), 1);
}

TEST(UtilsXDGDesktopEntry, GetString)
{
    auto entry = DesktopEntry::New("data/utils/xdg/org.deepin.calculator.desktop");
    ASSERT_EQ(entry.has_value(), true) << entry.error().message().toStdString();

    auto testGetStringValue = [&entry](const QString &key, const QString &expectedValue) {
        auto value = entry->getValue<QString>(key);
        ASSERT_EQ(value.has_value(), true) << value.error().message().toStdString();
        ASSERT_EQ(value->toStdString(), expectedValue.toStdString());
    };

    auto testGetStringValueCases = QMap<QString, QString>{
        { "Name", "Calculator" },
        { "Exec", "/opt/apps/org.deepin.calculator/files/bin/deepin-calculator" },
        { "TryExec", "/opt/apps/org.deepin.calculator/files/bin/deepin-calculator" },
        { "Terminal", "false" },
    };

    for (const auto &testCase : testGetStringValueCases.keys()) {
        testGetStringValue(testCase, testGetStringValueCases[testCase]);
    }
}

TEST(UtilsXDGDesktopEntry, SetString)
{
    auto entry = DesktopEntry::New("data/utils/xdg/org.deepin.calculator.desktop");
    ASSERT_EQ(entry.has_value(), true) << entry.error().message().toStdString();

    auto testSetStringValue = [&entry](const QString &key, const QString &expectedValue) {
        entry->setValue<QString>(key, expectedValue);

        auto value = entry->getValue<QString>(key);
        ASSERT_EQ(value.has_value(), true) << value.error().message().toStdString();
        ASSERT_EQ(value->toStdString(), expectedValue.toStdString());
    };

    auto testGetStringValueCases = QMap<QString, QString>{
        { "Name", "Calculator1" },
        { "Exec", "/opt/apps/org.deepin.calculator/files/bin/deepin-calculator1" },
        { "TryExec", "/opt/apps/org.deepin.calculator/files/bin/deepin-calculator1j" },
        { "Terminal", "false1" },
    };

    for (const auto &testCase : testGetStringValueCases.keys()) {
        testSetStringValue(testCase, testGetStringValueCases[testCase]);
    }
}

TEST(UtilsXDGDesktopEntry, SaveToFile)
{
    auto entry = DesktopEntry::New("data/utils/xdg/org.deepin.calculator.desktop");
    ASSERT_EQ(entry.has_value(), true) << entry.error().message().toStdString();

    QTemporaryFile tmpFile;
    tmpFile.open();
    if (!(tmpFile.isOpen() && tmpFile.isWritable())) {
        qCritical() << "Cannot get a temporary file.";
        GTEST_SKIP();
    }
    tmpFile.close();

    auto result = entry->saveToFile(tmpFile.fileName());
    ASSERT_EQ(result.has_value(), true);

    entry = DesktopEntry::New(tmpFile.fileName());
    ASSERT_EQ(entry.has_value(), true) << entry.error().message().toStdString();

    auto testGetStringValueCases = QMap<QString, QString>{
        { "Name", "Calculator" },
        { "Exec", "/opt/apps/org.deepin.calculator/files/bin/deepin-calculator" },
        { "TryExec", "/opt/apps/org.deepin.calculator/files/bin/deepin-calculator" },
        { "Terminal", "false" },
    };

    auto testGetStringValue = [&entry](const QString &key, const QString &expectedValue) {
        auto value = entry->getValue<QString>(key);
        ASSERT_EQ(value.has_value(), true) << value.error().message().toStdString();
        ASSERT_EQ(value->toStdString(), expectedValue.toStdString());
    };

    for (const auto &testCase : testGetStringValueCases.keys()) {
        testGetStringValue(testCase, testGetStringValueCases[testCase]);
    }
}
