#!/bin/bash 

set -e 

# 仓库已经创建 
workDir=`pwd`
testRef=linglong/test_0/latest/x86_64/runtime

create_commit() {
    commit_dir=$workDir/commit_test_0
    mkdir $commit_dir
    commit_file="commit"
    touch ${commit_dir}/${commit_file}
    output=`ostree commit --repo=$workDir/repo-test/repo -b $testRef $commit_dir`
}

test_importdirectory() {
    # $1 repo path
    # $2 refs
    # $3 commit file name 

    ostree --repo=$1 ls $2 $3
}

test_checkout() {
    ls "$1"
}

test_remoteadd() {
    ostree remote show-url --repo=$1 $2
}

test_pull() {
    ls $1/repo/refs/remotes/$2/$3
}


"$@"