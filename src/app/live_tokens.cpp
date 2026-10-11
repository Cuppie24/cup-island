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
float g_scale = 1.0f;   // display scale: tokens.json is logical pixels

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

namespace {

// `resize`: canvasSize may change too (startup, or a new display scale).
bool apply(const std::filesystem::path& file, bool startup, bool resize)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (startup) log("no tokens.json at " + file.string() + ", using built-in defaults");
        if (!resize) return false;
        Tokens defaults = scaleTokens(Tokens{}, g_scale);
        if (defaults == tokens()) return false;
        setTokens(defaults);
        return true;
    }
    std::string text{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
    if (text == g_lastText && !resize) return false;
    g_lastText = text;

    TokensParse parsed = parseTokens(text);
    if (!parsed.ok()) {
        log("tokens.json has problems, keeping previous values:");
        for (const auto& problem : parsed.problems) log("  " + problem);
        return false;
    }

    // canvasSize (and islandScale, which scales it) apply live: the app
    // resizes the render layer when the canvas changes.
    Tokens next = scaleTokens(parsed.tokens, g_scale);
    warnIfClipped(next);
    if (next == tokens()) return false;

    setTokens(next);
    log(startup ? "loaded " + file.string() : resize ? "tokens.json re-applied for the new display scale" : "tokens.json reloaded");
    return true;
}

} // namespace

bool loadTokens(const std::filesystem::path& file, bool startup) { return apply(file, startup, startup); }

void setDisplayScale(float scale)
{
    g_scale = scale > 0 ? scale : 1.0f;
    log("display scale " + std::to_string(static_cast<int>(g_scale * 100 + 0.5f)) + "%");
}

bool rescaleTokens(const std::filesystem::path& file, float scale)
{
    if (scale <= 0 || scale == g_scale) return false;
    setDisplayScale(scale);
    return apply(file, /* startup */ false, /* resize */ true);
}

} // namespace cup::app
