# Linyaps Repository Mirror Function Design

The `enable-mirror` and `disable-mirror` commands have been added to ll-cli and ll-builder to enable and disable mirror functionality.

When mirror functionality is enabled for a repository, the ostree config file is updated to add the contenturl configuration item:

```diff
url=$repo_url/repos/$repo_name
gpg-verify=false
http2=false
+ contenturl=mirrorlist=$repo_url/api/v2/mirrors/$repo_name
```

After adding the contenturl configuration item, ostree will prioritize using contenturl to download static files such as files, while url is only used to obtain metadata like summary. The contenturl supports three protocols: file://, http://, and https://.

If `mirrorlist=` is added before the url, ostree will first obtain a line-separated mirror list from the url, then select mirrors from the mirror list for downloading files. See the [ostree_pull](#ostree-pull-steps) section for specific logic.

/api/v2/mirrors/$repo_name is an API interface of the Linyaps server that obtains the client's country through the client IP, gets the corresponding country's mirror list from the configuration file, and then returns it to ostree. This implements the function of automatically diverting repository file downloads based on the user's country.

## Mirror Site Configuration

Linyaps mirror sites only need to provide https access to repository static files. Linyaps repositories support both rsync and ostree synchronization protocols.

### Using rsync Synchronization Configuration

The advantage is fast synchronization speed, the disadvantage is that it needs to synchronize from mirror sites or official repositories that support the rsync protocol.

```bash
rsync rsync://rsync.linyaps.org.cn/repos/stable $www_root/repos/stable
```

### Using ostree Synchronization Configuration

The advantage is that no protocol support is needed, and repositories can be synchronized from any mirror site. The disadvantage is slower synchronization speed.

Save the following script and name it sync.sh, then execute `sh sync.sh https://mirror-repo-linglong.deepin.com/repos/stable/ $www_root/repos/stable`

```bash
#!/bin/bash
set -e
url=$1
dir=$2
echo sync $url to $dir
sleep 3
ostree init --repo=$dir --mode archive
ostree --repo=$dir remote add --if-not-exists --no-sign-verify remote $url
for ref in $(ostree --repo=$dir remote refs remote); do
    echo pull $ref;
    ostree --repo=$dir pull --mirror $ref;
done
```

## ostree pull Steps

### Determine Mirror Availability

When pulling, ostree first obtains the mirror list from contenturl, then gets the /config file from each url. If the /config file cannot be obtained, the mirror is considered unavailable. If the /config file is obtained, the mirror is considered available. If no mirrors are available, the pull fails.

### Get summary File

ostree gets the summary file from url. If the summary file cannot be obtained, or the summary file does not contain the ref, the pull fails.

### delta-indexes File Acquisition

ostree gets delta-indexes from each available mirror. If the mirror server returns 4xx or 5xx, it gets delta-indexes from the next mirror. If the last mirror returns 5xx, the pull fails. If the last mirror returns 4xx, it skips the delta step and directly pulls files.

### files File Acquisition

ostree gets files from available mirrors in order. If the mirror server returns 403, 404, or 410, the error is considered unrecoverable and the pull fails. If the mirror server returns other error codes, it uses the next mirror to get files. If all mirrors cannot get files, the pull fails.
