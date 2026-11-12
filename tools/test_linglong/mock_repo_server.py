#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""最小化的玲珑仓库服务端，供冒烟测试验证 ll-builder push。

为什么需要它：push 走的是固定的 HTTP JSON 接口（见
api/http/client_swagger.json），而 CI 里没有真实仓库服务端，原来只能测到
"连不上"的失败分支。这个脚本把 push 需要的 4 个接口实现成最小可用版本，
让冒烟能跑通"登录 -> 建上传任务 -> 传 tar -> 轮询状态"的完整成功路径，
也能按需返回错误码或失败状态来覆盖失败分支。

pushToRemote 依次调用：
    POST /api/v1/sign-in                   体 {username,password}   -> {code,data:{token}}
    POST /api/v1/upload-tasks              X-Token，体 {ref,repoName} -> {code,data:{id}}
    PUT  /api/v1/upload-tasks/<id>/tar     X-Token，multipart 字段 file -> {code,data:{watchId}}
    GET  /api/v1/upload-tasks/<id>/status  X-Token -> {code,data:{status}}

控制端点（只给用例用，不属于仓库协议）：
    GET  /__ping    存活检查
    GET  /__state   收到过的请求记录，用例用它断言"push 真的传了东西"
    POST /__mode    切换行为：{"mode": "ok|signin-500|task-500|upload-500|status-failed"}

用法：
    python3 mock_repo_server.py --port 0     # 端口随机时在 stdout 打印 PORT=<n>

