#include "providers/update/release_json.h"

#include "indexer/json.h"

namespace cup {
namespace {

std::string field(const indexer::Json& object, std::string_view key)
{
    const indexer::Json* v = object.get(key);
    return v && v->isString() ? v->text : std::string();
}

} // namespace

std::optional<GitHubRelease> parseReleaseJson(std::string_view text)
{
    auto json = indexer::parseJson(text);
    if (!json || json->type != indexer::Json::Type::Object) return std::nullopt;
    GitHubRelease r{ .tag = field(*json, "tag_name"), .page = field(*json, "html_url") };
    r.mandatory = isMandatory(field(*json, "body"));
    if (const indexer::Json* assets = json->get("assets"); assets && assets->type == indexer::Json::Type::Array) {
        for (const indexer::Json& a : assets->items) {
            if (a.type != indexer::Json::Type::Object) continue;
            r.assets.push_back({ field(a, "name"), field(a, "browser_download_url"), field(a, "digest") });
        }
    }
    return r;
}

} // namespace cup
