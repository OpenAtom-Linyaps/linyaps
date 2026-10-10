// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <tuple>

namespace linglong::service {

// Identity of a package manager caller that survives a restart of the client.
// `ll-cli` starts a new process for every operation, so its bus name, its pid
// and its process start time keep changing while the user and its login session
// stay the same.
struct PolkitCallerIdentity
{
    std::uint32_t uid{ 0 };
    std::string sessionId;

    // A caller without a login session can not be used as a cache key: without
    // the session part a grant would leak into another session of the same user.
    [[nodiscard]] bool isValid() const noexcept { return !sessionId.empty(); }
};

// polkit only retains an authorization that was obtained through a `*_keep`
// policy, so only those actions may ever be served from the cache below.
// Actions declared with a plain `auth_admin` policy, such as prune and
// set-configuration, have to be re-authenticated for every call and are
// therefore deliberately absent.
//
// Keep this list in sync with the `allow_active` values of
// misc/share/polkit-1/actions/org.deepin.linglong.PackageManager1.policy, the
// KeepActionListMatchesPolicy unit test reads that file and fails as soon as
// the two disagree.
[[nodiscard]] bool isKeepAuthorizedAction(const std::string &actionId) noexcept;

// Remembers successful polkit checks so that a series of package manager calls
// from the same session does not authenticate again and again.
//
// It never replaces the policy: isKeepAuthorizedAction() restricts the cache to
// the actions whose policy already allows the authorization to be kept.
class PolkitAuthorizationCache
{
public:
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;

    // polkit keeps a retained authorization for a short period of time, five
    // minutes in the current implementation. Mirror that window instead of
    // picking a longer one, otherwise the daemon would keep accepting calls that
    // polkit itself would already reject again.
    static constexpr auto defaultKeepDuration = std::chrono::minutes(5);

    // Upper bound of remembered grants. Entries are tiny, the limit only keeps
    // the map from growing without bound on a long running daemon.
    static constexpr std::size_t defaultCapacity = 64;

    // `now` is injectable so that tests can move the clock without sleeping.
    explicit PolkitAuthorizationCache(Clock::duration keepDuration = defaultKeepDuration,
                                      std::size_t capacity = defaultCapacity,
                                      Now now = {});

    PolkitAuthorizationCache(const PolkitAuthorizationCache &) = delete;
    auto operator=(const PolkitAuthorizationCache &) -> PolkitAuthorizationCache & = delete;

    // True while the grant recorded by grant() for the same caller and action is
    // still inside the keep window. Expired grants are dropped on the way and an
    // identity without a session never matches.
    [[nodiscard]] bool isGranted(const PolkitCallerIdentity &caller, const std::string &actionId);

    // Records that polkit authorized `actionId` for `caller`. Invalid identities
    // are ignored so that a caller whose session could not be resolved is never
    // remembered.
    void grant(const PolkitCallerIdentity &caller, const std::string &actionId);

    // Drops every grant.
    void clear();

    // Number of grants that are still valid, expired entries are dropped first.
    [[nodiscard]] std::size_t size();

    // Process wide instance shared by all package manager objects.
    static PolkitAuthorizationCache &instance();

private:
    using Key = std::tuple<std::uint32_t, std::string, std::string>;

    void pruneLocked(Clock::time_point now);
    [[nodiscard]] auto nowLocked() const -> Clock::time_point;

    const Clock::duration mKeepDuration;
    const std::size_t mCapacity;
    const Now mNow;

    std::mutex mMutex;
    std::map<Key, Clock::time_point> mGrants;
};

} // namespace linglong::service
