// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/builder/builder_script.h"

#include <filesystem>
#include <fstream>

using namespace linglong::builder;

TEST(BuilderScript, UsesInstalledFileThroughSymlinkedBinDirectory)
{
    TempDir temporary;
    const auto installedBinDir = temporary.path() / "installed/bin";
    std::filesystem::create_directories(installedBinDir);
    const auto binAlias = temporary.path() / "bin";
    std::error_code ec;
    std::filesystem::create_directory_symlink(installedBinDir, binAlias, ec);
    ASSERT_FALSE(ec) << ec.message();

    const auto installedFile = temporary.path() / "installed-template.yaml";
    std::ofstream(installedFile) << "id: @ID@\n";
    const auto buildRelative = "misc/share/linglong/builder/templates/example.yaml";
    const auto buildFile = temporary.path() / buildRelative;
    std::filesystem::create_directories(buildFile.parent_path());
    std::ofstream(buildFile) << "unrelated build template\n";

    auto found = detail::findBuilderFileForExecutable(binAlias / "ll-builder",
                                                      buildRelative,
                                                      installedFile,
                                                      installedBinDir);
    ASSERT_TRUE(found.has_value()) << found.error().message();
    EXPECT_EQ(*found, installedFile);

    std::filesystem::remove(installedFile);
    EXPECT_FALSE(detail::findBuilderFileForExecutable(binAlias / "ll-builder",
                                                      buildRelative,
                                                      installedFile,
                                                      installedBinDir)
                   .has_value());
}

TEST(BuilderScript, FindsScriptsAndTemplateInBuilderBuildLayout)
{
    TempDir temporary;
    const auto build = temporary.path() / "build";
    const auto scripts = build / "misc/libexec/linglong";
    const auto sourceTemplate = build / "misc/share/linglong/builder/templates/example.yaml";
    std::filesystem::create_directories(scripts);
    std::filesystem::create_directories(sourceTemplate.parent_path());
    std::ofstream(scripts / "fetch-git-source") << "#!/bin/sh\n";
    std::ofstream(scripts / "app-conf-generator") << "#!/bin/bash\n";
    std::ofstream(sourceTemplate) << "id: @ID@\n";

    const auto executable = build / "apps/ll-builder/ll-builder";
    auto fetch = detail::findBuilderFileForExecutable(executable,
                                                      "misc/libexec/linglong/fetch-git-source",
                                                      {});
    auto appConf = detail::findBuilderFileForExecutable(executable,
                                                        "misc/libexec/linglong/app-conf-generator",
                                                        {});
    auto sourceTemplateResult =
      detail::findBuilderFileForExecutable(executable,
                                           "misc/share/linglong/builder/templates/example.yaml",
                                           {});
    ASSERT_TRUE(fetch.has_value()) << fetch.error().message();
    ASSERT_TRUE(appConf.has_value()) << appConf.error().message();
    ASSERT_TRUE(sourceTemplateResult.has_value()) << sourceTemplateResult.error().message();
    EXPECT_EQ(*fetch, scripts / "fetch-git-source");
    EXPECT_EQ(*appConf, scripts / "app-conf-generator");
    EXPECT_EQ(*sourceTemplateResult, sourceTemplate);
}

TEST(BuilderScript, FindsScriptForAnotherExecutableName)
{
    TempDir temporary;
    const auto build = temporary.path() / "build";
    const auto scripts = build / "misc/libexec/linglong";
    std::filesystem::create_directories(scripts);
    std::ofstream(scripts / "fetch-git-source") << "#!/bin/sh\n";

    const auto executable = build / "tools/check-sources/check-sources";
    auto script = detail::findBuilderFileForExecutable(executable,
                                                       "misc/libexec/linglong/fetch-git-source",
                                                       {});
    ASSERT_TRUE(script.has_value()) << script.error().message();
    EXPECT_EQ(*script, scripts / "fetch-git-source");
}

TEST(BuilderScript, FindsScriptInTestBuildLayout)
{
    TempDir temporary;
    const auto build = temporary.path() / "build";
    const auto scripts = build / "misc/libexec/linglong";
    std::filesystem::create_directories(scripts);
    std::ofstream(scripts / "fetch-git-source") << "#!/bin/sh\n";

    const auto executable = build / "libs/linglong/tests/ll-tests/ll-tests";
    auto script = detail::findBuilderFileForExecutable(executable,
                                                       "misc/libexec/linglong/fetch-git-source",
                                                       {});
    ASSERT_TRUE(script.has_value()) << script.error().message();
    EXPECT_EQ(*script, scripts / "fetch-git-source");
}

TEST(BuilderScript, FindsConfiguredFilesForRunningTest)
{
    auto script = findBuilderScript("app-conf-generator");
    auto sourceTemplate = findBuilderTemplate();
    ASSERT_TRUE(script.has_value()) << script.error().message();
    ASSERT_TRUE(sourceTemplate.has_value()) << sourceTemplate.error().message();
    EXPECT_EQ(script->filename(), "app-conf-generator");
    EXPECT_EQ(sourceTemplate->filename(), "example.yaml");
}

TEST(BuilderScript, PrefersClosestAncestorWithRequestedScript)
{
    TempDir temporary;
    const auto outerScript = temporary.path() / "misc/libexec/linglong/fetch-git-source";
    const auto build = temporary.path() / "build";
    const auto innerScript = build / "misc/libexec/linglong/fetch-git-source";
    std::filesystem::create_directories(outerScript.parent_path());
    std::filesystem::create_directories(innerScript.parent_path());
    std::ofstream(outerScript) << "#!/bin/sh\n";
    std::ofstream(innerScript) << "#!/bin/sh\n";

    const auto executable = build / "apps/ll-builder/ll-builder";
    auto script = detail::findBuilderFileForExecutable(executable,
                                                       "misc/libexec/linglong/fetch-git-source",
                                                       {});
    ASSERT_TRUE(script.has_value()) << script.error().message();
    EXPECT_EQ(*script, innerScript);
}

TEST(BuilderScript, SearchesBeyondKnownBuildLayouts)
{
    TempDir temporary;
    const auto script = temporary.path() / "misc/libexec/linglong/fetch-git-source";
    std::filesystem::create_directories(script.parent_path());
    std::ofstream(script) << "#!/bin/sh\n";

    const auto executable = temporary.path() / "nested/bin/tools/ll-builder";
    auto found = detail::findBuilderFileForExecutable(executable,
                                                      "misc/libexec/linglong/fetch-git-source",
                                                      {});
    ASSERT_TRUE(found.has_value()) << found.error().message();
    EXPECT_EQ(*found, script);
}

TEST(BuilderScript, RejectsMissingScriptAndInvalidName)
{
    TempDir temporary;
    const auto executable = temporary.path() / "build/apps/ll-builder/ll-builder";
    EXPECT_FALSE(
      detail::findBuilderFileForExecutable(executable, "misc/libexec/linglong/fetch-git-source", {})
        .has_value());
    EXPECT_FALSE(findBuilderScript("../fetch-git-source").has_value());
}
