#pragma once
// One Composition visual per laid-out content item (core Placed), diffed by
// key: new items fade in, gone items fade out, changed items are redrawn.
// Progress bars, waveforms and swimming text animate on the compositor, so a
// playing track costs no app CPU per frame (TR §5).

#include "core/layout.h"
#include "platform/element_art.h"
#include "platform/graphics_device.h"
#include "platform/list_viewports.h"
#include "platform/progress_wave.h"
#include "platform/rolling_text.h"
#include "platform/segments_parts.h"
#include "platform/shine.h"
#include "platform/switch_parts.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cup::platform {

class ElementVisuals {
public:
    void init(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent);
    // On the island (not a provider's window): on liquid glass, filled
    // controls get a shiny edge (decision 165). Before any content.
    void setShiny(bool on) { shiny_ = on; }
    // `reveal`: new content starts arriving this much later, when the morph
    // that brought it is far enough along (decision 080).
    // `glide`: what moved glides to its new place instead of jumping there (a window's page, the
    // same size and page as before: a row opened above pushes it down, decision 132). Off while the
    // layout follows a resize, and for what's carried (Placed::carried).
    void apply(const std::vector<Placed>& elements, Ms now, Ms reveal = Ms{ 0 }, bool glide = false);
    // Tokens changed. Colours only (a theme, a cover): shapes fade to the new
    // colours and text is redrawn in place. Anything else (tokens.json, display
    // scale): every visual is dropped, so the next apply() rebuilds it.
    void refresh(bool coloursOnly);

    // Pointer feedback on content (decision 002), by element key. Unknown keys are ignored.
    void setHovered(const std::string& key); // "" = nothing; buttons light up, progress shows its knob
    // "<slot>/<row>" under the pointer, "" = none: reveals its revealOnHover buttons; `slot`, the
    // card under it, its revealWithSlot ones (a window card's handle, decision 129).
    void setHoveredRow(const std::string& row, const std::string& slot = {});
    // The tab under the pointer (Tab::id, "" = Home), none = std::nullopt: its muted content
    // (Placed::muted) is drawn in colour and at full strength meanwhile (decision 072).
    void setHoveredTab(const std::optional<std::string>& tab);
    void press(const std::string& key);      // buttons
    void release(const std::string& key);
    void scrub(const std::string& key, float fraction); // progress follows the pointer and ignores updates
    void endScrub(const std::string& key, std::optional<float> seekTo, Ms now); // nullopt = cancelled

private:
    struct View {
        Placed placed;
        wuc::Visual visual{ nullptr };
        // A button's glyph sprite, and the filled circle under it (a primary
        // button, a toggle that's on): a shape whose colour fades (decision 101).
        wuc::SpriteVisual glyphHost{ nullptr };
        wuc::CompositionColorBrush buttonFill{ nullptr };
        // A search card's field on the island: the island's colour, solid, under its box (solidBacked, decision 176).
        wuc::CompositionColorBrush backing{ nullptr };
        Shine shine; // ...and on liquid glass, its shiny edge (decision 165), or a large plain button's; only where shiny_
        bool shineHost = false; // shine is made and in a visual
        // A one-colour text's colour: it's painted white and shown through
        // this brush, so a recolour fades it like a shape (decision 102).
        wuc::CompositionColorBrush ink{ nullptr };
        Color inkTarget{ 0, 0, 0, 0 };
        float lead = 0; // the visual starts this far left of the frame (knob overhang, marquee fade)
        wuc::CompositionDrawingSurface surface{ nullptr };          // text, icon, button glyph, image
        float marqueeWidth = 0;                                     // > 0: swimming text, one copy's width
        wuc::Visual highlight{ nullptr };                           // button hover shape
        ArtParts art;                                               // clickable art: hover dim + "open" glyph
        wuc::CompositionDrawingSurface glyphSurface{ nullptr };     // that glyph
        wuc::SpriteVisual picture{ nullptr };                       // an Image's: a new picture fades in over it
        wuc::CompositionDrawingSurface shadowShape{ nullptr };      // raised art: its rounded shape, the shadow's mask (decision 094)
        wuc::DropShadow shadow{ nullptr };                          // ...and the shadow, its colour faded on a recolour
        wuc::CompositionRoundedRectangleGeometry fill{ nullptr };   // progress bar
        wuc::CompositionEllipseGeometry arc{ nullptr };             // progress ring
        wuc::CompositionSpriteShape knob{ nullptr };                // progress
        WaveParts wave;                                             // a wavy bar's wave (decision 167)
        bool pinned = false;                                        // ...and its knob always shown
        std::vector<wuc::CompositionRoundedRectangleGeometry> bars; // waveform
        wuc::ShapeVisual waveform{ nullptr };
        bool scrubbing = false;
        // Shape colours, kept so a recolour fades them (decision 051); null when unused.
        wuc::CompositionColorBrush track{ nullptr }, accent{ nullptr }, knobBrush{ nullptr }, hover{ nullptr };
        SwitchParts toggle; // a Switch's track and knob (decision 053)
        SegmentsParts segments; // Segments' track, sliding pill and labels (decision 093)
        RollingText rolling;    // a Hero time: a sprite per character, rolling as it changes (decision 107)
        // A focused field's caret (decision 131): its own sprite over the text, blinking on the compositor.
        wuc::SpriteVisual caret{ nullptr };
        Rect caretFrame; // ...where it is now
        bool lit = false;   // a secondary button under the pointer: drawn at full strength (decision 064)
        bool grey = false;  // drawn muted now: an inactive tab's, its tab not hovered (decision 072)
    };

    // fade: fades in (not when a text turns into its field). arrive: new, not
    // rebuilt: also grows in, this long after the top row (decision 080).
    View create(const Placed& placed, Ms now, bool fade = true, std::optional<Ms> arrive = std::nullopt); // arrive: its row's stagger
    bool update(View& view, const Placed& placed, Ms now); // true = rebuilt
    void paint(View& view);
    void recolor(View& view); // fades its shapes to the current colours and redraws its pixels
    void syncFill(View& view, Ms duration); // a button's filled circle to its colour now: made, or faded
    void fadeInk(View& view, Ms duration);  // a one-colour text's brush to its colour now (decision 102)
    void animateBars(View& view);
    void syncCaret(View& view); // a field's caret where its text says, blinking anew after each change (decision 131)
    void showHover(const std::string& key, bool on);
    bool greyNow(const Placed& placed) const;
    void setGrey(View& view, bool grey);
    void fadeOutAndRemove(wuc::Visual const& visual);
    void place(View& view); // its offset, in its list's scroller or the content

    wuc::Compositor compositor_{ nullptr };
    GraphicsDevice* device_ = nullptr;
    wuc::ContainerVisual parent_{ nullptr };
    std::map<std::string, View> views_;
    ListViewports lists_; // scrolling lists' clipped, eased containers (decision 029)
    std::string hovered_;
    std::string hoveredRow_, hoveredSlot_;
    bool revealedNow(const Placed& placed) const; // a reveal-on-hover item: shown now
    Ms reveal_{}; // the latest apply()'s: what's built during it waits this long
    bool glide_ = false; // ...and whether what moves in it glides
    std::optional<std::string> hoveredTab_;
    bool deviceLost_ = false;
    bool shiny_ = false; // setShiny()
};

} // namespace cup::platform
