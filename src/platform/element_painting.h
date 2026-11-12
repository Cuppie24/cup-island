#pragma once
// Pixels for content items: Direct2D/DirectWrite drawing into a surface of
// `size` pixels. Stateless apart from cached font lookups.

#include "core/content.h"
#include "platform/graphics_device.h"

#include <functional>
#include <optional>

namespace cup::platform {

// `ink`: drawn in it instead of its style's colour (white, for a text
// coloured by a fading brush through it, decision 102).
void paintText(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, Vec2 size, std::optional<Color> ink = std::nullopt);
// The one colour `text` is drawn in, if it's one (decision 102): not with
// bright digits or parts, a detail line under it, or an emoji (its own colours).
std::optional<Color> singleInkOf(const Text& text);
// Width of `text` on a single line, untrimmed: decides whether marquee text has to swim.
float measureText(IDWriteFactory* dw, const Text& text);
// Swimming text: the string twice, `gap` apart, starting at x = `lead`, so
// scrolling one copy's width plus the gap loops seamlessly.
void paintMarquee(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, float width, float lead, float gap, float height);
// `precise`: exactly centred on its ink, unsnapped, a ▶ optically nudged (a primary button's glyph, decision 071).
void paintGlyph(ID2D1DeviceContext* dc, IDWriteFactory* dw, Glyph glyph, float fontSize, Vec2 size, const Color& color,
                bool precise = false);
// A text button's label, centred.
void paintLabel(ID2D1DeviceContext* dc, IDWriteFactory* dw, const std::string& label, Vec2 size, const Color& color,
                TextStyle style = TextStyle::Body);
// A group's piece of background behind its row (decision 090): tileColor,
// `radius` round at the ends the group ends at, square where it goes on.
void paintGroupFill(ID2D1DeviceContext* dc, const GroupFill& fill, Vec2 size, float radius);
// Segments' labels (decision 093), every one in `color`, each centred in its
// option. Drawn twice: on the track, and in the island's colour inside the pill.
// The track and the pill are shapes (segments_parts.h).
void paintSegmentLabels(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Segments& segments, Vec2 size, const Color& color);
// Keys (decision 092): each key in a cap (tileColor, edged in trackColor), the words between them plain.
void paintKeys(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Keys& keys, Vec2 size);
// A Divider: its line, in the track colour (decision 064).
void paintDivider(ID2D1DeviceContext* dc, Vec2 size);
// A Dial's middle (decision 065): its time big, its label under it, the two centred as a block. Its ring is shapes.
void paintDial(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Dial& dial, Vec2 size);
// Button::primary: its glyph in the island's colour (decision 064), over its
// filled circle, which is a shape whose colour fades (ElementVisuals, decision 101).
void paintPrimaryButton(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Button& button, Vec2 size);
// Button::round: its circle (a Switch's colours: the theme's live colour when on, its track's when off) and label.
void paintRoundButton(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Button& button, Vec2 size);
// Where in `text` (a byte offset) a click at `at` (from its frame's top left) lands:
// where editing it in place puts the caret (decision 043).
std::size_t caretAt(IDWriteFactory* dw, const Text& text, Vec2 size, Vec2 at);
// Steps (decision 010): a dot per step, centred vertically; done and to-do
// ones in their token colours, the current one in `accent`.
void paintSteps(ID2D1DeviceContext* dc, const Steps& steps, Vec2 size, const Color& accent);
// A Swatch (decision 030): a circle of its fill, ringed in trackColor so a
// pale one shows on a pale island, with a dot of its mark in the middle.
void paintSwatch(ID2D1DeviceContext* dc, const Swatch& swatch, Vec2 size);
// A Tag (decision 048): a pill of badgeColor, its text centred in tagTextColor, heavier than captions.
void paintTag(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Tag& tag, Vec2 size);
// Whatever `paint` draws, drawn into `dc` without colour (decision 072): a muted
// tab's text, glyph and picture alike. Plain `paint(dc)` if the effect can't be made.
void paintGrey(ID2D1DeviceContext* dc, const std::function<void(ID2D1DeviceContext*)>& paint);
// Fills a rounded rect with the image scaled to cover it; a neutral tile if `data` is null.
void paintImage(ID2D1DeviceContext* dc, const ImageData* data, Vec2 size, float cornerRadius);
// `data` as an icon, `side` square (no bigger than `size`), centred on whole
// pixels: a search result's (Button::picture, decision 079).
void paintPicture(ID2D1DeviceContext* dc, const ImageData* data, Vec2 size, float side);

} // namespace cup::platform
