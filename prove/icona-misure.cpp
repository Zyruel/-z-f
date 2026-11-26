// Icona di [z]f a ogni misura: quanto costa caricarla e come viene, senza cambiare il DPI dello schermo.
//   build.bat prova prove\icona-misure.cpp user32.lib gdi32.lib
//   build\icona-misure.exe <file.ico> [<cartella dove scrivere le immagini>]
//
// Fa ciò che fa LoadImageW con l'icona dentro l'exe (appIconSet in chrome.cpp): sceglie dalla
// directory l'immagine più adatta (LookupIconIdFromDirectoryEx) e crea l'icona alla misura chiesta
// (CreateIconFromResourceEx), riducendola se il .ico non ha quella misura. Per ogni misura che
// Windows chiede da 100% a 225% (16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72) dice quale immagine
// sceglie, il tempo (mediana di 200 volte) e, con la cartella, scrive l'icona che ne esce come
// RGBA grezzo (<misura>.rgba), disegnata su nero e su bianco per ricavarne l'alfa.

#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <algorithm>

#pragma pack(push, 1)
struct IcoEntry { BYTE w, h, colors, res; WORD planes, bpp; DWORD size, offset; };
struct GrpEntry { BYTE w, h, colors, res; WORD planes, bpp; DWORD size; WORD id; };
#pragma pack(pop)

static double nowMs() {
    static LARGE_INTEGER f = {};
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart * 1000.0 / f.QuadPart;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { fwprintf(stderr, L"uso: icona-misure <file.ico> [cartella]\n"); return 1; }
    FILE* f = _wfopen(argv[1], L"rb");
    if (!f) { fwprintf(stderr, L"non si apre %ls\n", argv[1]); return 1; }
    std::vector<BYTE> ico;
    BYTE buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) ico.insert(ico.end(), buf, buf + n);
    fclose(f);

    // Directory nel formato delle risorse (RT_GROUP_ICON): al posto dell'offset, l'id dell'immagine
    WORD count = *(WORD*)&ico[4];
    const IcoEntry* e = (const IcoEntry*)&ico[6];
    std::vector<BYTE> grp(6 + sizeof(GrpEntry) * count);
    memcpy(grp.data(), ico.data(), 6);
    for (WORD i = 0; i < count; i++) {
        GrpEntry g = { e[i].w, e[i].h, e[i].colors, e[i].res, e[i].planes, e[i].bpp, e[i].size, (WORD)(i + 1) };
        memcpy(&grp[6 + sizeof(GrpEntry) * i], &g, sizeof(g));
    }

    const int sizes[] = { 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72 };
    HDC dc = CreateCompatibleDC(nullptr);
    for (int s : sizes) {
        int id = LookupIconIdFromDirectoryEx(grp.data(), TRUE, s, s, LR_DEFAULTCOLOR);
        if (id < 1 || id > count) { printf("%3d px: nessuna immagine\n", s); continue; }
        const IcoEntry& img = e[id - 1];
        bool png = !memcmp(&ico[img.offset], "\x89PNG", 4);
        std::vector<double> t;
        HICON h = nullptr;
        for (int k = 0; k < 200; k++) {
            double t0 = nowMs();
            HICON x = CreateIconFromResourceEx(&ico[img.offset], img.size, TRUE, 0x00030000, s, s, LR_DEFAULTCOLOR);
            t.push_back(nowMs() - t0);
            if (k == 0) h = x; else DestroyIcon(x);
        }
        double first = t[0]; // la prima volta del processo: per un PNG può caricare il decodificatore
        std::sort(t.begin(), t.end());
        int from = img.w ? img.w : 256;
        printf("%3d px: dall'immagine di %3d (%s)%s  %.3f ms (la prima volta %.3f ms)\n", s, from, png ? "PNG" : "BMP",
               from == s ? "           " : ", ridotta", t[t.size() / 2], first);
        if (argc > 2 && h) {
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
            bi.bmiHeader.biWidth = s;
            bi.bmiHeader.biHeight = -s;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            uint32_t* bits = nullptr;
            HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
            HGDIOBJ old = SelectObject(dc, bmp);
            std::vector<uint32_t> black((size_t)s * s), out((size_t)s * s);
            for (int pass = 0; pass < 2; pass++) {
                std::fill(bits, bits + s * s, pass ? 0x00FFFFFFu : 0u);
                GdiFlush();
                DrawIconEx(dc, 0, 0, h, s, s, 0, nullptr, DI_NORMAL);
                GdiFlush();
                if (!pass) memcpy(black.data(), bits, (size_t)s * s * 4);
            }
            for (int i = 0; i < s * s; i++) { // alfa dalla differenza su nero e su bianco, poi RGBA
                int bg = (black[i] >> 8) & 0xFF, wg = (bits[i] >> 8) & 0xFF;
                int a = std::max(0, std::min(255, 255 - (wg - bg)));
                int r = a ? std::min(255, (int)((black[i] >> 16) & 0xFF) * 255 / a) : 0;
                int g = a ? std::min(255, bg * 255 / a) : 0;
                int b = a ? std::min(255, (int)(black[i] & 0xFF) * 255 / a) : 0;
                out[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
            }
            SelectObject(dc, old);
            DeleteObject(bmp);
            wchar_t path[MAX_PATH];
            swprintf(path, MAX_PATH, L"%ls\\%d.rgba", argv[2], s);
            FILE* o = _wfopen(path, L"wb");
            if (o) { fwrite(out.data(), 4, out.size(), o); fclose(o); }
        }
        if (h) DestroyIcon(h);
    }
    DeleteDC(dc);
    // Le DLL che i PNG potrebbero aver caricato (all'avvio di [z]f non ci sono)
    const wchar_t* dll[] = { L"windowscodecs.dll", L"ole32.dll", L"combase.dll", nullptr };
    for (int i = 0; dll[i]; i++) printf("%ls: %s\n", dll[i], GetModuleHandleW(dll[i]) ? "caricata" : "no");
    return 0;
}
