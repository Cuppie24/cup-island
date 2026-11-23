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

float nearest(float2 p) { return min(distanceTo(p, shape[0], radius.x), distanceTo(p, shape[1], radius.y)); }

float4 ps(float4 at : SV_Position) : SV_Target
{
    float2 p = at.xy;
    float d = nearest(p);
    // Which way is out: the distance's slope.
    float2 n = float2(nearest(p + float2(1, 0)) - nearest(p - float2(1, 0)), nearest(p + float2(0, 1)) - nearest(p - float2(0, 1)));
    n /= max(length(n), 1e-4);
    float inside = max(-d, 0);
    // The rim: 1 at the edge, 0 where it ends. A pane's rounded lip is a
    // quarter circle in section, so the bend grows that way: gentle at the
    // rim's inner end, steep at the edge. No steps.
    float t = (d < 0 && lens.y > 0) ? saturate(1 - inside / lens.y) : 0;
    float lip = 1 - sqrt(1 - t * t);
    float2 from = p - n * lens.z * lip; // what's further in shows here

    // A little blur: a spiral of taps around that point.
    float3 c = 0;
    for (int i = 0; i < 13; ++i) {
        float angle = i * 2.39996;
        float reach = lens.x * sqrt(i / 12.0);
        c += desktop.SampleLevel(clamped, (from + screen.zw + reach * float2(cos(angle), sin(angle))) / screen.xy, 0).rgb;
    }
    c /= 13;
    float grey = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = saturate(lerp(grey.xxx, c, light.x)) * lens.w;

    // The edge catches the light from the top left, and again opposite it.
    float edge = (d < 0) ? saturate(1 - inside / max(radius.z, 0.5)) : 0;
    float facing = dot(n, normalize(float2(-1, -1)));
    c += edge * light.y * facing * facing;
    return float4(saturate(c), 1);
}
)hlsl";

} // namespace cup::platform
