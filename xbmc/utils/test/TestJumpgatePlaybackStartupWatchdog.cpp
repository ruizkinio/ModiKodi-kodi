/*
 *  Copyright (C) 2026 Team Jumpgate
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "utils/JumpgatePlaybackStartupWatchdog.h"

#include <gtest/gtest.h>

using namespace KODI::JUMPGATE;

namespace
{
JumpgatePlaybackStartupBinding Binding(std::uint64_t generation, std::uint64_t token)
{
  return {17, generation, token, "request-" + std::to_string(generation)};
}

constexpr JumpgatePlaybackStartupThresholds TEST_THRESHOLDS{10, 30, 60};
} // namespace

TEST(TestJumpgatePlaybackStartupWatchdog, EmitsSoftDelayOnceThenRetainsTimeoutUntilAcknowledged)
{
  CJumpgatePlaybackStartupWatchdog watchdog{TEST_THRESHOLDS};
  ASSERT_TRUE(watchdog.Begin(Binding(1, 11), 100));
  EXPECT_FALSE(watchdog.Poll(109));

  const auto delayed = watchdog.Poll(110);
  ASSERT_TRUE(delayed);
  EXPECT_EQ(delayed->type, JumpgatePlaybackStartupSignalType::Delayed);
  EXPECT_EQ(delayed->stage, JumpgatePlaybackStartupStage::Admitted);
  EXPECT_FALSE(watchdog.Poll(111));

  const auto timeout = watchdog.Poll(130);
  ASSERT_TRUE(timeout);
  EXPECT_EQ(timeout->type, JumpgatePlaybackStartupSignalType::TimedOut);
  const auto retry = watchdog.Poll(131);
  ASSERT_TRUE(retry);
  EXPECT_EQ(retry->binding.playbackToken, 11U);
  EXPECT_TRUE(watchdog.AcknowledgeTimeout(timeout->binding));
  EXPECT_FALSE(watchdog.Poll(1000));
}

TEST(TestJumpgatePlaybackStartupWatchdog, ProgressExtendsInactivityButNotAbsoluteDeadline)
{
  CJumpgatePlaybackStartupWatchdog watchdog{TEST_THRESHOLDS};
  ASSERT_TRUE(watchdog.Begin(Binding(2, 22), 100));
  ASSERT_TRUE(watchdog.Advance(22, JumpgatePlaybackStartupStage::Dispatched, 125));
  EXPECT_FALSE(watchdog.Poll(154));
  ASSERT_TRUE(watchdog.Advance(22, JumpgatePlaybackStartupStage::CoreOpened, 155));
  EXPECT_FALSE(watchdog.Poll(159));

  const auto timeout = watchdog.Poll(160);
  ASSERT_TRUE(timeout);
  EXPECT_EQ(timeout->type, JumpgatePlaybackStartupSignalType::TimedOut);
  EXPECT_EQ(timeout->stage, JumpgatePlaybackStartupStage::CoreOpened);
}

TEST(TestJumpgatePlaybackStartupWatchdog, ReplacementRejectsEveryStaleMutationAndAcknowledgement)
{
  CJumpgatePlaybackStartupWatchdog watchdog{TEST_THRESHOLDS};
  const auto first = Binding(3, 33);
  const auto replacement = Binding(4, 44);
  ASSERT_TRUE(watchdog.Begin(first, 100));
  ASSERT_TRUE(watchdog.Begin(replacement, 105));

  EXPECT_FALSE(watchdog.Advance(33, JumpgatePlaybackStartupStage::CoreOpened, 106));
  EXPECT_FALSE(watchdog.Complete(33));
  EXPECT_FALSE(watchdog.Cancel(33));
  const auto timeout = watchdog.Poll(135);
  ASSERT_TRUE(timeout);
  EXPECT_EQ(timeout->binding.generation, 4U);
  EXPECT_FALSE(watchdog.AcknowledgeTimeout(first));
  EXPECT_TRUE(watchdog.AcknowledgeTimeout(replacement));
}

TEST(TestJumpgatePlaybackStartupWatchdog, ReadyAndTerminalCancellationSuppressTimeout)
{
  CJumpgatePlaybackStartupWatchdog watchdog{TEST_THRESHOLDS};
  ASSERT_TRUE(watchdog.Begin(Binding(5, 55), 100));
  ASSERT_TRUE(watchdog.Advance(55, JumpgatePlaybackStartupStage::CoreOpened, 110));
  EXPECT_TRUE(watchdog.Complete(55));
  EXPECT_FALSE(watchdog.Poll(1000));

  ASSERT_TRUE(watchdog.Begin(Binding(6, 66), 200));
  EXPECT_TRUE(watchdog.Cancel(66));
  EXPECT_FALSE(watchdog.Poll(1000));
}

TEST(TestJumpgatePlaybackStartupWatchdog, RejectsInvalidOrRegressiveState)
{
  CJumpgatePlaybackStartupWatchdog watchdog{TEST_THRESHOLDS};
  EXPECT_FALSE(watchdog.Begin({}, 0));
  ASSERT_TRUE(watchdog.Begin(Binding(7, 77), 100));
  EXPECT_FALSE(watchdog.Advance(77, JumpgatePlaybackStartupStage::Admitted, 101));
  ASSERT_TRUE(watchdog.Advance(77, JumpgatePlaybackStartupStage::ParserReady, 102));
  EXPECT_FALSE(watchdog.Advance(77, JumpgatePlaybackStartupStage::CoreOpened, 103));
  EXPECT_FALSE(watchdog.Advance(77, JumpgatePlaybackStartupStage::ParserReady, 104));
}
