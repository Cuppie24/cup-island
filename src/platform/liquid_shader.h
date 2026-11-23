#pragma once
// Liquid glass's shader (decision 165), compiled when it's first turned on.
// One pass over the island's pixels: for each, how deep inside the nearest
// shape it is, and from that where in the desktop's picture to look. Its
// numbers were tuned on a copy of it run in a browser (decision 165).

namespace cup::platform {

// Mirrors the shader's Params, register for register.
struct LiquidParams {
    float screen[4];   // the picture's size (the render layer's: it's the layer's part of the desktop)
    float shape[2][4]; // each shape's x, y, width, height (render-layer pixels); no width: none
    float radius[4];   // each shape's corner radius
    float lens[4];     // unused, rim, bend (pixels); dim
    float light[4];    // saturation, the lip's light
    float ears[4];     // each shape's flared top corners: its left one's size, its right one's (0: none)
    float tops[4];     // each shape's real top (its frame's is carried past the screen edge)
    float blur[4];     // a blur pass: one tap's step (in the source's 0..1), the Gaussian's sigma (taps), the source's mip level
    float quarter[4];  // a blur pass: its target's size
};

inline constexpr char kLiquidShader[] = R"hlsl(
Texture2D desktop : register(t0);
SamplerState clamped : register(s0);
cbuffer Params : register(b0) {
    float4 screen;
    float4 shape[2];
    float4 radius;
    float4 lens;
    float4 light;
    float4 ears;
    float4 tops;
    float4 blur;
    float4 quarter;
};

// One triangle over the whole target; the scissor keeps it to the island.
float4 vs(uint id : SV_VertexID) : SV_Position
{
    float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}

// Signed distance to a rounded rectangle: negative inside.
float distanceTo(float2 p, float4 box, float r)
{
    if (box.z <= 0 || box.w <= 0) return 1e6;
    float2 extent = box.zw / 2;
    float2 q = abs(p - (box.xy + extent)) - extent + r;
    return length(max(q, 0)) + min(max(q.x, q.y), 0) - r;
}

// A flared top corner (decision 025): beside the body's top, where its side
// curves out into the screen edge. `corner` is where that side meets the
// edge, `e` the flare's size, `side` -1 for the left one, 1 for the right.
// The curve is a circle of radius e about the flare's outer bottom corner,
// and the flare is what's outside it, down to e below the edge and as far
// into the body, so the two join as one shape.
float flareDistance(float2 p, float2 corner, float e, float side)
{
    if (e <= 0) return 1e6;
    float curve = e - length(p - (corner + float2(side * e, e)));
    return max(curve, max(abs(p.x - corner.x) - e, p.y - (corner.y + e)));
}

float shapeDistance(float2 p, float4 box, float r, float2 ear, float top)
{
    float d = distanceTo(p, box, r);
    if (box.z <= 0 || box.w <= 0) return d;
    d = min(d, flareDistance(p, float2(box.x, top), ear.x, -1));
    return min(d, flareDistance(p, float2(box.x + box.z, top), ear.y, 1));
}

float nearest(float2 p)
{
    return min(shapeDistance(p, shape[0], radius.x, ears.xy, tops.x), shapeDistance(p, shape[1], radius.y, ears.zw, tops.y));
}

float4 ps(float4 at : SV_Position) : SV_Target
{
    float2 p = at.xy;
    float d = nearest(p);
    // Which way is out: the distance's slope.
    float2 n = float2(nearest(p + float2(1, 0)) - nearest(p - float2(1, 0)), nearest(p + float2(0, 1)) - nearest(p - float2(0, 1)));
    n /= max(length(n), 1e-4);
    // Past the edge counts as on it: the outline is cut by the compositor,
    // smoothly, a pixel or so either side of where this thinks the edge is.
    // Anything that changed at the edge here would show through that cut as
    // a staircase.
    float inside = max(-d, 0);
    // The rim: 1 at the edge, 0 where it ends. There the glass shows what's
    // further out, past its own edge at the edge itself: what lies beside
    // the island wraps into its lip, as round a real pane's. The bend grows
    // as a cube: nothing at the rim's inner end, most at the edge, no steps.
    float t = lens.y > 0 ? saturate(1 - inside / lens.y) : 0;
    float2 from = p + n * lens.z * t * t * t;
    // The picture is already blurred (blurps, twice): one tap, smooth.
    float3 c = desktop.SampleLevel(clamped, from / screen.xy, 0).rgb;
    float grey = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = saturate(lerp(grey.xxx, c, light.x)) * lens.w;
    // The lip gathers light, most along the bottom. Soft, and nothing at the
    // edge's own pixel: the crisp line there is the compositor's rim.
    c += light.y * t * t * t * (0.35 + 0.65 * max(n.y, 0));
    return float4(saturate(c), 1);
}

// One way of a Gaussian blur (decision 165), run across and then down, on a
// picture a quarter the size: 17 taps there are a blur some 10 pixels wide
// here, and smooth, where reading a smaller copy alone was blocky.
float4 blurps(float4 at : SV_Position) : SV_Target
{
    float2 uv = at.xy / quarter.xy;
    float3 c = 0;
    float total = 0;
    for (int i = -8; i <= 8; ++i) {
        float weight = exp(-(i * i) / (2 * blur.z * blur.z));
        c += weight * desktop.SampleLevel(clamped, uv + i * blur.xy, blur.w).rgb;
        total += weight;
    }
    return float4(c / total, 1);
}
)hlsl";

} // namespace cup::platform
