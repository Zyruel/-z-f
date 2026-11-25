// [z]f — firma dei rilasci (ECDSA P-256, CNG). Si compila con: build.bat firma → build\firma.exe
//
//   firma nuova               crea la chiave (una volta sola) e la sua copia di sicurezza
//   firma <file.exe>          scrive <file.exe>.sig (64 byte, r||s) e la verifica
//   firma pubblica            stampa la chiave pubblica da mettere in src\update.cpp (kUpdKey)
//   firma ripristina <file>   rimette la chiave da una copia di sicurezza (PC nuovo, Windows reinstallato)
//
// La chiave privata sta in %LOCALAPPDATA%\z-f-firma\chiave.bin, cifrata con DPAPI (la può leggere
// solo questo utente di Windows su questo PC): mai nel repository. La copia di sicurezza
// (copia-di-sicurezza.bin, non cifrata) va spostata su una chiavetta o in un gestore di password:
// senza la chiave, gli exe già distribuiti non accettano più aggiornamenti.
// La firma contiene solo numeri calcolati dall'impronta dell'exe: nessun dato del PC.

#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#endif

typedef std::vector<unsigned char> Bytes;

static std::wstring keyDir() {
    wchar_t b[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", b, MAX_PATH);
    if (!n || n >= MAX_PATH) return L"";
    return std::wstring(b) + L"\\z-f-firma";
}

static bool readFile(const std::wstring& p, Bytes& out) {
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < (256 << 20);
    if (ok) {
        out.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        ok = out.empty() || (ReadFile(h, out.data(), (DWORD)out.size(), &got, nullptr) && got == out.size());
    }
    CloseHandle(h);
    return ok;
}

static bool writeFile(const std::wstring& p, const Bytes& data) {
    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    bool ok = WriteFile(h, data.data(), (DWORD)data.size(), &w, nullptr) && w == data.size();
    CloseHandle(h);
    return ok;
}

static bool dpapi(const Bytes& in, Bytes& out, bool protect) {
    DATA_BLOB a = { (DWORD)in.size(), (BYTE*)in.data() }, b = {};
    BOOL ok = protect ? CryptProtectData(&a, L"z-f firma", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &b)
                      : CryptUnprotectData(&a, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &b);
    if (!ok) return false;
    out.assign(b.pbData, b.pbData + b.cbData);
    SecureZeroMemory(b.pbData, b.cbData);
    LocalFree(b.pbData);
    return true;
}

// Chiave privata (BCRYPT_ECCPRIVATE_BLOB) dal file cifrato
static bool loadPrivate(Bytes& blob) {
    Bytes enc;
    if (!readFile(keyDir() + L"\\chiave.bin", enc)) { fwprintf(stderr, L"Nessuna chiave: prima \"firma nuova\" (o \"firma ripristina\").\n"); return false; }
    if (!dpapi(enc, blob, false)) { fwprintf(stderr, L"La chiave non si decifra (altro utente o altro PC?): \"firma ripristina\".\n"); return false; }
    return true;
}

static bool importKey(BCRYPT_ALG_HANDLE alg, const Bytes& blob, bool priv, BCRYPT_KEY_HANDLE& key) {
    return BCryptImportKeyPair(alg, nullptr, priv ? BCRYPT_ECCPRIVATE_BLOB : BCRYPT_ECCPUBLIC_BLOB, &key,
                               (PUCHAR)blob.data(), (ULONG)blob.size(), 0) == 0;
}

static bool exportBlob(BCRYPT_KEY_HANDLE key, LPCWSTR type, Bytes& out) {
    ULONG n = 0;
    if (BCryptExportKey(key, nullptr, type, nullptr, 0, &n, 0) != 0) return false;
    out.resize(n);
    return BCryptExportKey(key, nullptr, type, out.data(), n, &n, 0) == 0;
}

static bool sha256(const Bytes& d, unsigned char h[32]) {
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_HASH_HANDLE hh;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
    bool ok = BCryptCreateHash(alg, &hh, nullptr, 0, nullptr, 0, 0) == 0;
    if (ok) {
        ok = BCryptHashData(hh, (PUCHAR)d.data(), (ULONG)d.size(), 0) == 0 && BCryptFinishHash(hh, h, 32, 0) == 0;
        BCryptDestroyHash(hh);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

// La parte pubblica di un blob privato: intestazione "ECS1" + X + Y (senza d)
static Bytes publicOf(const Bytes& priv) {
    Bytes pub(priv.begin(), priv.begin() + 8 + 64);
    pub[2] = 'S'; pub[3] = '1'; // BCRYPT_ECDSA_PUBLIC_P256_MAGIC = "ECS1"
    return pub;
}

static void printPublic(const Bytes& pub) {
    wprintf(L"static const uint8_t kUpdKey[72] = {\n");
    for (size_t i = 0; i < pub.size(); i++) wprintf(L"%s0x%02x,%s", i % 12 == 0 ? L"    " : L"", pub[i], i % 12 == 11 || i + 1 == pub.size() ? L"\n" : L" ");
    wprintf(L"};\n");
}

static int cmdNew() {
    std::wstring dir = keyDir(), file = dir + L"\\chiave.bin";
    if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES) { fwprintf(stderr, L"La chiave c'e' gia': %s\n", file.c_str()); return 1; }
    CreateDirectoryW(dir.c_str(), nullptr);
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_KEY_HANDLE key;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) return 1;
    Bytes priv, enc;
    bool ok = BCryptGenerateKeyPair(alg, &key, 256, 0) == 0 && BCryptFinalizeKeyPair(key, 0) == 0 &&
              exportBlob(key, BCRYPT_ECCPRIVATE_BLOB, priv) && dpapi(priv, enc, true) &&
              writeFile(file, enc) && writeFile(dir + L"\\copia-di-sicurezza.bin", priv);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) { fwprintf(stderr, L"Creazione della chiave non riuscita.\n"); return 1; }
    wprintf(L"Chiave creata: %s\nCopia di sicurezza: %s\\copia-di-sicurezza.bin (da spostare in un posto sicuro)\n\n", file.c_str(), dir.c_str());
    printPublic(publicOf(priv));
    SecureZeroMemory(priv.data(), priv.size());
    return 0;
}

