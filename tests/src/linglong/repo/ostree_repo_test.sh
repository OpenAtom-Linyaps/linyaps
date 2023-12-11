#!/bin/bash

set -e
set -x

create_files() {
        mkdir -p tmp
        files=(tmp/test_file.txt)
        for file in "${files[@]}"; do
                touch "$file"
        done
        find tmp -type f | sed 's/^tmp\///g'
}

cleanup() {
        rm -rf tmp
}

check_commit() {
        repo=$1
        shift
        ref=$1
        shift
        file=$1
        shift

        ostree --repo="$repo" ls "$ref" "$file" --nul-filenames-only | strings
}

check_files() {
        for file in "$@"; do
                test -f ."$file"
        done
}

"$@"
