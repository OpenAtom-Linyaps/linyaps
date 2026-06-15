/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/common/cli/repo.h"

#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/utils/gettext.h"
#include "linglong/utils/log/log.h"

#include <algorithm>
#include <cerrno>

namespace linglong::common::cli {

namespace {

std::int64_t getRepoMaxPriority(const api::types::v1::RepoConfigV2 &cfg) noexcept
{
    if (cfg.repos.empty()) {
        return 0;
    }

    auto repo =
      std::max_element(cfg.repos.cbegin(), cfg.repos.cend(), [](const auto &lhs, const auto &rhs) {
          return lhs.priority < rhs.priority;
      });

    return repo->priority;
}

std::string usage(const std::string &programName, const std::string &suffix)
{
    return _("Usage: ") + programName + suffix;
}

} // namespace

CLI::App *addRepoCommand(CLI::App &commandParser,
                         RepoOptions &repoOptions,
                         const std::string &group,
                         const CLI::Validator &validatorString,
                         const std::string &programName)
{
    auto *cliRepo =
      commandParser
        .add_subcommand("repo",
                        _("Display or modify information of the repository currently using"))
        ->group(group);
    cliRepo->usage(usage(programName, " repo SUBCOMMAND [OPTIONS]"));
    cliRepo->require_subcommand(1);

    auto *repoAdd = cliRepo->add_subcommand("add", _("Add a new repository"));
    repoAdd->usage(usage(programName, " repo add [OPTIONS] NAME URL"));
    repoAdd->add_option("NAME", repoOptions.repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);
    repoAdd->add_option("URL", repoOptions.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);
    repoAdd->add_option("--alias", repoOptions.repoAlias, _("Alias of the repo name"))
      ->type_name("ALIAS")
      ->check(validatorString);

    auto *repoModify = cliRepo->add_subcommand("modify", _("Modify repository URL"))->group("");
    repoModify->add_option("--name", repoOptions.repoName, _("Specify the repo name"))
      ->type_name("REPO")
      ->check(validatorString);
    repoModify->add_option("URL", repoOptions.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    auto *repoRemove = cliRepo->add_subcommand("remove", _("Remove a repository"));
    repoRemove->usage(usage(programName, " repo remove [OPTIONS] NAME"));
    repoRemove->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);

    auto *repoUpdate = cliRepo->add_subcommand("update", _("Update the repository URL"));
    repoUpdate->usage(usage(programName, " repo update [OPTIONS] NAME URL"));
    repoUpdate->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);
    repoUpdate->add_option("URL", repoOptions.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    auto *repoSetDefault =
      cliRepo->add_subcommand("set-default", _("Set a default repository name"));
    repoSetDefault->usage(usage(programName, " repo set-default [OPTIONS] NAME"));
    repoSetDefault->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);

    cliRepo->add_subcommand("show", _("Show repository information"))
      ->usage(usage(programName, " repo show [OPTIONS]"));

    auto *repoSetPriority =
      cliRepo->add_subcommand("set-priority", _("Set the priority of the repo"));
    repoSetPriority->usage(usage(programName, " repo set-priority ALIAS PRIORITY"));
    repoSetPriority->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);
    repoSetPriority->add_option("PRIORITY", repoOptions.repoPriority, _("Priority of the repo"))
      ->required()
      ->check(validatorString);

    auto *repoEnableMirror =
      cliRepo->add_subcommand("enable-mirror", _("Enable mirror for the repo"));
    repoEnableMirror->usage(usage(programName, " repo enable-mirror [OPTIONS] ALIAS"));
    repoEnableMirror->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);

    auto *repoDisableMirror =
      cliRepo->add_subcommand("disable-mirror", _("Disable mirror for the repo"));
    repoDisableMirror->usage(usage(programName, " repo disable-mirror [OPTIONS] ALIAS"));
    repoDisableMirror->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);

    return cliRepo;
}

