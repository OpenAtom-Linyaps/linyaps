/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/common/gkeyfile_wrapper.h"
#include "linglong/common/strings.h"
#include "linglong/common/xdg.h"
#include "linglong/utils/env.h"
#include "linglong/utils/error/error.h"

#include <QTemporaryFile>
#include <QTextStream>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace linglong::common::test {

namespace fs = std::filesystem;
using namespace linglong::common::strings;

class CommonDeepSuiteTest : public ::testing::Test
{
};

TEST_F(CommonDeepSuiteTest, StringsCaseInsensitiveEqualityEdgeCases)
{
    EXPECT_TRUE(stringEqual("LINGLONG", "linglong", false));
    EXPECT_TRUE(stringEqual("LingLong-v2", "linglong-V2", false));
    EXPECT_FALSE(stringEqual("linglong", "linglong_extra", false));
    EXPECT_FALSE(stringEqual("short", "longer_string", false));
    EXPECT_FALSE(stringEqual("prefix_same", "prefix_diff", false));

    std::string longA(1000, 'A');
    std::string longB(1000, 'a');
    EXPECT_TRUE(stringEqual(longA, longB, false));
    EXPECT_FALSE(stringEqual(longA, longB, true));

    std::string longDiff = longB;
    longDiff[500] = 'z';
    EXPECT_FALSE(stringEqual(longA, longDiff, false));
}

TEST_F(CommonDeepSuiteTest, StringsTrimAdvancedCharacters)
{
    EXPECT_EQ(trim("---===content===---", "-="), "content");
    EXPECT_EQ(trim("///var/lib/linglong///", "/"), "var/lib/linglong");
    EXPECT_EQ(trim("$$$12345$$$", "$"), "12345");

    EXPECT_EQ(trim_left(">>>message<<<", ">"), "message<<<");
    EXPECT_EQ(trim_right(">>>message<<<", "<"), ">>>message");

    EXPECT_EQ(trim_left(" \r\n\t  mixed whitespace \r\n", " \r\n\t"), "mixed whitespace \r\n");
    EXPECT_EQ(trim_right(" \r\n\t  mixed whitespace \r\n", " \r\n\t"), " \r\n\t  mixed whitespace");
    EXPECT_EQ(trim(" \r\n\t  mixed whitespace \r\n", " \r\n\t"), "mixed whitespace");
}

TEST_F(CommonDeepSuiteTest, StringsSplitComplexDelimitersAndFlags)
{
    auto resNone = split("apple:banana::cherry:", ':', splitOption::None);
    ASSERT_EQ(resNone.size(), 5);
    EXPECT_EQ(resNone[0], "apple");
    EXPECT_EQ(resNone[1], "banana");
    EXPECT_EQ(resNone[2], "");
    EXPECT_EQ(resNone[3], "cherry");
    EXPECT_EQ(resNone[4], "");

    auto resSkip = split("apple:banana::cherry:", ':', splitOption::SkipEmpty);
    ASSERT_EQ(resSkip.size(), 3);
    EXPECT_EQ(resSkip[0], "apple");
    EXPECT_EQ(resSkip[1], "banana");
    EXPECT_EQ(resSkip[2], "cherry");

    auto resTrim = split("  x  |  y  || z ", '|', splitOption::TrimWhitespace | splitOption::SkipEmpty);
    ASSERT_EQ(resTrim.size(), 3);
    EXPECT_EQ(resTrim[0], "x");
    EXPECT_EQ(resTrim[1], "y");
    EXPECT_EQ(resTrim[2], "z");

    auto resNoDelim = split("single_token_without_delim", ',', splitOption::None);
    ASSERT_EQ(resNoDelim.size(), 1);
    EXPECT_EQ(resNoDelim[0], "single_token_without_delim");
}

TEST_F(CommonDeepSuiteTest, StringsJoinVariedCollections)
{
    EXPECT_EQ(join({ "one", "two", "three", "four" }, '/'), "one/two/three/four");
    EXPECT_EQ(join({ "path" }, '/'), "path");
    EXPECT_EQ(join({}, ':'), "");
    EXPECT_EQ(join({ "a", "b" }, '\0'), std::string("a\0b", 3));
}

