# SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: LGPL-3.0-or-later

function __fish_ll_cli_get_containers
    ll-cli ps 2>/dev/null | awk 'NR>1 {print $1}'
end

function __fish_ll_cli_get_installed_apps
    ll-cli list --type=app 2>/dev/null | awk 'NR>1 {print $1}'
end

function __fish_ll_cli_get_installed_all
    ll-cli list 2>/dev/null | awk 'NR>1 {print $1}'
end

function __fish_ll_cli_get_repo_aliases
    ll-cli repo show 2>/dev/null | awk 'NR>2 {print $3}'
end

complete -c ll-cli -f

# version
complete -c ll-cli -n "__fish_use_subcommand" -l version

# 全局选项
set -l global_long_opts help help-all json verbose no-progress
set -l global_short_opts v

for opt in $global_long_opts
    complete -c ll-cli -l $opt
end
for opt in $global_short_opts
    complete -c ll-cli -s $opt
end

# --- 一级子命令定义 ---
set -l subcommands run ps enter kill install uninstall upgrade search list repo info content prune
complete -c ll-cli -n "__fish_use_subcommand" -a "$subcommands"

# --- 子命令参数补全逻辑 ---

# run, enter, kill
complete -c ll-cli -n "__fish_seen_subcommand_from run" -l file -l url -l env -l base -l runtime -l extensions
complete -c ll-cli -n "__fish_seen_subcommand_from run" -a "(__fish_ll_cli_get_installed_apps)"
complete -c ll-cli -n "__fish_seen_subcommand_from enter" -l working-directory
complete -c ll-cli -n "__fish_seen_subcommand_from kill" -s s -l signal -xa "SIGTERM SIGKILL SIGINT SIGHUP"
complete -c ll-cli -n "__fish_seen_subcommand_from enter kill" -a "(__fish_ll_cli_get_containers)"

# install / uninstall / upgrade
complete -c ll-cli -n "__fish_seen_subcommand_from install" -l module -l repo -l force -s y
complete -c ll-cli -n "__fish_seen_subcommand_from install" -k -a "(__fish_complete_suffix .layer .uab)"
complete -c ll-cli -n "__fish_seen_subcommand_from uninstall" -l module -l force
complete -c ll-cli -n "__fish_seen_subcommand_from uninstall upgrade" -a "(__fish_ll_cli_get_installed_apps)"

# search / list
complete -c ll-cli -n "__fish_seen_subcommand_from search list" -l type -xa "runtime base app all"
complete -c ll-cli -n "__fish_seen_subcommand_from search" -l repo -l dev -l show-all-version
complete -c ll-cli -n "__fish_seen_subcommand_from list" -l upgradable

# repo
set -l repo_subs add remove update set-default show set-priority enable-mirror disable-mirror
complete -c ll-cli -n "__fish_seen_subcommand_from repo; and not __fish_seen_subcommand_from $repo_subs" -a "$repo_subs"
complete -c ll-cli -n "__fish_seen_subcommand_from repo; and __fish_seen_subcommand_from add" -l alias
complete -c ll-cli -n "__fish_seen_subcommand_from repo; and __fish_seen_subcommand_from remove update set-default set-priority enable-mirror disable-mirror" -a "(__fish_ll_cli_get_repo_aliases)"

# info 和 content 支持已安装的 app/runtime
complete -c ll-cli -n "__fish_seen_subcommand_from info content" -a "(__fish_ll_cli_get_installed_all)"

# info 额外支持补全当前目录下的 .layer 文件 (核心修复)
complete -c ll-cli -n "__fish_seen_subcommand_from info" -k -a "(__fish_complete_suffix .layer)"