static int cmdPublic() {
    Bytes priv;
    if (!loadPrivate(priv)) return 1;
    printPublic(publicOf(priv));
    SecureZeroMemory(priv.data(), priv.size());
    return 0;
}

static int cmdRestore(const wchar_t* from) {
    Bytes priv, enc;
    std::wstring dir = keyDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    if (!readFile(from, priv) || priv.size() != 8 + 96 || !dpapi(priv, enc, true) || !writeFile(dir + L"\\chiave.bin", enc)) {
        fwprintf(stderr, L"Ripristino non riuscito.\n");
        return 1;
    }
    wprintf(L"Chiave ripristinata.\n");
    printPublic(publicOf(priv));
    SecureZeroMemory(priv.data(), priv.size());
    return 0;
}

static int cmdSign(const wchar_t* exe) {
    Bytes priv, data;
    if (!loadPrivate(priv)) return 1;
    if (!readFile(exe, data)) { fwprintf(stderr, L"Non leggo %s\n", exe); return 1; }
    unsigned char h[32];
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_KEY_HANDLE key, pubKey;
    if (!sha256(data, h) || BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) return 1;
    Bytes sig(64);
    ULONG n = 0;
    bool ok = importKey(alg, priv, true, key) && BCryptSignHash(key, nullptr, h, 32, sig.data(), 64, &n, 0) == 0 && n == 64;
    SecureZeroMemory(priv.data(), priv.size());
    // Verifica con la sola parte pubblica, come farà [z]f
    Bytes pub;
    ok = ok && exportBlob(key, BCRYPT_ECCPUBLIC_BLOB, pub) && importKey(alg, pub, false, pubKey) &&
         BCryptVerifySignature(pubKey, nullptr, h, 32, sig.data(), 64, 0) == 0;
    BCryptCloseAlgorithmProvider(alg, 0);
    std::wstring out = std::wstring(exe) + L".sig";
    if (!ok || !writeFile(out, sig)) { fwprintf(stderr, L"Firma non riuscita.\n"); return 1; }
    wprintf(L"Firmato: %s\nsha256 ", out.c_str());
    for (int i = 0; i < 32; i++) wprintf(L"%02x", h[i]);
    wprintf(L"\n");
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && !wcscmp(argv[1], L"nuova")) return cmdNew();
    if (argc == 2 && !wcscmp(argv[1], L"pubblica")) return cmdPublic();
    if (argc == 3 && !wcscmp(argv[1], L"ripristina")) return cmdRestore(argv[2]);
    if (argc == 2) return cmdSign(argv[1]);
    fwprintf(stderr, L"Uso: firma nuova | firma <file.exe> | firma pubblica | firma ripristina <file>\n");
    return 2;
}
