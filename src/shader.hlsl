// Tenere allineato con kShaderSrc in gfx.cpp (usato quando fxc manca).
cbuffer C : register(b0) { float2 inv; float2 pad; };
// sh = (half width, half height, radius, border): radius > 0 is a rounded rect, uv = offset from its center
struct VI { float2 p : POSITION; float2 uv : TEXCOORD0; float4 c : COLOR; float4 sh : TEXCOORD1; };
struct VO { float4 p : SV_POSITION; float2 uv : TEXCOORD0; float4 c : COLOR; float4 sh : TEXCOORD1; };
VO vs(VI i) {
    VO o;
    o.p = float4(i.p.x * inv.x * 2.0 - 1.0, 1.0 - i.p.y * inv.y * 2.0, 0.0, 1.0);
    o.uv = i.uv; o.c = i.c; o.sh = i.sh;
    return o;
}
Texture2D t : register(t0);   // glyphs (RGBA coverage)
Texture2D ti : register(t1);  // icons (BGRA)
Texture2D tp : register(t2);  // preview (BGRA)
Texture2D tt : register(t3);  // thumbnails (BGRA)
SamplerState s : register(s0);
// Dual-source blending: o.c is the color, o.a the per-channel coverage (ClearType: one per subpixel)
struct PO { float4 c : SV_Target0; float4 a : SV_Target1; };
PO ps(VO i) {
    PO o;
    if (i.sh.z > 0.0) { // rounded rect: signed distance from the edge, 1 px antialiasing
        float2 q = abs(i.uv) - i.sh.xy + i.sh.z;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - i.sh.z;
        float a = saturate(0.5 - d);
        if (i.sh.w > 0.0) a -= saturate(0.5 - d - i.sh.w); // outline only
        o.c = float4(i.c.rgb, 1.0);
        o.a = (a * i.c.a).xxxx;
        return o;
    }
    if (i.uv.x >= 3.0) { // UV shifted by +3: thumbnail atlas (grid view)
        float4 c = tt.SampleLevel(s, float2(i.uv.x - 3.0, i.uv.y), 0);
        o.c = float4(c.rgb, 1.0);
        o.a = (c.a * i.c.a).xxxx;
        return o;
    }
    if (i.uv.x >= 2.0) { // UV shifted by +2: preview texture
        float4 c = tp.SampleLevel(s, float2(i.uv.x - 2.0, i.uv.y), 0);
        o.c = float4(c.rgb, 1.0);
        o.a = (c.a * i.c.a).xxxx;
        return o;
    }
    if (i.uv.x >= 1.0) { // UV shifted by +1: icon atlas
        float4 c = ti.SampleLevel(s, float2(i.uv.x - 1.0, i.uv.y), 0);
        o.c = float4(c.rgb, 1.0);
        o.a = (c.a * i.c.a).xxxx;
        return o;
    }
    float4 g = t.SampleLevel(s, i.uv, 0); // rgb = subpixel coverage, a = max
    o.c = float4(i.c.rgb, 1.0);
    o.a = g * i.c.a;
    return o;
}
