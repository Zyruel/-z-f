// [z]f — grafica: device D3D11, swapchain flip-model, shader, batch di quad.
// Tutto ciò che si vede è fatto di quad: rettangoli pieni, glifi e icone, in un solo vertex buffer.

struct Vtx { float x, y, u, v; uint32_t c; float hw, hh, r, bw; }; // hw..bw: rettangoli arrotondati
enum { ATLAS = 1024, MAXQ = 16384 }; // atlante glifi RGBA: 4 MB, migliaia di glifi
static const int ICON_ATLAS = 1024;  // atlante delle icone della shell (BGRA), riempito da icons.cpp

struct Gfx {
    ID3D11Device*             dev = nullptr;
    ID3D11DeviceContext*      ctx = nullptr;
    IDXGISwapChain1*          swap = nullptr;
    ID3D11RenderTargetView*   rtv = nullptr;
    ID3D11VertexShader*       vs = nullptr;
    ID3D11PixelShader*        ps = nullptr;
    ID3D11InputLayout*        il = nullptr;
    ID3D11Buffer*             vb = nullptr;
    ID3D11Buffer*             ib = nullptr;
    ID3D11Buffer*             cb = nullptr;
    ID3D11Texture2D*          atlas = nullptr;
    ID3D11ShaderResourceView* atlasSrv = nullptr;
    ID3D11Texture2D*          iconAtlas = nullptr;
    ID3D11ShaderResourceView* iconSrv = nullptr;
    ID3D11Texture2D*          prevTex = nullptr;  // anteprima: immagine o icona grande (preview.cpp)
    ID3D11ShaderResourceView* prevSrv = nullptr;
    ID3D11Texture2D*          thumbTex = nullptr; // atlante delle miniature della griglia (thumbs.cpp), creato al primo uso
    ID3D11ShaderResourceView* thumbSrv = nullptr;
    ID3D11SamplerState*       samp = nullptr;
    ID3D11BlendState*         blend = nullptr;
    ID3D11RasterizerState*    rs = nullptr;
    ID3D11Query*              doneQuery = nullptr; // per misurare quando il disegno è davvero finito
    ID3D11Query*              tsDisjoint = nullptr; // timestamp GPU: solo il lavoro, senza attese
    ID3D11Query*              tsBegin = nullptr;
    ID3D11Query*              tsEnd = nullptr;
    HANDLE                    waitable = nullptr;
    UINT                      swapFlags = 0;
    int                       w = 0, h = 0;
    Vtx*                      mapped = nullptr;
    int                       nq = 0;
    double                    deviceMs = 0, resMs = 0, swapMs = 0;
    wchar_t                   adapterName[128] = L"?";
    bool                      isWarp = false;
};
static Gfx G;     // dispositivo corrente (quello che disegna)
static Gfx g_hw;  // GPU preparata in background (modalità ibrida)

// Copia di shader.hlsl per quando gli shader non sono precompilati (zig, o MSVC senza fxc): se
// cambia shader.hlsl va cambiata anche questa. È l'unico doppione voluto (prove\da-verificare.md).
static const char* kShaderSrc = R"(
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
Texture2D t : register(t0);   // glifi (copertura RGBA)
Texture2D ti : register(t1);  // icone (BGRA)
Texture2D tp : register(t2);  // anteprima (BGRA)
Texture2D tt : register(t3);  // miniature della griglia (BGRA)
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
)";

typedef HRESULT(WINAPI* PFN_D3DCompile_)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*,
                                         LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

static bool g_shadersPrecompiled = false;
static bool compileShaders(std::vector<uint8_t>& vsCode, std::vector<uint8_t>& psCode) {
#ifdef ZF_PRECOMPILED_SHADERS
    vsCode.assign(g_vs, g_vs + sizeof(g_vs));
    psCode.assign(g_ps, g_ps + sizeof(g_ps));
    g_shadersPrecompiled = true;
    return true;
#else
    HMODULE m = LoadLibraryW(L"d3dcompiler_47.dll");
    if (!m) return false;
    PFN_D3DCompile_ compile = (PFN_D3DCompile_)GetProcAddress(m, "D3DCompile");
    if (!compile) return false;
    ID3DBlob *b = nullptr, *err = nullptr;
    const UINT flags = (1 << 15); // D3DCOMPILE_OPTIMIZATION_LEVEL3
    if (FAILED(compile(kShaderSrc, strlen(kShaderSrc), "z-f", nullptr, nullptr, "vs", "vs_4_0", flags, 0, &b, &err))) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        return false;
    }
    vsCode.assign((uint8_t*)b->GetBufferPointer(), (uint8_t*)b->GetBufferPointer() + b->GetBufferSize());
    b->Release();
    if (FAILED(compile(kShaderSrc, strlen(kShaderSrc), "z-f", nullptr, nullptr, "ps", "ps_4_0", flags, 0, &b, &err))) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        return false;
    }
    psCode.assign((uint8_t*)b->GetBufferPointer(), (uint8_t*)b->GetBufferPointer() + b->GetBufferSize());
    b->Release();
    return true;
