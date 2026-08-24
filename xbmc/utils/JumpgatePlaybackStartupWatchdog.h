/*
 *  Copyright (C) 2026 Team Jumpgate
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace KODI::JUMPGATE
{

constexpr std::int64_t JUMPGATE_PLAYBACK_STARTUP_SOFT_DELAY_MS = 12000;
constexpr std::int64_t JUMPGATE_PLAYBACK_STARTUP_WAKE_INTERVAL_MS = 1000;
constexpr std::int64_t JUMPGATE_PLAYBACK_STARTUP_INACTIVITY_TIMEOUT_MS = 45000;
constexpr std::int64_t JUMPGATE_PLAYBACK_STARTUP_ABSOLUTE_TIMEOUT_MS = 90000;

enum class JumpgatePlaybackStartupStage : std::uint8_t
{
  Admitted,
  Dispatched,
  CoreOpened,
  ParserReady,
};

enum class JumpgatePlaybackStartupSignalType : std::uint8_t
{
  Delayed,
  TimedOut,
};

struct JumpgatePlaybackStartupBinding final
{
  std::uint64_t lifecycleToken{0};
  std::uint64_t generation{0};
  std::uint64_t playbackToken{0};
  std::string requestId;
};

struct JumpgatePlaybackStartupSignal final
{
  JumpgatePlaybackStartupSignalType type{JumpgatePlaybackStartupSignalType::Delayed};
  JumpgatePlaybackStartupBinding binding;
  JumpgatePlaybackStartupStage stage{JumpgatePlaybackStartupStage::Admitted};
};

struct JumpgatePlaybackStartupThresholds final
{
  std::int64_t softDelayMs{JUMPGATE_PLAYBACK_STARTUP_SOFT_DELAY_MS};
  std::int64_t inactivityTimeoutMs{JUMPGATE_PLAYBACK_STARTUP_INACTIVITY_TIMEOUT_MS};
  std::int64_t absoluteTimeoutMs{JUMPGATE_PLAYBACK_STARTUP_ABSOLUTE_TIMEOUT_MS};
};

class CJumpgatePlaybackStartupWatchdog final
{
public:
  explicit CJumpgatePlaybackStartupWatchdog(JumpgatePlaybackStartupThresholds thresholds = {});

  bool Begin(JumpgatePlaybackStartupBinding binding, std::int64_t nowMs);
  bool Advance(std::uint64_t playbackToken, JumpgatePlaybackStartupStage stage, std::int64_t nowMs);
  std::optional<JumpgatePlaybackStartupBinding> Complete(std::uint64_t playbackToken);
  bool Cancel(std::uint64_t playbackToken);
  void Reset();

  std::optional<JumpgatePlaybackStartupSignal> Poll(std::int64_t nowMs);
  bool AcknowledgeTimeout(const JumpgatePlaybackStartupBinding& binding);

private:
  struct Attempt final
  {
    JumpgatePlaybackStartupBinding binding;
    JumpgatePlaybackStartupStage stage{JumpgatePlaybackStartupStage::Admitted};
    std::int64_t startedAtMs{0};
    std::int64_t lastProgressAtMs{0};
    bool delayedEmitted{false};
    bool timeoutEmitted{false};
  };

  static bool SameBinding(const JumpgatePlaybackStartupBinding& left,
                          const JumpgatePlaybackStartupBinding& right);

  JumpgatePlaybackStartupThresholds m_thresholds;
  std::mutex m_mutex;
  std::optional<Attempt> m_attempt;
};

} // namespace KODI::JUMPGATE
