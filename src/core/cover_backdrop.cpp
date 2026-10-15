#include "core/cover_backdrop.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cup {
namespace {

// One box-blur pass along rows (step 1) or columns (step width): each channel
// becomes its neighbourhood's average, edges repeating their last pixel.
void boxPass(std::vector<float>& px, int width, int height, int radius, bool rows)
{
    std::vector<float> out(px.size());
    int lines = rows ? height : width, length = rows ? width : height;
    auto index = [&](int line, int i) { return rows ? (line * width + i) * 4 : (i * width + line) * 4; };
    for (int line = 0; line < lines; ++line)
        for (int i = 0; i < length; ++i)
            for (int ch = 0; ch < 4; ++ch) {
                float sum = 0;
                for (int k = -radius; k <= radius; ++k) sum += px[static_cast<std::size_t>(index(line, std::clamp(i + k, 0, length - 1)) + ch)];
                out[static_cast<std::size_t>(index(line, i) + ch)] = sum / static_cast<float>(2 * radius + 1);
            }
    px.swap(out);
}

} // namespace

ImageData blurredCover(const ImageData& cover)
{
    std::size_t count = static_cast<std::size_t>(std::max(cover.width, 0)) * static_cast<std::size_t>(std::max(cover.height, 0));
    if (count == 0 || cover.bgra.size() < count * 4) return {};

    // Shrink by averaging (each small pixel the mean of the big ones under it):
    // a blur this strong loses nothing, and it's cheap.
    int longest = std::max(cover.width, cover.height);
    int size = std::max(4, static_cast<int>(tokens().coverBackdropSize));
    float scale = std::min(1.0f, static_cast<float>(size) / static_cast<float>(longest));
    int w = std::max(1, static_cast<int>(std::lround(cover.width * scale)));
    int h = std::max(1, static_cast<int>(std::lround(cover.height * scale)));
    std::vector<float> px(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4, 0.0f);
    std::vector<float> counts(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    for (int y = 0; y < cover.height; ++y)
        for (int x = 0; x < cover.width; ++x) {
            int sx = std::min(w - 1, x * w / cover.width), sy = std::min(h - 1, y * h / cover.height);
            std::size_t to = static_cast<std::size_t>(sy * w + sx), from = (static_cast<std::size_t>(y) * static_cast<std::size_t>(cover.width) + static_cast<std::size_t>(x)) * 4;
            for (int ch = 0; ch < 4; ++ch) px[to * 4 + static_cast<std::size_t>(ch)] += cover.bgra[from + static_cast<std::size_t>(ch)];
            counts[to] += 1;
        }
    for (std::size_t i = 0; i < counts.size(); ++i)
        for (int ch = 0; ch < 4; ++ch) px[i * 4 + static_cast<std::size_t>(ch)] /= std::max(counts[i], 1.0f);

    // Three box blurs, each way, look like a Gaussian; the radius is a share of the size.
    int radius = std::max(1, static_cast<int>(std::lround(static_cast<float>(std::max(w, h)) * tokens().coverBackdropBlur)));
    for (int pass = 0; pass < 3; ++pass) {
        boxPass(px, w, h, radius, true);
        boxPass(px, w, h, radius, false);
    }

    ImageData out{ .width = w, .height = h };
    out.bgra.resize(px.size());
    for (std::size_t i = 0; i < px.size(); ++i) out.bgra[i] = static_cast<std::uint8_t>(std::clamp(std::lround(px[i]), 0L, 255L));
    for (std::size_t i = 3; i < out.bgra.size(); i += 4) out.bgra[i] = 255; // opaque: it's a background (premultiplied, so colour stays as is)
    return out;
}

} // namespace cup
