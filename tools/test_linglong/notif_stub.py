#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""桩通知服务：own org.freedesktop.Notifications，供冒烟测试驱动交互确认。

ll-cli 在【非 TTY】下需要用户确认时（ref_installation.cpp 的
ActionOperation::Policy::Upgrade 分支），会通过 org.freedesktop.Notifications
发一条带按钮的通知，然后阻塞等两个信号：

  * ActionInvoked(id, action)        —— 用户点了哪个按钮
  * NotificationClosed(id, reason)   —— 通知关闭的原因

⚠️ 两个信号都必须发，而且 reason 必须是 Dismissed(2) 或 CloseByCall(3)：
   cli/dbus_notifier.cpp 的 request() 只在收到 NotificationClosed 时才退出
   事件循环，且 reason 为 Expired(1)/Undefined(4) 时会走到
   LogE("unexpected reason") + std::abort()。

用法: notif_stub.py <action|none> <log> [delay]

  action  ActionInvoked 里回的动作名（Y / N / ...）；none = 只记日志不回应
  log     每收到一条通知追加一行 JSON（summary / body / actions / hints）
  delay   收到通知后多少秒回 ActionInvoked，默认 0.5
"""

import json
import os
import sys

import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

ACTION = sys.argv[1] if len(sys.argv) > 1 else "Y"
LOG = sys.argv[2] if len(sys.argv) > 2 else "/tmp/linglong-notif-stub.jsonl"
DELAY = float(sys.argv[3]) if len(sys.argv) > 3 else 0.5

# 关闭原因，取值同 org.freedesktop.Notifications 规范 / CloseReason 枚举
REASON_DISMISSED = 2
REASON_CLOSE_BY_CALL = 3


class Notifications(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, "/org/freedesktop/Notifications")
        self.next_id = 1

    @dbus.service.method("org.freedesktop.Notifications",
                         in_signature="susssasa{sv}i", out_signature="u")
    def Notify(self, app_name, replaces_id, app_icon, summary, body,
               actions, hints, expire_timeout):
        nid = self.next_id
        self.next_id += 1
        try:
            with open(LOG, "a") as f:
                f.write(json.dumps({
                    "id": nid,
                    "app_name": app_name,
                    "summary": summary,
                    "body": body,
                    "actions": list(actions),
                    "hints": {str(k): str(v) for k, v in hints.items()},
                    "expire_timeout": int(expire_timeout),
                }, ensure_ascii=False) + "\n")
        except OSError:
            pass
        if ACTION and ACTION != "none":
            GLib.timeout_add(int(DELAY * 1000), self._fire, nid)
        return dbus.UInt32(nid)

    def _fire(self, nid):
        self.ActionInvoked(dbus.UInt32(nid), ACTION)
        GLib.timeout_add(150, self._close, nid)
        return False

    def _close(self, nid):
        self.NotificationClosed(dbus.UInt32(nid), dbus.UInt32(REASON_DISMISSED))
        return False

    @dbus.service.signal("org.freedesktop.Notifications", signature="us")
    def ActionInvoked(self, nid, action):
        pass

    @dbus.service.signal("org.freedesktop.Notifications", signature="uu")
    def NotificationClosed(self, nid, reason):
        pass

    @dbus.service.method("org.freedesktop.Notifications",
                         in_signature="u", out_signature="")
    def CloseNotification(self, nid):
        self.NotificationClosed(dbus.UInt32(nid),
                                dbus.UInt32(REASON_CLOSE_BY_CALL))

    @dbus.service.method("org.freedesktop.Notifications",
                         in_signature="", out_signature="as")
    def GetCapabilities(self):
        return ["actions", "body", "persistence"]

    @dbus.service.method("org.freedesktop.Notifications",
                         in_signature="", out_signature="ssss")
    def GetServerInformation(self):
        return ("linglong-smoke-notif-stub", "linglong", "1.0", "1.2")


def main():
    DBusGMainLoop(set_as_default=True)

    # ⚠️ 必须用 DBUS_SESSION_BUS_ADDRESS 显式建连接，不能用
    #    dbus.SessionBus()：实测 SessionBus() 在本机（root 身份）连到了
    #    另一条总线上，桩打印 "ready" 但那条总线上根本没有
    #    org.freedesktop.Notifications 的 owner，ll-cli 调用
    #    GetCapabilities 直接报 ServiceUnknown。
    address = os.environ.get("DBUS_SESSION_BUS_ADDRESS")
    if not address:
        print("缺少 DBUS_SESSION_BUS_ADDRESS", file=sys.stderr, flush=True)
        return 1
    bus = dbus.bus.BusConnection(address)

    # ⚠️ 必须把 BusName 存下来：不持有引用的话它会被 GC 掉，析构时
    #    【释放】这个名字 —— 桩照样打印 "ready"，但总线上已经没有
    #    org.freedesktop.Notifications 的 owner，ll-cli 调
    #    GetCapabilities 直接报 ServiceUnknown，交互被判成 no。
    #    实测踩过，现象是"桩收到了通知"永远是 0 条。
    name = dbus.service.BusName("org.freedesktop.Notifications", bus,
                                do_not_queue=True)
    Notifications(bus)
    print("stub ready action=%s name=%s unique=%s address=%s"
          % (ACTION, name.get_name(), bus.get_unique_name(), address),
          flush=True)
    GLib.MainLoop().run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