#endif
}

// Shader compilati una sola volta per processo, anche se i device sono due (WARP e GPU)
// e vengono creati su thread diversi.
static std::vector<uint8_t> g_vsCode, g_psCode;
static bool g_shadersOk = false;
static double g_shaderMs = 0;
static INIT_ONCE g_shaderOnce = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK shaderOnceCb(PINIT_ONCE, PVOID, PVOID*) {
    double t0 = nowMs();
    g_shadersOk = compileShaders(g_vsCode, g_psCode);
    g_shaderMs = nowMs() - t0;
    return TRUE;
}

// Modalità grafica (riga di comando, per diagnosi)
//   (nessuna)   ibrido: si parte con WARP (nessun driver da caricare), poi GPU appena è pronta
//   --gpu=hw    solo GPU (attende il driver)
//   --gpu=warp  solo rendering software
// Provata anche la scelta esplicita della GPU a basso consumo: con il monitor collegato
// alla scheda dedicata costa di più (copia tra schede), quindi non c'è.
enum GpuPref { GPU_HYBRID, GPU_HW, GPU_WARP };
static int g_gpuPref = GPU_HYBRID;
static const wchar_t* kGpuPrefName[] = { L"ibrido", L"--gpu=hw", L"--gpu=warp" };

// Crea device e tutte le risorse che non dipendono dalla finestra. Chiamabile da qualsiasi thread.
static bool gfxCreateDevice(Gfx& g, bool warp) {
    D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
    double tDev = nowMs();
    HRESULT hr = D3D11CreateDevice(nullptr, warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   fls, 3, D3D11_SDK_VERSION, &g.dev, &got, &g.ctx);
    if (FAILED(hr)) return false;
    g.deviceMs = nowMs() - tDev;
    g.isWarp = warp;

    IDXGIDevice* dxdev = nullptr;
    IDXGIAdapter* adapter = nullptr;
    if (SUCCEEDED(g.dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxdev))) {
        if (SUCCEEDED(dxdev->GetAdapter(&adapter))) {
            DXGI_ADAPTER_DESC ad;
            if (SUCCEEDED(adapter->GetDesc(&ad))) {
                wcsncpy(g.adapterName, ad.Description, 127);
                g.adapterName[127] = 0;
            }
            adapter->Release();
        }
        dxdev->Release();
    }

    double tRes = nowMs();
    InitOnceExecuteOnce(&g_shaderOnce, shaderOnceCb, nullptr, nullptr);
    if (!g_shadersOk) return false;
    g.dev->CreateVertexShader(g_vsCode.data(), g_vsCode.size(), nullptr, &g.vs);
    g.dev->CreatePixelShader(g_psCode.data(), g_psCode.size(), nullptr, &g.ps);
    D3D11_INPUT_ELEMENT_DESC ied[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g.dev->CreateInputLayout(ied, 4, g_vsCode.data(), g_vsCode.size(), &g.il);

    // Vertex buffer dinamico + index buffer statico
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(Vtx) * 4 * MAXQ;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g.dev->CreateBuffer(&bd, nullptr, &g.vb);

    std::vector<uint16_t> idx(MAXQ * 6);
    for (int q = 0; q < MAXQ; q++) {
        uint16_t b = (uint16_t)(q * 4);
        uint16_t* p = &idx[q * 6];
        p[0] = b; p[1] = b + 1; p[2] = b + 2; p[3] = b + 2; p[4] = b + 1; p[5] = b + 3;
    }
    bd = {};
    bd.ByteWidth = (UINT)(idx.size() * sizeof(uint16_t));
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA srd = { idx.data(), 0, 0 };
    g.dev->CreateBuffer(&bd, &srd, &g.ib);

    bd = {};
    bd.ByteWidth = 16;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    g.dev->CreateBuffer(&bd, nullptr, &g.cb);

    // Atlante glifi (con un blocco bianco 4x4 in (0,0) per i rettangoli)
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = ATLAS;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // copertura per subpixel (ClearType)
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    g.dev->CreateTexture2D(&td, nullptr, &g.atlas);
    g.dev->CreateShaderResourceView(g.atlas, nullptr, &g.atlasSrv);
    td.Width = td.Height = ICON_ATLAS;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    g.dev->CreateTexture2D(&td, nullptr, &g.iconAtlas);
    g.dev->CreateShaderResourceView(g.iconAtlas, nullptr, &g.iconSrv);
    uint8_t white[4 * 4 * 4]; memset(white, 255, sizeof(white));
    D3D11_BOX box = { 0, 0, 0, 4, 4, 1 };
    g.ctx->UpdateSubresource(g.atlas, 0, &box, white, 16, 0);

    D3D11_SAMPLER_DESC smp = {};
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    g.dev->CreateSamplerState(&smp, &g.samp);

    D3D11_BLEND_DESC bl = {};
    bl.RenderTarget[0].BlendEnable = TRUE;
    // Dual-source: colore × copertura per canale (dal secondo output dello shader). Per rettangoli
    // e icone la copertura è uguale sui tre canali, cioè il normale blend con l'alfa.
    bl.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC1_COLOR;
    bl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_COLOR;
    bl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC1_ALPHA;
    bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
    bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g.dev->CreateBlendState(&bl, &g.blend);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    g.dev->CreateRasterizerState(&rd, &g.rs);

    D3D11_QUERY_DESC qd = { D3D11_QUERY_EVENT, 0 };
    g.dev->CreateQuery(&qd, &g.doneQuery);
    qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    g.dev->CreateQuery(&qd, &g.tsDisjoint);
    qd.Query = D3D11_QUERY_TIMESTAMP;
    g.dev->CreateQuery(&qd, &g.tsBegin);
    g.dev->CreateQuery(&qd, &g.tsEnd);
    g.resMs = nowMs() - tRes;
    return true;
}

