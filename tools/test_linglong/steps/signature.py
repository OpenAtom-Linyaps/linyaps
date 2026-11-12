#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — UAB 外部签名数据（linglong.bundle.sign）的提取与落盘。"""

import io
import os
import shutil
import tarfile
import tempfile
from pathlib import Path

from ..constants import DEMO_ARCH
from ..models import StepSkipped

# UAB 里跟"签名"有关的两块东西，名字容易混，含义完全不同：
#   .note.uab.sig       —— 64 个十六进制字符的 SHA-256 摘要，ll-builder export
#                          自己写进去；UABFile::verify() 只做"重算摘要再比对"，
#                          没有密钥、没有外部程序，防的是损坏/篡改，不是来源可信。
#   linglong.bundle.sign —— deepin-elf-verify 那边的旧签名包（一个 tar）。
#                          UABFile::extractSignData() 只把它原样写到
#                          entries/share/deepin-elf-verify/.elfsign/sign.tar 再解开，
#                          真正的验签发生在宿主的 deepin-elf-verify 框架里，不在本仓库。
SIGN_SECTION = "linglong.bundle.sign"
NOTE_SECTION = ".note.uab.sig"
ELF_VERIFY_DIR = "entries/share/deepin-elf-verify/.elfsign"


class SignatureSteps:
    # ── Test: UAB 外部签名数据的提取 ──
    #
    # extractSignData 此前零覆盖：夹具仓库里没有任何 UAB 带 linglong.bundle.sign
    # 段。这一段只做"读段 -> 写 sign.tar -> 外部 tar 解包 -> 落盘 .elfsign"，
    # 所以用自制 tar 当载荷就能把整条路径跑通，不需要真的签名工具。
    #
    # ⚠️ 这条用例的结论只到"签名数据被正确搬运"，不代表"包可信"：
    #    载荷是假的，真实验签由宿主的 deepin-elf-verify 做。以后若上游加了
    #    真 PKI 验签，这条用例会自然变红，正好提醒更新它。
    def test_uab_signature_data(self):
        for tool in ("objcopy", "tar"):
            if shutil.which(tool) is None:
                raise StepSkipped(f"缺少 {tool}，无法构造带签名数据的 UAB")

        workdir = Path(tempfile.mkdtemp(prefix="smokesign-"))
        app_id = f"org.deepin.smokesign{os.getpid()}"
        marker = f"smoke-sign-{os.getpid()}.txt"
        payload = b"fake signature payload: only for covering extractSignData\n"
        uab = workdir / "plain.uab"
        signed_uab = workdir / "signed.uab"
        tampered_uab = workdir / "tampered.uab"
        try:
            # 手写最小工程，不依赖 ll-builder create 的模板（模板里的 base
            # 是 25.2.1，冒烟仓库没有那条线，见 build.py 的 _pin_project_base）。
            (workdir / "linglong.yaml").write_text(
                'version: "1"\n\n'
                "package:\n"
                f"  id: {app_id}\n"
                "  name: smokesign\n"
                "  version: 1.0.0.1\n"
                "  kind: app\n"
                "  description: probe uab sign data extraction\n"
                f"  architecture: {DEMO_ARCH}\n\n"
                "base: org.deepin.base/25.2.2\n\n"
                "command:\n"
                "  - /bin/echo\n\n"
                "build: |\n"
                '  mkdir -p "$PREFIX/bin"\n'
                f"  printf '#!/bin/sh\\necho smokesign\\n' > "
                '"$PREFIX/bin/smokesign"\n'
                '  chmod 755 "$PREFIX/bin/smokesign"\n')

            r = self._ll_builder("build", cwd=str(workdir), timeout=1800,
                                 check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"签名探针构建失败: {(r.stdout + r.stderr)[-400:]}")

            r = self._ll_builder("export", "-o", str(uab), cwd=str(workdir),
                                 timeout=1800, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"签名探针导出失败: {(r.stdout + r.stderr)[-400:]}")

            # 载荷：一个只含 marker 的 tar，结构跟 deepin-elf-verify 的签名包一致。
            # 用 yet-another 假签名段把它塞进 UAB —— objcopy 追加段不会破坏
            # UAB 的 ELF 结构（内部靠段表读 bundle，实测装得上、跑得起来）。
            sign_tar = workdir / "sign.tar"
            with tarfile.open(sign_tar, "w", format=tarfile.USTAR_FORMAT) as tar:
                info = tarfile.TarInfo(marker)
                info.size = len(payload)
                info.mode = 0o644
                tar.addfile(info, io.BytesIO(payload))

            r = self._run_cmd(
                ["objcopy", "--add-section", f"{SIGN_SECTION}={sign_tar}",
                 str(uab), str(signed_uab)],
                check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"给 UAB 追加 {SIGN_SECTION} 段失败: "
                    f"{(r.stdout + r.stderr)[-300:]}")

            # 【负例】先证明摘要确实起保护作用：把 .note.uab.sig 里的摘要
            # 换成 64 个 "0"（仍是合法十六进制，走的是"摘要不匹配"而不是
            # "格式非法"），安装必须失败。
            note_bin = workdir / "note.bin"
            r = self._run_cmd(
                ["objcopy", "--dump-section", f"{NOTE_SECTION}={note_bin}",
                 str(signed_uab)],
                check=False)
            if r.returncode != 0 or not note_bin.exists():
                raise AssertionError(
                    f"取不到 {NOTE_SECTION} 段: {(r.stdout + r.stderr)[-300:]}")

            note_bytes = bytearray(note_bin.read_bytes())
            if len(note_bytes) <= 64:
                raise AssertionError(
                    f"{NOTE_SECTION} 段只有 {len(note_bytes)} 字节，"
                    "结构不符合预期")
            note_bytes[-64:] = b"0" * 64  # 摘要字段是 note 的最后 64 字节
            note_bin.write_bytes(bytes(note_bytes))

            r = self._run_cmd(
                ["objcopy", "--update-section", f"{NOTE_SECTION}={note_bin}",
                 str(signed_uab), str(tampered_uab)],
                check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"改写 {NOTE_SECTION} 段失败: {(r.stdout + r.stderr)[-300:]}")

            self._sudo_ll_cli("uninstall", app_id, timeout=300, check=False)
            r = self._sudo_ll_cli("install", str(tampered_uab), timeout=900,
                                  check=False)
            out = r.stdout + r.stderr
            if r.returncode == 0:
                raise AssertionError(
                    f"摘要被改坏的 UAB 竟然装上了: {out[:300]}")
            if "failed to verify" not in out:
                raise AssertionError(
                    f"摘要被改坏后没报验签失败: {out[:400]}")

            # 【正例】带假签名段的 UAB 应当装成功，并且签名数据落到 layer 的
            # entries/share/deepin-elf-verify/.elfsign/ 下。
            r = self._sudo_ll_cli("install", str(signed_uab), timeout=1800,
                                  check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"带 {SIGN_SECTION} 段的 UAB 安装失败: "
                    f"{(r.stdout + r.stderr)[-400:]}")

            r = self._run_cmd(
                ["find", "/var/lib/linglong/layers", "-name", marker],
                sudo=True, check=False)
            hits = [line for line in (r.stdout or "").splitlines() if line.strip()]
            if not hits:
                raise AssertionError(
                    f"签名数据没落盘：在 /var/lib/linglong/layers 下找不到 {marker}")
            landed = min(hits)  # 同一份数据可能在多个 layer 里，取第一个即可
            if ELF_VERIFY_DIR not in landed:
                raise AssertionError(
                    f"签名数据落到了意外位置: {landed}")

            got = self._run_cmd(["cat", landed], sudo=True, check=False).stdout
            if got != payload.decode():
                raise AssertionError(
                    f"落盘的签名数据内容不对: {got!r}")
        finally:
            self._sudo_ll_cli("uninstall", app_id, timeout=900, check=False)
            shutil.rmtree(workdir, ignore_errors=True)