TEST_F(CommonDeepSuiteTest, StringsReplaceSubstringBoundaryPatterns)
{
    EXPECT_EQ(replaceSubstring("aaaaa", "a", "bb"), "bbbbbbbbbb");
    EXPECT_EQ(replaceSubstring("root/sub/path", "/", "::"), "root::sub::path");
    EXPECT_EQ(replaceSubstring("nothing to replace", "xyz", "abc"), "nothing to replace");
    EXPECT_EQ(replaceSubstring("prefix_target_suffix", "target", ""), "prefix__suffix");
    EXPECT_EQ(replaceSubstring("", "find", "replace"), "");
    EXPECT_EQ(replaceSubstring("find", "find", ""), "");
}

TEST_F(CommonDeepSuiteTest, StringsPrefixSuffixAndContainsPredicates)
{
    std::string s = "linglong.package.manager";
    EXPECT_TRUE(starts_with(s, "linglong"));
    EXPECT_TRUE(starts_with(s, "linglong.package"));
    EXPECT_FALSE(starts_with(s, "package"));

    EXPECT_TRUE(ends_with(s, "manager"));
    EXPECT_TRUE(ends_with(s, ".package.manager"));
    EXPECT_FALSE(ends_with(s, "package"));

    EXPECT_TRUE(contains(s, "package"));
    EXPECT_TRUE(contains(s, "long.pack"));
    EXPECT_FALSE(contains(s, "unknown"));

    EXPECT_TRUE(starts_with(s, ""));
    EXPECT_TRUE(ends_with(s, ""));
    EXPECT_TRUE(contains(s, ""));
}

TEST_F(CommonDeepSuiteTest, StringsQuoteBashArgShellSafety)
{
    EXPECT_EQ(quoteBashArg("normal"), "'normal'");
    EXPECT_EQ(quoteBashArg(""), "''");
    EXPECT_EQ(quoteBashArg("echo `rm -rf /`"), "'echo `rm -rf /`'");
    EXPECT_EQ(quoteBashArg("$(whoami)"), "'$(whoami)'");
    EXPECT_EQ(quoteBashArg("foo && bar || baz; qux"), "'foo && bar || baz; qux'");
    EXPECT_EQ(quoteBashArg("can't stop won't stop"), "'can'\\''t stop won'\\''t stop'");
}

TEST_F(CommonDeepSuiteTest, StringsUrlEncodingExhaustivePunctuation)
{
    std::string raw = "https://linglong.deepin.org/pkg?id=demo&ver=1.0#section";
    auto encoded = encode_url(raw);
    EXPECT_NE(encoded.find("%3A"), std::string::npos);
    EXPECT_NE(encoded.find("%2F"), std::string::npos);
    EXPECT_NE(encoded.find("%3F"), std::string::npos);
    EXPECT_NE(encoded.find("%26"), std::string::npos);
    EXPECT_NE(encoded.find("%23"), std::string::npos);

    auto decoded = decode_url(encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, raw);
}

TEST_F(CommonDeepSuiteTest, StringsUrlDecodingMalformedSequences)
{
    EXPECT_FALSE(decode_url("%").has_value());
    EXPECT_FALSE(decode_url("%A").has_value());
    EXPECT_FALSE(decode_url("%ZZ").has_value());
    EXPECT_FALSE(decode_url("%1Z").has_value());
    EXPECT_FALSE(decode_url("prefix%2").has_value());
    EXPECT_FALSE(decode_url("invalid%XXsuffix").has_value());
}