// Senza buffer (device perso a metà di un ridimensionamento) resta senza rtv: render.cpp non disegna
static void createRtv(Gfx& g) {
    ID3D11Texture2D* bb = nullptr;
    if (FAILED(g.swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb)) || !bb) return;
    if (FAILED(g.dev->CreateRenderTargetView(bb, nullptr, &g.rtv))) g.rtv = nullptr;
    bb->Release();
}

// Swapchain sulla finestra di rendering. Va chiamata dal thread della finestra.
static bool gfxCreateSwapchain(Gfx& g, HWND view, HWND top) {
    double tSwap = nowMs();
    IDXGIDevice* dxdev = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIFactory2* factory = nullptr;
    if (SUCCEEDED(g.dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxdev)) && dxdev &&
        SUCCEEDED(dxdev->GetAdapter(&adapter)) && adapter)
        adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);
    if (!factory) { safeRelease(adapter); safeRelease(dxdev); return false; }

    RECT rc;
    GetClientRect(view, &rc);
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = (UINT)std::max(1L, rc.right);
    sd.Height = (UINT)std::max(1L, rc.bottom);
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;               // Win10+
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT; // latenza input minima
    HRESULT hr = factory->CreateSwapChainForHwnd(g.dev, view, &sd, nullptr, nullptr, &g.swap);
    if (FAILED(hr)) { // Win8.x
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        hr = factory->CreateSwapChainForHwnd(g.dev, view, &sd, nullptr, nullptr, &g.swap);
    }
    if (FAILED(hr)) { // Win7
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd.BufferCount = 1;
        sd.Flags = 0;
        hr = factory->CreateSwapChainForHwnd(g.dev, view, &sd, nullptr, nullptr, &g.swap);
    }
    if (SUCCEEDED(hr)) factory->MakeWindowAssociation(top, DXGI_MWA_NO_ALT_ENTER);
    factory->Release(); adapter->Release(); dxdev->Release();
    if (FAILED(hr)) return false;
    g.swapFlags = sd.Flags;
    g.w = (int)sd.Width;
    g.h = (int)sd.Height;

    if (g.swapFlags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) {
        IDXGISwapChain2* sc2 = nullptr;
        if (SUCCEEDED(g.swap->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2))) {
            sc2->SetMaximumFrameLatency(1);
            g.waitable = sc2->GetFrameLatencyWaitableObject();
            sc2->Release();
        }
    }
    createRtv(g);
    g.swapMs = nowMs() - tSwap;
    return true;
}


// Copia le prime `rows` righe di una texture da un device all'altro (passaggio WARP → GPU).
// Il device WARP vive in RAM: leggerlo costa poco, molto meno che rasterizzare di nuovo glifi e icone.
static void gfxCopyRows(Gfx& from, ID3D11Texture2D* src, Gfx& to, ID3D11Texture2D* dst, UINT rows) {
    if (!src || !dst || !rows) return;
    D3D11_TEXTURE2D_DESC d;
    src->GetDesc(&d);
    rows = std::min(rows, d.Height);
    d.Height = rows;
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.MiscFlags = 0;
    ID3D11Texture2D* st = nullptr;
    if (FAILED(from.dev->CreateTexture2D(&d, nullptr, &st))) return;
    D3D11_BOX box = { 0, 0, 0, d.Width, rows, 1 };
    from.ctx->CopySubresourceRegion(st, 0, 0, 0, 0, src, 0, &box);
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(from.ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
        to.ctx->UpdateSubresource(dst, 0, &box, m.pData, m.RowPitch, 0);
        from.ctx->Unmap(st, 0);
    }
    st->Release();
}

