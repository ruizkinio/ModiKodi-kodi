/*
 *  Copyright (C) 2026 Team Jumpgate
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JumpgatePlaybackStartupWatchdog.h"

#include <algorithm>
#include <utility>

namespace KODI::JUMPGATE
{

CJumpgatePlaybackStartupWatchdog::CJumpgatePlaybackStartupWatchdog(
    JumpgatePlaybackStartupThresholds thresholds)
  : m_thresholds(thresholds)
{
}

bool CJumpgatePlaybackStartupWatchdog::Begin(JumpgatePlaybackStartupBinding binding,
                                             std::int64_t nowMs)
{
  if (binding.lifecycleToken == 0 || binding.generation == 0 || binding.playbackToken == 0 ||
      binding.requestId.empty() || nowMs < 0 || m_thresholds.softDelayMs < 0 ||
      m_thresholds.inactivityTimeoutMs <= 0 || m_thresholds.absoluteTimeoutMs <= 0)
  {
    return false;
  }

  std::lock_guard lock(m_mutex);
  m_attempt = Attempt{std::move(binding), JumpgatePlaybackStartupStage::Admitted, nowMs, nowMs};
  return true;
}

bool CJumpgatePlaybackStartupWatchdog::Advance(std::uint64_t playbackToken,
                                               JumpgatePlaybackStartupStage stage,
                                               std::int64_t nowMs)
{
  std::lock_guard lock(m_mutex);
  if (!m_attempt || playbackToken == 0 || playbackToken != m_attempt->binding.playbackToken ||
      nowMs < m_attempt->startedAtMs || m_attempt->timeoutEmitted || stage <= m_attempt->stage)
  {
    return false;
  }

  m_attempt->stage = stage;
  m_attempt->lastProgressAtMs = nowMs;
  return true;
}

std::optional<JumpgatePlaybackStartupBinding> CJumpgatePlaybackStartupWatchdog::Complete(
    std::uint64_t playbackToken)
{
  std::lock_guard lock(m_mutex);
  if (!m_attempt || playbackToken == 0 || playbackToken != m_attempt->binding.playbackToken ||
      m_attempt->timeoutEmitted)
  {
    return std::nullopt;
  }
  JumpgatePlaybackStartupBinding binding = m_attempt->binding;
  m_attempt.reset();
  return binding;
}

bool CJumpgatePlaybackStartupWatchdog::Cancel(std::uint64_t playbackToken)
{
  std::lock_guard lock(m_mutex);
  if (!m_attempt || playbackToken == 0 || playbackToken != m_attempt->binding.playbackToken)
    return false;
  m_attempt.reset();
  return true;
}

void CJumpgatePlaybackStartupWatchdog::Reset()
{
  std::lock_guard lock(m_mutex);
  m_attempt.reset();
}

std::optional<JumpgatePlaybackStartupSignal> CJumpgatePlaybackStartupWatchdog::Poll(
    std::int64_t nowMs)
{
  std::lock_guard lock(m_mutex);
  if (!m_attempt || nowMs < m_attempt->startedAtMs)
    return std::nullopt;

  const std::int64_t inactivityDeadline =
      m_attempt->lastProgressAtMs + m_thresholds.inactivityTimeoutMs;
  const std::int64_t absoluteDeadline = m_attempt->startedAtMs + m_thresholds.absoluteTimeoutMs;
  if (nowMs >= std::min(inactivityDeadline, absoluteDeadline))
  {
    m_attempt->timeoutEmitted = true;
    return JumpgatePlaybackStartupSignal{JumpgatePlaybackStartupSignalType::TimedOut,
                                         m_attempt->binding, m_attempt->stage};
  }

  if (!m_attempt->delayedEmitted && nowMs >= m_attempt->startedAtMs + m_thresholds.softDelayMs)
  {
    m_attempt->delayedEmitted = true;
    return JumpgatePlaybackStartupSignal{JumpgatePlaybackStartupSignalType::Delayed,
                                         m_attempt->binding, m_attempt->stage};
  }
  return std::nullopt;
}

bool CJumpgatePlaybackStartupWatchdog::AcknowledgeTimeout(
    const JumpgatePlaybackStartupBinding& binding)
{
  std::lock_guard lock(m_mutex);
  if (!m_attempt || !m_attempt->timeoutEmitted || !SameBinding(m_attempt->binding, binding))
    return false;
  m_attempt.reset();
  return true;
}

bool CJumpgatePlaybackStartupWatchdog::SameBinding(const JumpgatePlaybackStartupBinding& left,
                                                   const JumpgatePlaybackStartupBinding& right)
{
  return left.lifecycleToken == right.lifecycleToken && left.generation == right.generation &&
         left.playbackToken == right.playbackToken && left.requestId == right.requestId;
}

} // namespace KODI::JUMPGATE
