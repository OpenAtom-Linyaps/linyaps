// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/package_manager/polkit_authorization_cache.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace linglong::service {
namespace {

using namespace std::chrono_literals;

constexpr const char *INSTALL_ACTION = "org.deepin.linglong.PackageManager1.install";
constexpr const char *UNINSTALL_ACTION = "org.deepin.linglong.PackageManager1.uninstall";
constexpr const char *PRUNE_ACTION = "org.deepin.linglong.PackageManager1.prune";

constexpr std::uint32_t ALICE_UID = 1000;
constexpr std::uint32_t BOB_UID = 1001;
constexpr const char *ALICE_SESSION = "3";
constexpr const char *BOB_SESSION = "4";

// A clock that the test drives itself, so that the keep window can be crossed
// without a single sleep().
class FakeClock
{
public:
    [[nodiscard]] PolkitAuthorizationCache::Clock::time_point now() const { return mNow; }

    void advance(PolkitAuthorizationCache::Clock::duration delta) { mNow += delta; }

private:
    PolkitAuthorizationCache::Clock::time_point mNow{};
};

PolkitAuthorizationCache::Now bindClock(FakeClock &clock)
{
    return [&clock] {
        return clock.now();
    };
}

[[nodiscard]] PolkitCallerIdentity makeCaller(std::uint32_t uid, const std::string &sessionId)
{
    return PolkitCallerIdentity{ .uid = uid, .sessionId = sessionId };
}

// Every <action> element of the policy file together with the value of its
// active default. The policy is a flat list of actions, so a couple of string
// searches are enough and the test does not need an XML parser.
std::vector<std::pair<std::string, bool>> actionsDeclaredByPolicy(const std::string &xml)
{
    const std::string actionOpen = "<action id=\"";
    const std::string actionClose = "</action>";
    const std::string keepActive = "<allow_active>auth_admin_keep</allow_active>";

    std::vector<std::pair<std::string, bool>> actions;
    std::size_t pos = 0;
    while ((pos = xml.find(actionOpen, pos)) != std::string::npos) {
        const auto idBegin = pos + actionOpen.size();
        const auto idEnd = xml.find('"', idBegin);
        const auto blockEnd = idEnd == std::string::npos ? idEnd : xml.find(actionClose, idEnd);
        if (idEnd == std::string::npos || blockEnd == std::string::npos) {
            break;
        }

        const auto id = xml.substr(idBegin, idEnd - idBegin);
        const auto block = xml.substr(idEnd, blockEnd - idEnd);
        actions.emplace_back(id, block.find(keepActive) != std::string::npos);
        pos = blockEnd + actionClose.size();
    }

    return actions;
}

} // namespace

TEST(PolkitAuthorizationCache, KeepActionsAreRecognized)
{
    EXPECT_TRUE(isKeepAuthorizedAction(INSTALL_ACTION));
    EXPECT_TRUE(isKeepAuthorizedAction(UNINSTALL_ACTION));
    EXPECT_TRUE(isKeepAuthorizedAction("org.deepin.linglong.PackageManager1.update"));
    EXPECT_TRUE(isKeepAuthorizedAction("org.deepin.linglong.PackageManager1.install-from-file"));

    // Actions with a plain auth_admin policy have to keep asking for a password.
    EXPECT_FALSE(isKeepAuthorizedAction(PRUNE_ACTION));
    EXPECT_FALSE(isKeepAuthorizedAction("org.deepin.linglong.PackageManager1.set-configuration"));
    EXPECT_FALSE(isKeepAuthorizedAction(""));
    EXPECT_FALSE(isKeepAuthorizedAction("org.deepin.linglong.PackageManager1.install2"));
}

TEST(PolkitAuthorizationCache, GrantIsReusedForTheSameSession)
{
    FakeClock clock;
    PolkitAuthorizationCache cache(5min, 64, bindClock(clock));
    const auto alice = makeCaller(ALICE_UID, ALICE_SESSION);

    EXPECT_FALSE(cache.isGranted(alice, INSTALL_ACTION));

    cache.grant(alice, INSTALL_ACTION);
    EXPECT_TRUE(cache.isGranted(alice, INSTALL_ACTION));

    // Granting again must not duplicate the entry either.
    cache.grant(alice, INSTALL_ACTION);
    EXPECT_TRUE(cache.isGranted(alice, INSTALL_ACTION));
    EXPECT_EQ(cache.size(), 1u);

    // A daemon that has to drop every grant can do so at once.
    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_FALSE(cache.isGranted(alice, INSTALL_ACTION));
}

TEST(PolkitAuthorizationCache, GrantExpiresAfterTheKeepWindow)
{
    FakeClock clock;
    PolkitAuthorizationCache cache(5min, 64, bindClock(clock));
    const auto alice = makeCaller(ALICE_UID, ALICE_SESSION);

    cache.grant(alice, INSTALL_ACTION);

    clock.advance(5min - 1ms);
    EXPECT_TRUE(cache.isGranted(alice, INSTALL_ACTION));

    // polkit stops reusing a retained authorization once its window elapsed, so
    // the daemon has to drop the entry at the same moment.
    clock.advance(1ms);
    EXPECT_FALSE(cache.isGranted(alice, INSTALL_ACTION));
    EXPECT_EQ(cache.size(), 0u);
}

