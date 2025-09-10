// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/runtime/wayland_security_ctx.h"

#include "linglong/common/xdg.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"
#include "wayland-security-context-v1.h"

#include <linux/un.h>
#include <sys/eventfd.h>

#include <utility>

#include <sys/socket.h>

namespace linglong::runtime {

WaylandSecurityContextManagerV1::WaylandSecurityContextManagerV1()
    : display(wl_display_connect(nullptr))
{
    if (display == nullptr) {
        throw std::system_error(errno,
                                std::generic_category(),
                                "failed to connect to wayland display");
    }

    auto *registry = wl_display_get_registry(display);
    if (registry == nullptr) {
        throw std::system_error(errno, std::generic_category(), "failed to get wayland registry");
    }
    auto releaseRegistry = linglong::utils::finally::finally([registry]() {
        wl_registry_destroy(registry);
    });

    auto registry_listener = wl_registry_listener{
        [](void *data,
           wl_registry *registry,
           uint32_t id,
           const char *interface,
           uint32_t version) {
            if (std::string_view{ interface } != "wp_security_context_manager_v1") {
                return;
            }

            auto *ret = wl_registry_bind(registry,
                                         id,
                                         &wp_security_context_manager_v1_interface,
                                         std::min(version, 1U));
            if (ret == nullptr) {
                LogE("failed to bind wp_security_context_manager_v1: {}", errno);
            }

            auto *self = static_cast<WaylandSecurityContextManagerV1 *>(data);
            self->manager = static_cast<wp_security_context_manager_v1 *>(ret);
        },
        []([[maybe_unused]] void *data,
           [[maybe_unused]] wl_registry *registry,
           [[maybe_unused]] uint32_t id) {}, // we don't need to handle this
    };

    auto ret = wl_registry_add_listener(registry, &registry_listener, this);
    if (ret != 0) {
        throw std::runtime_error("failed to add listener to registry");
    }

    if (wl_display_roundtrip(display) < 0) {
        throw std::system_error(errno, std::generic_category(), "failed to roundtrip display");
    }

    if (manager == nullptr) {
        throw std::runtime_error("failed to get wp_security_context_manager_v1, maybe compositor "
                                 "doesn't support this protocol");
    }
}

WaylandSecurityContextManagerV1::~WaylandSecurityContextManagerV1()
{
    if (manager != nullptr) {
        wp_security_context_manager_v1_destroy(manager);
        manager = nullptr;
    }

    if (display != nullptr) {
        wl_display_disconnect(display);
        display = nullptr;
    }
}

linglong::utils::error::Result<std::unique_ptr<SecurityContext>>
WaylandSecurityContextManagerV1::createSecurityContext(
  generator::ContainerCfgBuilder &builder) noexcept
{
    LINGLONG_TRACE("create security context");

    auto closeFd = ::eventfd(0, 0);
    if (closeFd == -1) {
        return LINGLONG_ERR("failed to create eventfd for close fd", errno);
    }
    auto releaseCloseFd = linglong::utils::finally::finally([&closeFd]() {
        if (closeFd != -1) {
            ::close(closeFd);
        }
    });

    auto listenFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd == -1) {
        return LINGLONG_ERR("failed to create socket for listen fd", errno);
    }
    auto releaseListenFd = linglong::utils::finally::finally([&listenFd]() {
        if (listenFd != -1) {
            ::close(listenFd);
        }
    });

    auto runtimeDir = linglong::common::getAppXDGRuntimeDir(builder.getAppId());
    auto waylandSocket = runtimeDir / "wayland-socket";

    std::error_code ec;
    std::filesystem::create_directories(waylandSocket.parent_path(), ec);
    if (ec) {
        return LINGLONG_ERR("failed to create directories for socket", ec);
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const auto path = waylandSocket.string();
    std::copy(path.cbegin(), path.cend(), &addr.sun_path[0]);
    addr.sun_path[path.size()] = 0;

    // just in case the socket file exists
    std::filesystem::remove(waylandSocket, ec);
    ec.clear();

    auto releaseIfError = linglong::utils::finally::finally([&waylandSocket]() {
        if (!waylandSocket.empty()) {
            std::error_code ec;
            std::filesystem::remove(waylandSocket, ec);
            if (ec) {
                LogE("failed to remove socket: {}", ec.message());
            }
        }
    });

    auto ret = ::bind(listenFd,
                      reinterpret_cast<const struct sockaddr *>(&addr),
                      offsetof(sockaddr_un, sun_path) + path.size());
    if (ret == -1) {
        return LINGLONG_ERR("failed to bind socket for listen fd", errno);
    }

    ret = ::listen(listenFd, SOMAXCONN);
    if (ret == -1) {
        return LINGLONG_ERR("failed to listen on socket", errno);
    }

    auto *context = wp_security_context_manager_v1_create_listener(manager, listenFd, closeFd);
    if (context == nullptr) {
        return LINGLONG_ERR("failed to create wayland security context listener", errno);
    }
    auto releaseContext = linglong::utils::finally::finally([context]() {
        wp_security_context_v1_destroy(context);
    });

    wp_security_context_v1_set_app_id(context, builder.getAppId().c_str());
    wp_security_context_v1_set_sandbox_engine(context, "cn.org.linyaps");
    wp_security_context_v1_set_instance_id(context, builder.getContainerId().c_str());
    wp_security_context_v1_commit(context);
    if (wl_display_roundtrip(display) < 0) {
        return LINGLONG_ERR("failed to roundtrip display", errno);
    }

    WaylandSecurityContextV1 *rawCtx{ nullptr };
    try {
        rawCtx = new WaylandSecurityContextV1(listenFd, closeFd, std::move(waylandSocket));
    } catch (const std::runtime_error &e) {
        auto msg = std::string{ "failed to create wayland security context:" } + e.what();
        return LINGLONG_ERR(msg.c_str());
    }

    auto secCtx = std::unique_ptr<SecurityContext>(rawCtx);

    closeFd = -1;
    listenFd = -1;

    return secCtx;
}

WaylandSecurityContextV1::WaylandSecurityContextV1(int listenFd,
                                                   int closeFd,
                                                   std::filesystem::path socketPath)
    : socketPath(std::move(socketPath))
    , listenFd(listenFd)
    , closeFd(closeFd)
{
}

WaylandSecurityContextV1::~WaylandSecurityContextV1()
{
    if (listenFd != -1) {
        ::close(listenFd);
        listenFd = -1;
    }

    if (closeFd != -1) {
        ::close(closeFd);
        closeFd = -1;
    }

    if (!socketPath.empty()) {
        std::error_code ec;
        std::filesystem::remove(socketPath, ec);
        if (ec) {
            LogE("failed to remove socket path: {}", ec.message());
        }

        socketPath.clear();
    }
}

linglong::utils::error::Result<void>
WaylandSecurityContextV1::apply(generator::ContainerCfgBuilder &builder) noexcept
{
    LINGLONG_TRACE("attach security context");

    builder.setAnnotation(generator::ANNOTATION::WAYLAND_SOCKET, socketPath.string());
    builder.bindWaylandSocket(socketPath);
    return LINGLONG_OK;
}

} // namespace linglong::runtime