为了不引第三方依赖，multipart 用最小解析：只认字段名 file，取它的
payload 和 filename；tar 是否是 gzip 由魔数判断，不做完整解包。
"""

import argparse
import json
import re
import signal
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# 行为模式：ok 是正常成功；其余各自把某个接口做成失败，用来覆盖 pushToRemote
# 里的错误分支。
MODES = {
    "ok": "一切正常",
    "signin-500": "登录返回 code=500（pushToRemote 的 sign error 分支）",
    "task-500": "建上传任务返回 code=500（create task error 分支）",
    "upload-500": "上传文件返回 code=500（upload file error 分支）",
    "status-failed": "轮询到 status=failed（远端处理失败分支）",
}

TOKEN = "mock-token-0123456789abcdef"

_LOCK = threading.Lock()
_STATE = {
    "mode": "ok",
    "calls": [],
    "upload": {"count": 0, "size": 0, "gzip": False, "filename": None},
}


def _record(entry):
    with _LOCK:
        _STATE["calls"].append(entry)


def _mode():
    with _LOCK:
        return _STATE["mode"]


def _parse_multipart_file(body):
    """返回 (filename, payload)；不是 multipart 或没有 file 字段时返回 (None, b"")。"""
    idx = body.find(b'name="file"')
    if idx < 0:
        return None, b""
    head_end = body.find(b"\r\n\r\n", idx)
    if head_end < 0:
        return None, b""
    start = head_end + 4
    end = body.find(b"\r\n--", start)
    if end < 0:
        end = len(body)
    m = re.search(rb'filename="([^"]*)"', body[idx:head_end])
    filename = m.group(1).decode("utf-8", "replace") if m else None
    return filename, body[start:end]


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        if getattr(self.server, "verbose", False):
            sys.stderr.write("mock-repo: " + (fmt % args) + "\n")

    # ── helpers ──
    def _send_json(self, obj, status=200):
        body = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(n) if n else b""

    @staticmethod
    def _parse_json(raw):
        try:
            return json.loads(raw or b"{}")
        except (ValueError, TypeError):
            return {}

    def _write_mode(self, raw):
        data = self._parse_json(raw)
        mode = data.get("mode", "ok")
        if mode not in MODES:
            self._send_json({"error": f"unknown mode {mode!r}"}, status=400)
            return
        with _LOCK:
            _STATE["mode"] = mode
        self._send_json({"mode": mode})

    # ── 控制端点 ──
    def _handle_ping(self):
        self._send_json({"ok": True})

    def _handle_state(self):
        with _LOCK:
            snapshot = json.loads(json.dumps(_STATE))
        self._send_json(snapshot)

    # ── 仓库协议 ──
    def _handle_sign_in(self, raw):
        body = self._parse_json(raw)
        _record(
            {
                "op": "sign-in",
                "username": body.get("username"),
                "password_len": len(body.get("password") or ""),
                # 记下这次发出的 token，用例用它核对后续接口有没有带着它
                "token": TOKEN,
            }
        )
        if _mode() == "signin-500":
            self._send_json({"code": 500, "msg": "invalid username or password"})
            return
        self._send_json({"code": 200, "msg": "", "data": {"token": TOKEN}})

    def _handle_new_task(self, raw):
        body = self._parse_json(raw)
        _record(
            {
                "op": "upload-task",
                "token": self.headers.get("X-Token"),
                "ref": body.get("ref"),
                "repoName": body.get("repoName"),
            }
        )
        if _mode() == "task-500":
            self._send_json({"code": 500, "msg": "repository not found"})
            return
        self._send_json({"code": 200, "msg": "", "data": {"id": "mock-task-1"}})

    def _handle_upload(self, path, raw):
        filename, payload = _parse_multipart_file(raw)
        with _LOCK:
            # 同一轮里前面的负面用例可能也会传文件，统计只累加成功上传的那次
            up = _STATE["upload"]
            up["count"] += 1
            up["size"] = len(payload)
            up["gzip"] = payload[:2] == b"\x1f\x8b"
            up["filename"] = filename
        _record(
            {
                "op": "upload-file",
                "token": self.headers.get("X-Token"),
                "task": path,
                "filename": filename,
                "size": len(payload),
                "gzip": payload[:2] == b"\x1f\x8b",
            }
        )
        if _mode() == "upload-500":
            self._send_json({"code": 500, "msg": "no space left on device"})
            return
        self._send_json({"code": 200, "msg": "", "data": {"watchId": "mock-watch-1"}})

    def _handle_status(self, path):
        _record({"op": "task-status", "token": self.headers.get("X-Token"), "task": path})
        status = "failed" if _mode() == "status-failed" else "complete"
        self._send_json({"code": 200, "msg": "", "data": {"status": status}})

    # ── HTTP 分派 ──
    def do_GET(self):  # noqa: N802 - BaseHTTPRequestHandler 约定
        path = self.path.split("?")[0]
        if path == "/__ping":
            self._handle_ping()
        elif path == "/__state":
            self._handle_state()
        elif re.fullmatch(r"/api/v1/upload-tasks/[^/]+/status", path):
            self._handle_status(path)
        else:
            self._send_json({"error": f"no handler for {path}"}, status=404)

    def do_POST(self):  # noqa: N802
        path = self.path.split("?")[0]
        raw = self._read_body()
        if path == "/__mode":
            self._write_mode(raw)
        elif path == "/api/v1/sign-in":
            self._handle_sign_in(raw)
        elif path == "/api/v1/upload-tasks":
            self._handle_new_task(raw)
        else:
            self._send_json({"error": f"no handler for {path}"}, status=404)

    def do_PUT(self):  # noqa: N802
        path = self.path.split("?")[0]
        raw = self._read_body()
        if re.fullmatch(r"/api/v1/upload-tasks/[^/]+/tar", path):
            self._handle_upload(path, raw)
        else:
            self._send_json({"error": f"no handler for {path}"}, status=404)


def main(argv=None):
    parser = argparse.ArgumentParser(description="玲珑仓库 mock 服务端（仅供冒烟测试）")
    parser.add_argument("--port", type=int, default=0, help="监听端口，0 表示随机")
    parser.add_argument("--verbose", action="store_true", help="把每个请求打到 stderr")
    args = parser.parse_args(argv)

    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.daemon_threads = True
    server.verbose = args.verbose

    def _stop(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, _stop)

    # 冒烟用例靠这一行拿到实际端口（--port 0 时端口是内核分配的）
    print(f"PORT={server.server_address[1]}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