TEST(PolkitAuthorizationCache, GrantIsScopedToCallerAndAction)
{
    FakeClock clock;
    PolkitAuthorizationCache cache(5min, 64, bindClock(clock));
    const auto alice = makeCaller(ALICE_UID, ALICE_SESSION);
    const auto aliceElsewhere = makeCaller(ALICE_UID, BOB_SESSION);
    const auto bob = makeCaller(BOB_UID, ALICE_SESSION);

    cache.grant(alice, INSTALL_ACTION);

    EXPECT_TRUE(cache.isGranted(alice, INSTALL_ACTION));
    // Another action of the same caller still needs its own authorization.
    EXPECT_FALSE(cache.isGranted(alice, UNINSTALL_ACTION));
    // Neither another session of the same user nor a session id recycled by
    // another user may inherit the grant.
    EXPECT_FALSE(cache.isGranted(aliceElsewhere, INSTALL_ACTION));
    EXPECT_FALSE(cache.isGranted(bob, INSTALL_ACTION));
}

TEST(PolkitAuthorizationCache, CallerWithoutSessionIsNeverRemembered)
{
    FakeClock clock;
    PolkitAuthorizationCache cache(5min, 64, bindClock(clock));
    const auto sessionLess = makeCaller(ALICE_UID, "");

    EXPECT_FALSE(sessionLess.isValid());

    cache.grant(sessionLess, INSTALL_ACTION);
    EXPECT_FALSE(cache.isGranted(sessionLess, INSTALL_ACTION));
    EXPECT_EQ(cache.size(), 0u);
}

TEST(PolkitAuthorizationCache, ExpiredGrantsAreDropped)
{
    FakeClock clock;
    PolkitAuthorizationCache cache(1min, 8, bindClock(clock));

    for (std::uint32_t index = 0; index < 12; ++index) {
        cache.grant(makeCaller(ALICE_UID, std::to_string(index)), INSTALL_ACTION);
    }

    // The cache may not grow without bound on a long running daemon.
    EXPECT_EQ(cache.size(), 8u);

    clock.advance(1min);
    EXPECT_EQ(cache.size(), 0u);
}

TEST(PolkitAuthorizationCache, CapacityDropsTheGrantThatExpiresFirst)
{
    FakeClock clock;
    PolkitAuthorizationCache cache(10min, 2, bindClock(clock));
    const auto first = makeCaller(ALICE_UID, "1");
    const auto second = makeCaller(ALICE_UID, "2");
    const auto third = makeCaller(ALICE_UID, "3");

    cache.grant(first, INSTALL_ACTION);
    clock.advance(1min);
    cache.grant(second, INSTALL_ACTION);
    clock.advance(1min);
    cache.grant(third, INSTALL_ACTION);

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_FALSE(cache.isGranted(first, INSTALL_ACTION));
    EXPECT_TRUE(cache.isGranted(second, INSTALL_ACTION));
    EXPECT_TRUE(cache.isGranted(third, INSTALL_ACTION));
}

TEST(PolkitAuthorizationCache, GrantsSurviveConcurrentAccess)
{
    PolkitAuthorizationCache cache(5min, 8);
    std::atomic<int> failures{ 0 };
    std::vector<std::thread> workers;

    for (std::uint32_t uid = ALICE_UID; uid < ALICE_UID + 4; ++uid) {
        workers.emplace_back([&cache, &failures, uid] {
            const auto session = std::to_string(uid);
            for (int round = 0; round < 50; ++round) {
                cache.grant(makeCaller(uid, session), INSTALL_ACTION);
                if (!cache.isGranted(makeCaller(uid, session), INSTALL_ACTION)) {
                    failures.fetch_add(1);
                }
                if (cache.size() > 8) {
                    failures.fetch_add(1);
                }
            }
        });
    }

    for (auto &worker : workers) {
        worker.join();
    }

    EXPECT_EQ(failures.load(), 0);
}

// The compiled in list has to follow the policy, otherwise the daemon would
// either ignore a keep policy or - much worse - stop asking for the password of
// an action that requires it for every call.
TEST(PolkitAuthorizationCache, KeepActionListMatchesPolicy)
{
    std::ifstream policy(LINGLONG_POLKIT_POLICY_FILE);
    ASSERT_TRUE(policy.is_open()) << "can't read " << LINGLONG_POLKIT_POLICY_FILE;

    const std::string xml{ std::istreambuf_iterator<char>(policy),
                           std::istreambuf_iterator<char>() };
    ASSERT_FALSE(xml.empty());

    const auto actions = actionsDeclaredByPolicy(xml);
    ASSERT_FALSE(actions.empty());

    std::size_t keepActions = 0;
    for (const auto &[actionId, keepsAuthorization] : actions) {
        EXPECT_EQ(isKeepAuthorizedAction(actionId), keepsAuthorization) << actionId;
        if (keepsAuthorization) {
            ++keepActions;
        }
    }

    EXPECT_EQ(keepActions, 4u);
}

} // namespace linglong::service
