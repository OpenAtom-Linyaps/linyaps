#!/usr/bin/env python3
#


import os
import sys
import os.path
import argparse
import subprocess
import tempfile
import paramiko
import json
import tarfile
import shutil
from jinja2 import Environment, BaseLoader

repo_sh = """
#!/bin/bash
set +x
LINGLONG_ROOT_REMOTE={{linglong_path}}

# pre_func
function pre_func(){
echo "pre"
if [ -f {{remote_path}}/{{repo_tar}} ]
then
echo "{% for x in range(10) %}-{% endfor %}"
    tar xf {{remote_path}}/{{repo_tar}} -C {{remote_path}}
    echo "prepare tarfile OK!"
echo "{% for x in range(10) %}-{% endfor %}"
else
echo "{% for x in range(10) %}-{% endfor %}"
    echo "prepare tarfile failed!"
    exit 1
echo "{% for x in range(10) %}-{% endfor %}"
fi
}

# start_func
function start_func(){
echo "{% for x in range(10) %}-{% endfor %}"
echo "start copy data"
cp  -vf {{remote_path}}/*.uab ${LINGLONG_ROOT_REMOTE}/uab/
if [ -f ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json ]
then
    mv -fv ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json.old.1 ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json.old.2
    mv -fv ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json.old ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json.old.1
    mv ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json.old
    cp  -vf {{remote_path}}/AppStream.json ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json
else
cp  -vf {{remote_path}}/AppStream.json ${LINGLONG_ROOT_REMOTE}/xml/AppStream.json
fi
echo "{% for x in range(10) %}-{% endfor %}"
ostree --repo=${LINGLONG_ROOT_REMOTE}/pool/repo pull-local {{remote_path}}/repo/

if [ $? -eq 0 ]
then
echo "{% for x in range(10) %}-{% endfor %}"
    echo "Pull Repo Ok!"
else
echo "{% for x in range(10) %}-{% endfor %}"
    echo "Pull Repo Failed!"
fi
echo "{% for x in range(10) %}-{% endfor %}"

ostree --repo=${LINGLONG_ROOT_REMOTE}/pool/repo summary -u
if [ $? -eq 0 ]
then
echo "{% for x in range(10) %}-{% endfor %}"
    echo "Upload Ok!"
else
echo "{% for x in range(10) %}-{% endfor %}"
    echo "Upload Failed!"
fi
echo "{% for x in range(10) %}-{% endfor %}"
}

# end_func
function end_func(){
echo "end"
rm -rf {{remote_path}}
}
echo "run pre"
echo "{% for x in range(10) %}#{% endfor %}"
pre_func
echo "{% for x in range(10) %}#{% endfor %}"
echo "run start"
start_func
echo "{% for x in range(10) %}#{% endfor %}"
echo "run end"
#end_func
echo "{% for x in range(10) %}#{% endfor %}"

"""
remote = {
    "ip": "repo.linglong.space",
    "port": 10022,
    "username": "linglong"
}

default_path = os.path.join(os.environ["HOME"], ".ssh", "id_rsa")

ssh_key_file = paramiko.RSAKey.from_private_key_file(default_path)


def run_command_remote(command):
    ssh = paramiko.SSHClient()

    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    ssh.connect(hostname=remote['ip'],
                port=remote['port'],
                username=remote['username'])

    cmd = " ".join(command)

    print("run: {0}".format(cmd))

    stdin, stdout, stderr = ssh.exec_command(cmd)
    # import pdb;pdb.set_trace()
    print(" ".join(stdout.readlines()))
    ssh.close()


def push_files_remote(local_files, remote_dir):
    transpt = paramiko.Transport((remote['ip'], remote['port']))

    transpt.connect(username=remote['username'], password=None, pkey=ssh_key_file)

    if not transpt.is_authenticated():
        return False

    sftp = paramiko.SFTPClient.from_transport(transpt)

    for fs in local_files:
        if not fs:
            continue
        print("upload: {0}".format(fs))
        sftp.put(fs, "{0}/{1}".format(remote_dir, os.path.basename(fs)))
    transpt.close()