utils::error::Result<void> handleRepoCommand(CLI::App *app,
                                             const RepoOptions &options,
                                             const RepoConfigBackend &backend,
                                             const RepoCommandCallbacks &callbacks)
{
    LINGLONG_TRACE("command repo");

    auto cfg = backend.getConfig();
    if (!cfg) {
        return LINGLONG_ERR(cfg);
    }

    auto argsParsed = [&app](const std::string &name) -> bool {
        return app->get_subcommand(name)->parsed();
    };

    if (argsParsed("show")) {
        if (!callbacks.showConfig) {
            return LINGLONG_ERR("missing repo show callback");
        }

        callbacks.showConfig(*cfg);
        return LINGLONG_OK;
    }

    if (argsParsed("modify")) {
        return LINGLONG_ERR("sub-command 'modify' already has been deprecated, please use "
                            "sub-command 'add' to add a remote repository and use it as default.",
                            EINVAL);
    }

    std::string url = options.repoUrl;

    if (argsParsed("add") || argsParsed("update")) {
        if (url.empty()) {
            return LINGLONG_ERR("url is empty.", EINVAL);
        }

        if (url.rfind("http", 0) != 0) {
            return LINGLONG_ERR("url is invalid: " + url, EINVAL);
        }

        if (url.back() == '/') {
            url.pop_back();
        }
    }

    std::string name = options.repoName;
    std::string alias = options.repoAlias.value_or(name);
    auto &cfgRef = *cfg;

    if (argsParsed("add")) {
        bool isExist =
          std::any_of(cfgRef.repos.begin(), cfgRef.repos.end(), [&alias](const auto &repo) {
              return repo.alias.value_or(repo.name) == alias;
          });
        if (isExist) {
            return LINGLONG_ERR("repo " + alias + " already exist");
        }
        cfgRef.repos.push_back(api::types::v1::Repo{
          .alias = options.repoAlias,
          .name = name,
          .priority = 0,
          .url = url,
        });

        auto ret = backend.setConfig(cfgRef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        return LINGLONG_OK;
    }

    auto existingRepo =
      std::find_if(cfgRef.repos.begin(), cfgRef.repos.end(), [&alias](const auto &repo) {
          return repo.alias.value_or(repo.name) == alias;
      });

    if (existingRepo == cfgRef.repos.end()) {
        return LINGLONG_ERR("the operated repo " + name + " doesn't exist");
    }

    if (argsParsed("remove")) {
        if (cfgRef.repos.size() == 1) {
            return LINGLONG_ERR("repo " + alias
                                + " is the only repo, please add another repo "
                                  "before removing it or update it directly.");
        }
        cfgRef.repos.erase(existingRepo);

        if (cfgRef.defaultRepo == alias) {
            auto maxPriority = getRepoMaxPriority(cfgRef);
            for (auto &repo : cfgRef.repos) {
                if (repo.priority == maxPriority) {
                    cfgRef.defaultRepo = repo.alias.value_or(repo.name);
                    LogI("default repo {} removed, use {} as default repo",
                         alias,
                         cfgRef.defaultRepo);
                    break;
                }
            }
        }

        auto ret = backend.setConfig(cfgRef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        return LINGLONG_OK;
    }

    if (argsParsed("update")) {
        existingRepo->url = url;
        auto ret = backend.setConfig(cfgRef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        return LINGLONG_OK;
    }

    if (argsParsed("enable-mirror")) {
        existingRepo->mirrorEnabled = true;
        auto ret = backend.setConfig(cfgRef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        return LINGLONG_OK;
    }

    if (argsParsed("disable-mirror")) {
        existingRepo->mirrorEnabled = false;
        auto ret = backend.setConfig(cfgRef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        return LINGLONG_OK;
    }

    if (argsParsed("set-default")) {
        if (cfgRef.defaultRepo != alias) {
            cfgRef.defaultRepo = alias;
            auto maxPriority = getRepoMaxPriority(cfgRef);
            for (auto &repo : cfgRef.repos) {
                if (repo.alias.value_or(repo.name) == alias) {
                    repo.priority = maxPriority + 100;
                    break;
                }
            }

            auto ret = backend.setConfig(cfgRef);
            if (!ret) {
                return LINGLONG_ERR(ret);
            }
        }

        return LINGLONG_OK;
    }

    if (argsParsed("set-priority")) {
        existingRepo->priority = options.repoPriority;
        auto ret = backend.setConfig(cfgRef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        return LINGLONG_OK;
    }

    return LINGLONG_ERR("unknown operation");
}

} // namespace linglong::common::cli
