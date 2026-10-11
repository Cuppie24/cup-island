#include "app/live_tokens.h"
#include "platform/log.h"

#include "core/tokens_json.h"
#include "platform/win.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace cup::app {
namespace {

std::string g_lastText; // skip the duplicate change notifications one save produces

void log(const std::string& message)
{
    platform::logLine("%s", message.c_str());
}

void warnIfClipped(const Tokens& t)
{
    struct Named { const char* name; Vec2 size; };
    for (auto [name, size] : { Named{ "idleSize", t.idleSize }, Named{ "compactSize", t.compactSize },
                               Named{ "alertSize", t.alertSize }, Named{ "expandedSize", t.expandedSize },
                               Named{ "receptiveSize", t.receptiveSize }, Named{ "hiddenSize", t.hiddenSize } }) {
        if (size.x > t.canvas.x || size.y > t.canvas.y)
            log(std::string("tokens.json: ") + name + " is larger than canvasSize and will be clipped");
    }
}

} // namespace

std::filesystem::path findTokensFile()
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    auto besideExe = std::filesystem::path(exePath).parent_path() / "tokens.json";
    if (std::filesystem::exists(besideExe)) return besideExe;
    return std::filesystem::path(CUP_TOKENS_SOURCE_PATH);
}

bool loadTokens(const std::filesystem::path& file, bool startup)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (startup) log("no tokens.json at " + file.string() + ", using built-in defaults");
        return false;
    }
    std::string text{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
    if (text == g_lastText) return false;
    g_lastText = text;

    TokensParse parsed = parseTokens(text);
    if (!parsed.ok()) {
        log("tokens.json has problems, keeping previous values:");
        for (const auto& problem : parsed.problems) log("  " + problem);
        return false;
    }

    Tokens next = parsed.tokens;
    if (!startup && next.canvas != tokens().canvas) {
        log("tokens.json: canvasSize changes apply on restart");
        next.canvas = tokens().canvas;
    }
    warnIfClipped(next);
    if (next == tokens()) return false;

    setTokens(next);
    log(startup ? "loaded " + file.string() : "tokens.json reloaded");
    return true;
}

} // namespace cup::app