def fetch_files_remote(remote_files, local_dir):
    transpt = paramiko.Transport((remote['ip'], remote['port']))

    transpt.connect(username=remote['username'], password=None, pkey=ssh_key_file)

    if not transpt.is_authenticated():
        return False

    sftp = paramiko.SFTPClient.from_transport(transpt)

    for fs in remote_files:
        print("download: {0}".format(fs))
        sftp.get(fs, "{0}/{1}".format(local_dir, os.path.basename(fs)))
    transpt.close()


def made_remote_shell(remote_path, repo_tar, linglong_path="/home/linglong/work/linglong"):
    rtemplate = Environment(loader=BaseLoader).from_string(repo_sh)
    remote_env = {"remote_path": remote_path,
                  "repo_tar": os.path.basename(repo_tar),
                  "linglong_path": linglong_path}
    data = rtemplate.render(**remote_env)
    with open("pkg.sh", "w") as f:
        f.write(data)
    return "pkg.sh"


def remote_lock():
    run_command_remote(["/usr/bin/flock", "-x", "/tmp/linglong-repo.lock", "-c",
                        """'while true ; do if [  -f /tmp/.linglong-repo-lock ]; then  echo 'wait' ; sleep 1; else  break ; fi;  done'"""])
    run_command_remote(["touch", "/tmp/.linglong-repo-lock"])


def remote_unlock():
    run_command_remote(["rm", "-f", "/tmp/.linglong-repo-lock"])


# TODO(fix): if not fetch arch info default return x86_64 be keep had arch info
def get_bundle_arch(info_file):
    if not os.path.exists(info_file):
        return "x86_64"
    with open(info_file, "r") as fd_json:
        info_json = json.load(fd_json)
    if not info_json:
        return "x86_64"
    return info_json["arch"][0]

# get  info
def get_bundle_info(info_file):
    # read  info
    print(info_file)
    if not os.path.exists(info_file):
        return {}

    bundle_info = {}
    with open(info_file, "r") as fd_json:
        info_json = json.load(fd_json)
    if not info_json:
        return {}
    bundle_info["appid"] = info_json["appid"]
    bundle_info["name"] = info_json["name"]
    bundle_info["version"] = info_json["version"]
    bundle_info["summary"] = info_json["description"]
    bundle_info["runtime"] = info_json["runtime"]
    bundle_info["reponame"] = "repo"

    return bundle_info

def update_app_stream(info_files="info.json",appstream_path="AppStream.json"):
    if not info_files:
        return False
    if not os.path.exists(info_files) or not os.path.exists(appstream_path):
        return False
    with open(appstream_path, "r") as fd_json:
        app_json = json.load(fd_json)
    bundle_info = {}
    if not app_json:
        app_json = {}
        bundle_info = get_bundle_info(info_files)
        if not bundle_info:
            return False
        app_key = "{0}_{1}".format(bundle_info["appid"], bundle_info["version"])
        app_arch = get_bundle_arch(info_files)
        app_json[app_key] = bundle_info
        app_json[app_key]["arch"] = []
        app_json[app_key]["arch"].append(app_arch)
    else:
        bundle_info = get_bundle_info(info_files)
        if not bundle_info:
            return False
        app_key = "{0}_{1}".format(bundle_info["appid"], bundle_info["version"])
        app_arch = get_bundle_arch(info_files)
        if app_key in app_json:
            if not app_arch in app_json[app_key]["arch"]:
                app_json[app_key]["arch"].append(app_arch)
            bundle_info["arch"] = app_json[app_key]["arch"]
            app_json[app_key] = bundle_info
        else:
            app_json[app_key] = bundle_info
            app_json[app_key]["arch"] = []
            app_json[app_key]["arch"].append(app_arch)

    # update appstream
    with open(appstream_path, "w",encoding='utf-8') as fd_w:
        fd_w.write(json.dumps(app_json, ensure_ascii=False,indent=4))
    return True


