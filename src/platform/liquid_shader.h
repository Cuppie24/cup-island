#pragma once
// Liquid glass's shader (decision 165), compiled when it's first turned on.
// One pass over the island's pixels: for each, how deep inside the nearest
// shape it is, and from that where in the desktop's picture to look.

namespace cup::platform {

// Mirrors the shader's Params, register for register.
struct LiquidParams {
    float screen[4];   // the picture's size; the render layer's origin in it
    float shape[2][4]; // each shape's x, y, width, height (render-layer pixels); no width: none
    float radius[4];   // each shape's corner radius; the highlight's width
    float lens[4];     // blur, rim, bend (pixels); dim
    float light[4];    // saturation, highlight
    float ears[4];     // each shape's flared top corners: its left one's size, its right one's (0: none)
    float tops[4];     // each shape's real top (its frame's is carried past the screen edge)
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
    // The rim: 1 at the edge, 0 where it ends. The bend grows as its square:
    // gentle at the rim's inner end, steepest at the edge, but never so
    // steep that the desktop is squeezed into single pixels there (a
    // circle's section is, and it glittered). No steps.
    float t = lens.y > 0 ? saturate(1 - inside / lens.y) : 0;
    float2 from = p - n * lens.z * t * t; // what's further in shows here
    // How many of the desktop's pixels each of ours covers there: blurred
    // over as many, so the squeezed picture is smooth, not sampled in dots.
    float squeeze = lens.y > 0 ? abs(lens.z) * 2 * t / lens.y : 0;

    // A little blur: a spiral of taps around that point.
    float3 c = 0;
    for (int i = 0; i < 13; ++i) {
        float angle = i * 2.39996;
        float reach = (lens.x + 1.5 * squeeze) * sqrt(i / 12.0);
        c += desktop.SampleLevel(clamped, (from + screen.zw + reach * float2(cos(angle), sin(angle))) / screen.xy, 0).rgb;
    }
    c /= 13;
    float grey = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = saturate(lerp(grey.xxx, c, light.x)) * lens.w;

    // The edge catches the light from the top left, and again opposite it:
    // a line just inside the edge, fading out both ways, and the same past
    // the edge as on it.
    float width = max(radius.z, 1);
    float edge = 1 - smoothstep(0, width, abs(inside - width / 2) * 2);
    float facing = dot(n, normalize(float2(-1, -1)));
    c += edge * light.y * facing * facing;
    return float4(saturate(c), 1);
}
)hlsl";

} // namespace cup::platform
