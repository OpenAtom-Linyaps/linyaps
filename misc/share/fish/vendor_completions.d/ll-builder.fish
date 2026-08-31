# SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: LGPL-3.0-or-later

function __ll_builder_get_repos
    ll-builder repo show 2>/dev/null | tail -n+3 | awk '{print $3 ? $3 : $1}'
end

function __ll_builder_get_refs
    ll-builder list 2>/dev/null
end

function __ll_builder_get_layer_files
    find . -maxdepth 1 -name '*.layer' -printf '%f\n' 2>/dev/null
end

complete -c ll-builder -n "__fish_use_subcommand" -l version

complete -c ll-builder -s h -l help
complete -c ll-builder -l help-all

# 3. Subcommands
set -l cmds create build run export push import extract clean repo remove list
complete -c ll-builder -f -n "__fish_use_subcommand" -a "$cmds"

# 4. Build options
complete -c ll-builder -f -n "__fish_seen_subcommand_from build" -s f -l file -l offline -l isolate-network
complete -c ll-builder -f -n "__fish_seen_subcommand_from build" -l skip-fetch-source -l skip-pull-depend -l skip-run-container -l skip-commit-output -l skip-output-check -l skip-strip-symbols

# 5. Run options
complete -c ll-builder -f -n "__fish_seen_subcommand_from run" -s f -l file -l modules -l workdir -l debug
complete -c ll-builder -f -n "__fish_seen_subcommand_from run" -l extensions -a "(__ll_builder_get_refs)"

# 6. Export options
complete -c ll-builder -f -n "__fish_seen_subcommand_from export" -s f -l file -l icon -l layer -l uabx -l loader -l no-develop -s o -l output -l modules
complete -c ll-builder -f -n "__fish_seen_subcommand_from export" -s z -l compressor -a "lz4 lzma zstd"
complete -c ll-builder -f -n "__fish_seen_subcommand_from export" -l ref -a "(__ll_builder_get_refs)"

# 7. Remove options
complete -c ll-builder -f -n "__fish_seen_subcommand_from remove" -l no-clean-objects
complete -c ll-builder -f -n "__fish_seen_subcommand_from remove" -a "(__ll_builder_get_refs)"

# clean
complete -c ll-builder -f -n "__fish_seen_subcommand_from clean" -s f -l file

# 8. Push options
complete -c ll-builder -f -n "__fish_seen_subcommand_from push" -s f -l file -l repo-url -l repo-name -l module

# 9. Repo subcommands and arguments
complete -c ll-builder -f -n "__fish_seen_subcommand_from repo" -a "add remove update set-default show set-priority enable-mirror disable-mirror"
set -l repo_sub_needs_name remove update set-default set-priority enable-mirror disable-mirror
complete -c ll-builder -f -n "__fish_seen_subcommand_from repo; and __fish_seen_subcommand_from $repo_sub_needs_name" -a "(__ll_builder_get_repos)"
complete -c ll-builder -f -n "__fish_seen_subcommand_from repo; and __fish_seen_subcommand_from enable-mirror" -l region

# 10. File completions for specific commands
complete -c ll-builder -f -n "__fish_seen_subcommand_from import extract" -a "(__ll_builder_get_layer_files)"