TEST_F(CommonDeepSuiteTest, GKeyFileWrapperMultipleSectionsAndTypes)
{
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    const QString sampleIni = R"([Desktop Entry]
Name=Deepin Music
Exec=/usr/bin/deepin-music %U
Terminal=false
Categories=Audio;Music;Player;

[X-Linglong]
PackageId=org.deepin.music
Version=7.0.1
Arch=x86_64
)";

    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << sampleIni;
    }

    auto loadRes = GKeyFileWrapper::New(path);
    ASSERT_TRUE(loadRes.has_value());
    auto &wrapper = *loadRes;

    auto groups = wrapper.getGroups();
    EXPECT_EQ(groups.size(), 2);
    EXPECT_TRUE(groups.contains("Desktop Entry"));
    EXPECT_TRUE(groups.contains("X-Linglong"));

    auto pkgId = wrapper.getValue<QString>("PackageId", "X-Linglong");
    ASSERT_TRUE(pkgId.has_value());
    EXPECT_EQ(*pkgId, "org.deepin.music");

    auto ver = wrapper.getValue<QString>("Version", "X-Linglong");
    ASSERT_TRUE(ver.has_value());
    EXPECT_EQ(*ver, "7.0.1");

    // Setting a new section and saving
    wrapper.setValue("Enabled", "true", "CustomSection");
    wrapper.setValue("MaxRetries", "5", "CustomSection");

    QString newPath = path + ".modified";
    ASSERT_TRUE(wrapper.saveToFile(newPath).has_value());

    auto reloaded = GKeyFileWrapper::New(newPath);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_TRUE(reloaded->getGroups().contains("CustomSection"));
    auto enabledVal = reloaded->getValue<QString>("Enabled", "CustomSection");
    ASSERT_TRUE(enabledVal.has_value());
    EXPECT_EQ(*enabledVal, "true");

    QFile::remove(newPath);
}

TEST_F(CommonDeepSuiteTest, GKeyFileWrapperRemoveKeysAndOverwrite)
{
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    const QString ini = R"([Settings]
Theme=dark
FontSize=12
)";
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << ini;
    }

    auto loadRes = GKeyFileWrapper::New(path);
    ASSERT_TRUE(loadRes.has_value());
    auto &wrapper = *loadRes;

    EXPECT_TRUE(wrapper.hasKey("Theme", "Settings").value_or(false));
    EXPECT_TRUE(wrapper.hasKey("FontSize", "Settings").value_or(false));

    wrapper.setValue("Theme", "light", "Settings");
    auto themeVal = wrapper.getValue<QString>("Theme", "Settings");
    ASSERT_TRUE(themeVal.has_value());
    EXPECT_EQ(*themeVal, "light");

    auto nonExist = wrapper.getValue<QString>("NotThere", "Settings");
    EXPECT_FALSE(nonExist.has_value());
}

TEST_F(CommonDeepSuiteTest, XdgRuntimeDirFallbackAndCustomGuards)
{
    {
        linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", "/var/run/custom_user_1000");
        auto dir = linglong::common::xdg::getXDGRuntimeDir();
        EXPECT_EQ(dir, "/var/run/custom_user_1000");
    }

    {
        linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", "/tmp/non_existent_path_fallback");
        auto dir = linglong::common::xdg::getXDGRuntimeDir();
        EXPECT_EQ(dir, "/tmp/non_existent_path_fallback");
    }
}

TEST_F(CommonDeepSuiteTest, SharedPermissionsConstantsMask)
{
    auto dirPerms = linglong::common::shared_directory_permissions;
    EXPECT_NE((dirPerms & fs::perms::owner_read), fs::perms::none);
    EXPECT_NE((dirPerms & fs::perms::owner_write), fs::perms::none);
    EXPECT_NE((dirPerms & fs::perms::owner_exec), fs::perms::none);
    EXPECT_NE((dirPerms & fs::perms::group_read), fs::perms::none);
    EXPECT_NE((dirPerms & fs::perms::group_exec), fs::perms::none);
    EXPECT_NE((dirPerms & fs::perms::others_read), fs::perms::none);
    EXPECT_NE((dirPerms & fs::perms::others_exec), fs::perms::none);

    auto filePerms = linglong::common::shared_file_permissions;
    EXPECT_NE((filePerms & fs::perms::owner_read), fs::perms::none);
    EXPECT_NE((filePerms & fs::perms::owner_write), fs::perms::none);
    EXPECT_NE((filePerms & fs::perms::group_read), fs::perms::none);
    EXPECT_NE((filePerms & fs::perms::others_read), fs::perms::none);
    EXPECT_EQ((filePerms & fs::perms::owner_exec), fs::perms::none);
}

