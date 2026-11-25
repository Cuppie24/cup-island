#include "providers/media/playback_status.h"

namespace cup {

bool showsPlaying(PlaybackStatus status)
{
    return status == PlaybackStatus::Playing || status == PlaybackStatus::Changing;
}

const char* statusName(PlaybackStatus status)
{
    switch (status) {
    case PlaybackStatus::Closed: return "Closed";
    case PlaybackStatus::Opened: return "Opened";
    case PlaybackStatus::Changing: return "Changing";
    case PlaybackStatus::Stopped: return "Stopped";
    case PlaybackStatus::Playing: return "Playing";
    case PlaybackStatus::Paused: return "Paused";
    }
    return "unknown";
}

} // namespace cup
