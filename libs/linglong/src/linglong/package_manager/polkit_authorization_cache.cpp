// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "polkit_authorization_cache.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>
#include <utility>

namespace linglong::service {

namespace {

// Actions whose policy retains the authorization. Keep them in sync with the
// `auth_admin_keep` entries of
// misc/share/polkit-1/actions/org.deepin.linglong.PackageManager1.policy.
constexpr std::array<std::string_view, 4> keepAuthorizedActions{
    "org.deepin.linglong.PackageManager1.install",
    "org.deepin.linglong.PackageManager1.install-from-file",
    "org.deepin.linglong.PackageManager1.uninstall",
    "org.deepin.linglong.PackageManager1.update",
};

} // namespace

bool isKeepAuthorizedAction(const std::string &actionId) noexcept
{
    const auto candidate = std::string_view{ actionId };
    return std::find(keepAuthorizedActions.cbegin(), keepAuthorizedActions.cend(), candidate)
      != keepAuthorizedActions.cend();
}

PolkitAuthorizationCache::PolkitAuthorizationCache(Clock::duration keepDuration,
                                                   std::size_t capacity,
                                                   Now now)
    : mKeepDuration(keepDuration)
    , mCapacity(capacity == 0 ? 1 : capacity)
    , mNow(std::move(now))
{
}

bool PolkitAuthorizationCache::isGranted(const PolkitCallerIdentity &caller,
                                         const std::string &actionId)
{
    if (!caller.isValid()) {
        return false;
    }

    std::lock_guard lock(mMutex);

    const auto it = mGrants.find(Key{ caller.uid, caller.sessionId, actionId });
    if (it == mGrants.end()) {
        return false;
    }

    if (it->second <= nowLocked()) {
        mGrants.erase(it);
        return false;
    }

    return true;
}

void PolkitAuthorizationCache::grant(const PolkitCallerIdentity &caller,
                                     const std::string &actionId)
{
    if (!caller.isValid()) {
        return;
    }

    std::lock_guard lock(mMutex);

    const auto now = nowLocked();
    pruneLocked(now);
    mGrants[Key{ caller.uid, caller.sessionId, actionId }] = now + mKeepDuration;

    while (mGrants.size() > mCapacity) {
        // Drop the grant that expires first, it is the least useful one.
        auto oldest = mGrants.begin();
        for (auto it = std::next(oldest); it != mGrants.end(); ++it) {
            if (it->second < oldest->second) {
                oldest = it;
            }
        }
        mGrants.erase(oldest);
    }
}

void PolkitAuthorizationCache::clear()
{
    std::lock_guard lock(mMutex);
    mGrants.clear();
}

std::size_t PolkitAuthorizationCache::size()
{
    std::lock_guard lock(mMutex);
    pruneLocked(nowLocked());
    return mGrants.size();
}

PolkitAuthorizationCache &PolkitAuthorizationCache::instance()
{
    static PolkitAuthorizationCache cache;
    return cache;
}

void PolkitAuthorizationCache::pruneLocked(Clock::time_point now)
{
    for (auto it = mGrants.begin(); it != mGrants.end();) {
        if (it->second <= now) {
            it = mGrants.erase(it);
        } else {
            ++it;
        }
    }
}

auto PolkitAuthorizationCache::nowLocked() const -> Clock::time_point
{
    return mNow ? mNow() : Clock::now();
}

} // namespace linglong::service
