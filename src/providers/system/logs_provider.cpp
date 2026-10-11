#include "providers/system/logs_provider.h"

#include "core/tokens.h"

namespace cup {

Panel logsPanel(LogsTile state)
{
    // Short words: a 1-column tile has room for about six characters in Title.
    const char* word = state == LogsTile::Copied ? "Copied" : state == LogsTile::Failed ? "Failed" : "Logs";
    Content content{
        .label = state == LogsTile::Ready ? "Copy logs" : word,
        .tile = { row({ Icon{ Glyph::Copy } }), row({ Text{ word, TextStyle::Title } }) },
    };
    return { .id = LogsProvider::kPanelId, .content = content, .tapAction = LogsProvider::kCopy };
}

void LogsProvider::start(Host& host)
{
    host_ = &host;
    host_->publishPanel(logsPanel(LogsTile::Ready));
}

void LogsProvider::stop()
{
    host_->wakeAt(std::nullopt);
    host_->removePanel(kPanelId);
    host_ = nullptr;
}

void LogsProvider::onAction(const Action& action)
{
    if (!host_ || action.actionId != kCopy) return;
    host_->publishPanel(logsPanel(copy_() ? LogsTile::Copied : LogsTile::Failed));
    host_->wakeAt(host_->now() + tokens().logsNote); // a second tap restarts the note
}

void LogsProvider::onWake()
{
    if (host_) host_->publishPanel(logsPanel(LogsTile::Ready));
}

} // namespace cup