TEST_F(CommonDeepSuiteTest, StringsSplitMultipleTokensAndEmptySpaces)
{
    std::string text = "   field1  ,  field2 ,field3, , field5   ";
    auto tokens = split(text, ',', splitOption::TrimWhitespace | splitOption::SkipEmpty);
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0], "field1");
    EXPECT_EQ(tokens[1], "field2");
    EXPECT_EQ(tokens[2], "field3");
    EXPECT_EQ(tokens[3], "field5");

    auto tokensWithEmpty = split(text, ',', splitOption::TrimWhitespace);
    ASSERT_EQ(tokensWithEmpty.size(), 5);
    EXPECT_EQ(tokensWithEmpty[3], "");
}

TEST_F(CommonDeepSuiteTest, StringsUrlEncodingNonAsciiAndEmoji)
{
    std::string utf8Text = "玲珑操作系统-Linyaps";
    auto encoded = encode_url(utf8Text);
    EXPECT_NE(encoded.find("%"), std::string::npos);

    auto decoded = decode_url(encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, utf8Text);
}

TEST_F(CommonDeepSuiteTest, StringsQuoteBashArgDoubleQuotesAndVariables)
{
    EXPECT_EQ(quoteBashArg("PATH=$PATH:/opt/bin"), "'PATH=$PATH:/opt/bin'");
    EXPECT_EQ(quoteBashArg("`uname -a`"), "'`uname -a`'");
    EXPECT_EQ(quoteBashArg("test 'with' embedded 'quotes'"), "'test '\\''with'\\'' embedded '\\''quotes'\\'''");
    EXPECT_EQ(quoteBashArg("\n\t\r"), "'\n\t\r'");
}

TEST_F(CommonDeepSuiteTest, GKeyFileWrapperInvalidSyntaxHandling)
{
    QTemporaryFile badFile;
    ASSERT_TRUE(badFile.open());
    QString path = badFile.fileName();
    badFile.close();

    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "[[[Bad Syntax No Equal Sign]]]\nJustSomeTextWithoutKey\n";
    }

    auto res = GKeyFileWrapper::New(path);
    EXPECT_FALSE(res.has_value());
}

TEST_F(CommonDeepSuiteTest, GKeyFileWrapperUnicodeContentRoundtrip)
{
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    const QString content = R"([Desktop Entry]
Name=统信深度应用
Comment=Linux 桌面操作系统
GenericName=开发工具集
)";

    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << content;
    }

    auto loadRes = GKeyFileWrapper::New(path);
    ASSERT_TRUE(loadRes.has_value());
    auto &wrapper = *loadRes;

    auto name = wrapper.getValue<QString>("Name", "Desktop Entry");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "统信深度应用");

    auto comment = wrapper.getValue<QString>("Comment", "Desktop Entry");
    ASSERT_TRUE(comment.has_value());
    EXPECT_EQ(*comment, "Linux 桌面操作系统");

    wrapper.setValue("Keywords", "系统;深度;玲珑;", "Desktop Entry");
    QString savePath = path + ".utf8";
    ASSERT_TRUE(wrapper.saveToFile(savePath).has_value());

    auto reloaded = GKeyFileWrapper::New(savePath);
    ASSERT_TRUE(reloaded.has_value());
    auto kw = reloaded->getValue<QString>("Keywords", "Desktop Entry");
    ASSERT_TRUE(kw.has_value());
    EXPECT_EQ(*kw, "系统;深度;玲珑;");

    QFile::remove(savePath);
}