static void gfxRelease(Gfx& g) {
    if (g.ctx) { g.ctx->ClearState(); g.ctx->Flush(); }
    safeRelease(g.doneQuery); safeRelease(g.tsDisjoint); safeRelease(g.tsBegin); safeRelease(g.tsEnd); safeRelease(g.rs); safeRelease(g.blend); safeRelease(g.samp);
    safeRelease(g.atlasSrv); safeRelease(g.atlas); safeRelease(g.iconSrv); safeRelease(g.iconAtlas); safeRelease(g.prevSrv); safeRelease(g.prevTex); safeRelease(g.thumbSrv); safeRelease(g.thumbTex); safeRelease(g.cb); safeRelease(g.ib); safeRelease(g.vb);
    safeRelease(g.il); safeRelease(g.ps); safeRelease(g.vs); safeRelease(g.rtv); safeRelease(g.swap);
    if (g.waitable) { CloseHandle(g.waitable); g.waitable = nullptr; }
    safeRelease(g.ctx); safeRelease(g.dev);
}

static void gfxResize(int w, int h) {
    if (!G.swap || w <= 0 || h <= 0) return;
    if (w == G.w && h == G.h) return;
    G.ctx->OMSetRenderTargets(0, nullptr, nullptr);
    if (G.rtv) { G.rtv->Release(); G.rtv = nullptr; }
    G.swap->ResizeBuffers(0, (UINT)w, (UINT)h, DXGI_FORMAT_UNKNOWN, G.swapFlags);
    createRtv(G);
    G.w = w; G.h = h;
}

static void mapVB() {
    D3D11_MAPPED_SUBRESOURCE m;
    G.nq = 0;
    // Fallisce con la GPU persa (driver aggiornato o reimpostato): senza buffer quad() non scrive
    // niente, e Present segnala la perdita a render(), che ne fa creare un altro (hybrid.cpp)
    if (FAILED(G.ctx->Map(G.vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) { G.mapped = nullptr; return; }
    G.mapped = (Vtx*)m.pData;
}

static void flushQuads(bool remap = true) {
    if (!G.mapped) return;
    G.ctx->Unmap(G.vb, 0);
    if (G.nq) G.ctx->DrawIndexed((UINT)G.nq * 6, 0, 0);
    G.mapped = nullptr;
    G.nq = 0;
    if (remap) mapVB();
}

static inline void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t c) {
    if (G.nq >= MAXQ) flushQuads();
    if (!G.mapped) return; // GPU persa
    Vtx* v = G.mapped + G.nq * 4;
    v[0] = { x0, y0, u0, v0, c, 0, 0, 0, 0 };
    v[1] = { x1, y0, u1, v0, c, 0, 0, 0, 0 };
    v[2] = { x0, y1, u0, v1, c, 0, 0, 0, 0 };
    v[3] = { x1, y1, u1, v1, c, 0, 0, 0, 0 };
    G.nq++;
}

// Rettangolo con angoli arrotondati (raggio r, in pixel). border > 0: solo il contorno, di quello
// spessore. La forma la calcola lo shader (distanza dal bordo), con 1 px di antialiasing.
static inline void rrect(float x, float y, float w, float h, float r, uint32_t c, float border = 0) {
    if (w <= 0 || h <= 0) return;
    r = std::min(r, std::min(w, h) * 0.5f);
    if (r < 0.5f && border <= 0) { // niente da arrotondare: rettangolo normale
        const float u = 1.0f / ATLAS;
        quad(x, y, x + w, y + h, u, u, u, u, c);
        return;
    }
    if (G.nq >= MAXQ) flushQuads();
    if (!G.mapped) return; // GPU persa
    Vtx* v = G.mapped + G.nq * 4;
    float hw = w * 0.5f, hh = h * 0.5f, rr = std::max(r, 0.01f);
    v[0] = { x, y, -hw, -hh, c, hw, hh, rr, border };
    v[1] = { x + w, y, hw, -hh, c, hw, hh, rr, border };
    v[2] = { x, y + h, -hw, hh, c, hw, hh, rr, border };
    v[3] = { x + w, y + h, hw, hh, c, hw, hh, rr, border };
    G.nq++;
}

static inline void rect(float x, float y, float w, float h, uint32_t c) {
    const float u = 1.0f / ATLAS; // centro del blocco bianco 4x4 in (0,0)
    quad(x, y, x + w, y + h, u, u, u, u, c);
}
