#pragma once
// A media session's status as Windows reports it (SMTC, TR §6), and what the
// island shows for it: playing or not. Pure, so the rule is tested on Linux;
// smtc_reading maps Windows' own enum onto this one.

namespace cup {

// Windows' GlobalSystemMediaTransportControlsSessionPlaybackStatus, value for value.
enum class PlaybackStatus { Closed = 0, Opened = 1, Changing = 2, Stopped = 3, Playing = 4, Paused = 5 };

// Playing, or Changing: between two states (loading, buffering, the next
// track). Windows' own flyout shows ⏸ for it, and Spotify can stay there
// while it plays: shown as paused, the island's button stuck on ▶ (decision 182).
bool showsPlaying(PlaybackStatus status);

// "Changing", for the log.
const char* statusName(PlaybackStatus status);

} // namespace cup