def run_main(root_args):
    # mktemp work path
    linglong_work_path = tempfile.mkdtemp(prefix="linglong-")
    print(linglong_work_path)

    # render template to shell
    remote_shell = made_remote_shell(linglong_work_path, root_args.repo_tar, root_args.linglong_remote_path)

    # init remote
    if root_args.force:
        print("force unlock remote lock")
        remote_unlock()
    remote_lock()  # lock remote
    #fetch appstream.json file
    fetch_files_remote(["{0}/xml/AppStream.json".format(root_args.linglong_remote_path)], "./")
    update_app_stream(root_args.info_file)
    run_command_remote(["mkdir", "-pv", "{0}".format(linglong_work_path)])

    # upload data
    push_files_remote([remote_shell,
                       "./AppStream.json",
                       root_args.repo_tar,
                       root_args.bundle_file], linglong_work_path)
    # run shell
    run_command_remote(["bash", "-x", "{0}/{1}".format(linglong_work_path, remote_shell)])

    # clean
    run_command_remote(["rm", "-rf", "{0}".format(linglong_work_path)])
    remote_unlock()  # unlock remote
    shutil.rmtree(linglong_work_path)
    # print("Upload OK!")

def check_file_exists(parser, file):
    if not os.path.exists(file):
        parser.error("Enter File {0} Not Found! Please Retry! ".format(file))
    else:
        return file


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='import package to linglong repo.')
    # repo tar
    parser.add_argument('--repo_tar', "-r",
                        dest='repo_tar',
                        action="store",
                        required=False,
                        type=lambda f: check_file_exists(parser, f),
                        help='repo tar file')

    # input repo path
    parser.add_argument('--repo_path', "-d",
                        dest='repo_path',
                        action="store",
                        required=False,
                        type=lambda f: check_file_exists(parser, f),
                        help='repo path directory') 

    # bundle
    # TODO(fix): support bundle
    parser.add_argument('--bundle', "-b",
                        dest='bundle_file',
                        action="store",
                        required=False,
                        type=lambda f: check_file_exists(parser, f),
                        help='bundle file')

    # info.json
    parser.add_argument('--info', "-i",
                        dest='info_file',
                        action="store",
                        required=False,
                        type=lambda f: check_file_exists(parser, f),
                        help='info file')

    parser.add_argument('--force', "-f",
                        dest='force',
                        action="store_true",
                        required=False,
                        default=False,
                        help='force upload and override remote lock')
    # remote path
    parser.add_argument('--remote_path',
                        dest='linglong_remote_path',
                        action="store",
                        required=False,
                        default="/data/linglong/linglong.repo",
                        help='linglong remote path')
    # remote url
    parser.add_argument('--remote_user',
                        dest='ssh_remote_user',
                        action="store",
                        required=False,
                        default="linglong",
                        help='remote ssh user name')

    parser.add_argument('--remote_port',
                        dest='ssh_remote_port',
                        action="store",
                        required=False,
                        default=10022,
                        help='remote ssh port')

    parser.add_argument('--remote_ip',
                        dest='ssh_remote_ip',
                        action="store",
                        required=False,
                        default="repo.linglong.space",
                        help='remote ssh ip address')

    root_args = parser.parse_args()
    remote["ip"] = root_args.ssh_remote_ip
    remote["port"] = root_args.ssh_remote_port
    remote["username"] = root_args.ssh_remote_user
    if not root_args.repo_tar:
        if not root_args.repo_path:
            print("Need Repo param")
            parser.print_help()
            sys.exit(2)
        else:
            repo_path_file_list = os.listdir(root_args.repo_path)
            research = ["config", "state", "extensions", "objects", "refs"]
            search_idx = 0
            for rpfs in repo_path_file_list:
                #print(os.path.basename(rpfs))
                if rpfs in research:
                    search_idx = search_idx + 1
                    if (rpfs == "refs" and (not os.path.isdir("{0}/{1}".format(root_args.repo_path, rpfs)))) or (
                            rpfs == "config" and (not os.path.isfile("{0}/{1}".format(root_args.repo_path, rpfs)))):
                        print("Not Found Repo Data")
                        sys.exit(1)
            if search_idx != len(research):
                print("Incorrect Repo Data")
                sys.exit(3)
            repo_tar_new = tarfile.open("repo.tar","w")
            repo_tar_new.add(root_args.repo_path,arcname='repo')
            repo_tar_new.close()
            root_args.repo_tar = "./repo.tar"
    print("opt: {0}".format(root_args))
    #sys.exit(2)
    run_main(root_args)