TEST_F(CommonDeepSuiteTest, GKeyFileWrapperMultipleGroupKeyQuery)
{
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    const QString content = R"([GroupA]
k1=v1
k2=v2

[GroupB]
k3=v3
k4=v4
)";

    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << content;
    }

    auto loadRes = GKeyFileWrapper::New(path);
    ASSERT_TRUE(loadRes.has_value());
    auto &wrapper = *loadRes;

    auto keysA = wrapper.getkeys("GroupA");
    ASSERT_TRUE(keysA.has_value());
    EXPECT_EQ(keysA->size(), 2);
    EXPECT_TRUE(keysA->contains("k1"));
    EXPECT_TRUE(keysA->contains("k2"));

    auto keysB = wrapper.getkeys("GroupB");
    ASSERT_TRUE(keysB.has_value());
    EXPECT_EQ(keysB->size(), 2);
    EXPECT_TRUE(keysB->contains("k3"));
    EXPECT_TRUE(keysB->contains("k4"));

    auto keysMissing = wrapper.getkeys("GroupC");
    EXPECT_FALSE(keysMissing.has_value());
}

TEST_F(CommonDeepSuiteTest, StringsSplitWhitespaceOnlyAndEmpty)
{
    std::string ws = "   \t\t\n\r   ";
    auto tokens = split(ws, ',', splitOption::TrimWhitespace | splitOption::SkipEmpty);
    EXPECT_TRUE(tokens.empty());

    auto noTrimTokens = split(ws, ',', splitOption::None);
    ASSERT_EQ(noTrimTokens.size(), 1);
    EXPECT_EQ(noTrimTokens[0], ws);
}

TEST_F(CommonDeepSuiteTest, StringsReplaceMultipleOccurrencesInSequence)
{
    std::string s = "path/to/nested/directory/structure";
    EXPECT_EQ(replaceSubstring(s, "/", "\\"), "path\\to\\nested\\directory\\structure");
    EXPECT_EQ(replaceSubstring("111111", "11", "2"), "222");
    EXPECT_EQ(replaceSubstring("hello world world world", "world", "linglong"), "hello linglong linglong linglong");
}

TEST_F(CommonDeepSuiteTest, StringsUrlEncodingNumbersAndAlpha)
{
    std::string safeChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~";
    EXPECT_EQ(encode_url(safeChars), safeChars);
}

TEST_F(CommonDeepSuiteTest, GKeyFileWrapperOverwriteExistingGroup)
{
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    const QString content = R"([Initial]
Key1=Val1
)";
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << content;
    }

    auto loadRes = GKeyFileWrapper::New(path);
    ASSERT_TRUE(loadRes.has_value());
    auto &wrapper = *loadRes;

    wrapper.setValue("Key1", "OverwrittenVal1", "Initial");
    wrapper.setValue("Key2", "NewVal2", "Initial");

    QString savePath = path + ".saved";
    ASSERT_TRUE(wrapper.saveToFile(savePath).has_value());

    auto reloaded = GKeyFileWrapper::New(savePath);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(*reloaded->getValue<QString>("Key1", "Initial"), "OverwrittenVal1");
    EXPECT_EQ(*reloaded->getValue<QString>("Key2", "Initial"), "NewVal2");

    QFile::remove(savePath);
}

TEST_F(CommonDeepSuiteTest, StringsSplitByTabOrSpecialCharacters)
{
    std::string line = "col1\tcol2\tcol3\t\tcol5";
    auto tokens = split(line, '\t', splitOption::SkipEmpty);
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0], "col1");
    EXPECT_EQ(tokens[1], "col2");
    EXPECT_EQ(tokens[2], "col3");
    EXPECT_EQ(tokens[3], "col5");
}

TEST_F(CommonDeepSuiteTest, StringsJoinWithSpecialDelimiters)
{
    std::vector<std::string> parts = { "usr", "local", "bin", "ll-cli" };
    EXPECT_EQ(join(parts, '/'), "usr/local/bin/ll-cli");

    std::vector<std::string> emptyParts = {};
    EXPECT_EQ(join(emptyParts, '/'), "");
}

} // namespace linglong::common::test
