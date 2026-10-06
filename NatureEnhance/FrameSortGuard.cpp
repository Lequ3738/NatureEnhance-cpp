#include "FrameSortGuard.h"
#include "RunnerHook.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <windows.h>

// The frame function (GM80_RunFrame) ends its phases with the animation
// section: an indexed walk of the room instance array that fires each
// instance's Animation End event. The frame's draw driver hangs off that
// event, and every draw pass enters ScreenRegion, whose prologue sorts the
// same array by depth in place. That is harmless while the array is ordered,
// but an instance created earlier in the frame leaves it unsorted, so the
// sort permutes the array under the running walk: shifted instances are
// visited twice in one frame, which renders the whole frame twice and lets
// the first pass consume every one-shot per-frame draw gate.
//
// 1. GM80_RunFrame's camera-advance call is replaced by a stub that sorts the
//    room array once, before the walk starts, then runs the original.
// 2. The frame function's last call (sub_513EC8) is replaced by a stub that
//    lifts the guard again, so draws outside the animation section keep
//    sorting normally.
// 3. ScreenRegion's sort call is replaced by a stub that skips the sort while
//    the guard is set - the array was just sorted, and any permutation
//    between the walk's iterations is exactly the hazard being prevented.

namespace framesort {

using gm80hook::base;

// Debug-build diagnostics (framesort.log next to this DLL); silent in Release.
static void Log(const char* msg);
static void LogSiteMismatch(const char* site, std::uint32_t rva,
                            const unsigned char* expect, std::size_t len);

static bool g_installed = false;

// Set while GM80_RunFrame is inside its animation section. Read by the
// emitted guard stub through a baked absolute address; the runner is
// single-threaded, so a plain byte suffices. Volatile so the explicit
// sort in beginAnimSection cannot be reordered around the flag writes.
static volatile unsigned char g_animGuard = 0;

// The sort the begin-stub performs, resolved at install.
static void* g_sortEntry = nullptr;

// Emitted stubs and the patched call bytes kept for restore.
static std::uint8_t* g_stubGuard = nullptr;
static std::uint8_t* g_stubBegin = nullptr;
static std::uint8_t* g_stubEnd = nullptr;
static unsigned char g_patchGuard[5] = {};
static unsigned char g_patchBegin[5] = {};
static unsigned char g_patchEnd[5] = {};

// ---------------------------------------------------------------- stub memory

static std::uint8_t* allocStub(std::size_t size) {
    constexpr std::size_t kPoolSize = 4096;
    static std::uint8_t* pool = nullptr;
    static std::size_t used = 0;

    if (!pool) {
        pool = reinterpret_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, kPoolSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!pool)
            return nullptr;
    }
    size = (size + 15) & ~std::size_t(15);
    if (used + size > kPoolSize)
        return nullptr;
    std::uint8_t* mem = pool + used;
    used += size;
    return mem;
}

// ---------------------------------------------------------------- emitted stubs

// Guard stub for the ScreenRegion sort call (17 bytes):
//   cmp byte ptr [guard], 0 / jne +7 (ret) / mov ecx, sort / jmp ecx / ret
// The run path leaves eax (the sort's room argument) untouched.
static void emitGuardStub(std::uint8_t* s) {
    const std::uint32_t guardAddr =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_animGuard));
    const std::uint32_t sortAddr =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_sortEntry));

    s[0] = 0x80; // cmp byte ptr [guardAddr], 0
    s[1] = 0x3D;
    std::memcpy(s + 2, &guardAddr, 4);
    s[6] = 0x00;
    s[7] = 0x75; // jne +7 -> ret (guard set: skip the sort)
    s[8] = 0x07;
    s[9] = 0xB9; // mov ecx, sortAddr
    std::memcpy(s + 10, &sortAddr, 4);
    s[14] = 0xFF; // jmp ecx (tail: the sort's ret returns to the caller)
    s[15] = 0xE1;
    s[16] = 0xC3; // ret
}

// Call-site stub for a frame-function call (18 bytes): full register save,
// call the C hook, restore, then tail-jump into the original function so its
// ret returns to the frame function exactly like the original call.
static void emitHookCallStub(std::uint8_t* s, void* hookFn, void* origFn) {
    const std::uint32_t hookAddr =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(hookFn));
    const std::uint32_t origAddr =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(origFn));

    s[0] = 0x60; // pushad
    s[1] = 0x9C; // pushfd
    s[2] = 0xB9; // mov ecx, hookAddr
    std::memcpy(s + 3, &hookAddr, 4);
    s[7] = 0xFF; // call ecx
    s[8] = 0xD1;
    s[9] = 0x9D;  // popfd
    s[10] = 0x61; // popad
    s[11] = 0xB9; // mov ecx, origAddr
    std::memcpy(s + 12, &origAddr, 4);
    s[16] = 0xFF; // jmp ecx
    s[17] = 0xE1;
}

// ---------------------------------------------------------------- frame hooks

// GM80_InstArraySortByDepth takes the room in eax (no C has that
// convention), so hand it over in ecx and tail-jump: the sort's ret returns
// to this function's caller.
__declspec(naked) static void __fastcall callSortEax(void* /* room in ecx */) {
    __asm {
        mov eax, ecx
        jmp dword ptr [g_sortEntry]
    }
}

// Runs in place of the frame's camera-advance call: sort once here so every
// draw in the animation section sees the depth order the draw-entry sort
// would have produced, but before the indexed walk starts.
static void beginAnimSection() {
    g_animGuard = 0;
    void* room = *reinterpret_cast<void* volatile*>(
        *reinterpret_cast<void* volatile*>(base() + gm80hook::RVA_CurRoomPtr));
    if (room)
        callSortEax(room);
    g_animGuard = 1;
}

// Runs in place of the frame function's last call: from here on, draws
// outside the animation section keep sorting normally again.
static void endAnimSection() {
    g_animGuard = 0;
}

// ---------------------------------------------------------------- patching

static bool rel32Fits(std::intptr_t rel) {
    return rel >= INT32_MIN && rel <= INT32_MAX;
}

// Writes `call stub` over the verified call site; `written` keeps the bytes
// for the restore in Uninstall.
static bool patchCall(std::uint32_t rva, const unsigned char* expect,
                      std::uint8_t* stub, unsigned char* written) {
    const std::intptr_t rel =
        reinterpret_cast<std::uint8_t*>(stub) - (base() + rva + 5);
    if (!rel32Fits(rel))
        return false;

    written[0] = 0xE8;
    std::memcpy(written + 1, &rel, 4);
    return gm80hook::patchBytes(rva, expect, written, 5);
}

bool Install() {
    if (g_installed)
        return true;

    if (std::memcmp(base() + gm80hook::RVA_InstArraySort, gm80hook::SIG_InstArraySort,
                    sizeof gm80hook::SIG_InstArraySort) != 0) {
        LogSiteMismatch("InstArraySort", gm80hook::RVA_InstArraySort,
                        gm80hook::SIG_InstArraySort, sizeof gm80hook::SIG_InstArraySort);
        return false;
    }
    g_sortEntry = base() + gm80hook::RVA_InstArraySort;

    // Verify every call site up front: any mismatch leaves the process
    // completely untouched.
    if (std::memcmp(base() + gm80hook::RVA_SortCallInScreenRegion,
                    gm80hook::SIG_SortCallInScreenRegion, 5) != 0 ||
        std::memcmp(base() + gm80hook::RVA_FrameViewAdvanceCall,
                    gm80hook::SIG_FrameViewAdvanceCall, 5) != 0 ||
        std::memcmp(base() + gm80hook::RVA_FrameEndCall,
                    gm80hook::SIG_FrameEndCall, 5) != 0) {
        Log("framesort: call-site signature mismatch, feature disabled.");
        return false;
    }

    g_stubGuard = allocStub(32);
    g_stubBegin = allocStub(32);
    g_stubEnd = allocStub(32);
    if (!g_stubGuard || !g_stubBegin || !g_stubEnd) {
        Log("framesort: stub allocation failed, feature disabled.");
        return false;
    }

    emitGuardStub(g_stubGuard);
    emitHookCallStub(g_stubBegin, reinterpret_cast<void*>(&beginAnimSection),
                     base() + gm80hook::RVA_ViewSpeedAdvance);
    emitHookCallStub(g_stubEnd, reinterpret_cast<void*>(&endAnimSection),
                     base() + gm80hook::RVA_FrameEndFn);

    if (!patchCall(gm80hook::RVA_SortCallInScreenRegion,
                   gm80hook::SIG_SortCallInScreenRegion, g_stubGuard, g_patchGuard)) {
        LogSiteMismatch("SortCallInScreenRegion", gm80hook::RVA_SortCallInScreenRegion,
                        gm80hook::SIG_SortCallInScreenRegion, 5);
        return false;
    }
    if (!patchCall(gm80hook::RVA_FrameViewAdvanceCall,
                   gm80hook::SIG_FrameViewAdvanceCall, g_stubBegin, g_patchBegin)) {
        gm80hook::patchBytes(gm80hook::RVA_SortCallInScreenRegion, g_patchGuard,
                             gm80hook::SIG_SortCallInScreenRegion, 5);
        LogSiteMismatch("FrameViewAdvanceCall", gm80hook::RVA_FrameViewAdvanceCall,
                        gm80hook::SIG_FrameViewAdvanceCall, 5);
        return false;
    }
    if (!patchCall(gm80hook::RVA_FrameEndCall,
                   gm80hook::SIG_FrameEndCall, g_stubEnd, g_patchEnd)) {
        gm80hook::patchBytes(gm80hook::RVA_FrameViewAdvanceCall, g_patchBegin,
                             gm80hook::SIG_FrameViewAdvanceCall, 5);
        gm80hook::patchBytes(gm80hook::RVA_SortCallInScreenRegion, g_patchGuard,
                             gm80hook::SIG_SortCallInScreenRegion, 5);
        LogSiteMismatch("FrameEndCall", gm80hook::RVA_FrameEndCall,
                        gm80hook::SIG_FrameEndCall, 5);
        return false;
    }

    g_installed = true;
    Log("framesort: installed (animation-section sort guard active)");
    return true;
}

void Uninstall() {
    if (!g_installed)
        return;
    // Reverse of Install, restoring each site from the bytes actually
    // written. The host frees this DLL while the runner is still executing
    // GML, so nothing runner-side may keep pointing at this image.
    gm80hook::patchBytes(gm80hook::RVA_FrameEndCall, g_patchEnd,
                         gm80hook::SIG_FrameEndCall, 5);
    gm80hook::patchBytes(gm80hook::RVA_FrameViewAdvanceCall, g_patchBegin,
                         gm80hook::SIG_FrameViewAdvanceCall, 5);
    gm80hook::patchBytes(gm80hook::RVA_SortCallInScreenRegion, g_patchGuard,
                         gm80hook::SIG_SortCallInScreenRegion, 5);
    g_animGuard = 0;
    g_installed = false;
}

// ---------------------------------------------------------------- diagnostics

#ifdef _DEBUG
static bool appendLine(const char* path, const char* line, int len) {
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    WriteFile(h, line, static_cast<DWORD>(len), &written, nullptr);
    CloseHandle(h);
    return true;
}

// Written next to this DLL - the folder the user deploys into. The IDE's
// gm_ttt_* test folder is deleted afterwards and a protected exe folder may
// be unwritable, so those cases fall back to %TEMP%.
static void Log(const char* msg) {
    char dllPath[MAX_PATH] = "?";
    HMODULE self = nullptr;
    UINT dllLen = 0;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&Log), &self) &&
        self) {
        dllLen = GetModuleFileNameA(self, dllPath, MAX_PATH);
    }
    if (dllLen == 0 || dllLen >= MAX_PATH) {
        std::strcpy(dllPath, "?");
        dllLen = 1;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[1024];
    int len = std::snprintf(line, sizeof line,
                            "[%04u-%02u-%02u %02u:%02u:%02u] %s\r\n",
                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, msg);
    if (len <= 0)
        return;

    bool ideTempRun = std::strstr(dllPath, "gm_ttt_") != nullptr;
    if (!ideTempRun) {
        char path[MAX_PATH];
        std::snprintf(path, MAX_PATH, "%s", dllPath);
        char* slash = std::strrchr(path, '\\');
        if (slash) {
            std::strcpy(slash + 1, "framesort.log");
            if (appendLine(path, line, len))
                return;
        }
    }

    char temp[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp)) {
        std::strncat(temp, "framesort-gm80.log", MAX_PATH - std::strlen(temp) - 1);
        appendLine(temp, line, len);
    }
}

static void LogSiteMismatch(const char* site, std::uint32_t rva,
                            const unsigned char* expect, std::size_t len) {
    char text[768];
    int n = std::snprintf(text, sizeof text,
                          "framesort: signature mismatch at %s (rva 0x%X), feature disabled.",
                          site, rva);
    if (n > 0 && n < static_cast<int>(sizeof text)) {
        n += std::snprintf(text + n, sizeof text - n, " found:");
        for (std::size_t i = 0; i < len && n < static_cast<int>(sizeof text); ++i)
            n += std::snprintf(text + n, sizeof text - n, " %02X", base()[rva + i]);
        n += std::snprintf(text + n, sizeof text - n, " expected:");
        for (std::size_t i = 0; i < len && n < static_cast<int>(sizeof text); ++i)
            n += std::snprintf(text + n, sizeof text - n, " %02X", expect[i]);
        Log(text);
    }
}
#else
static void Log(const char*) {}
static void LogSiteMismatch(const char*, std::uint32_t, const unsigned char*, std::size_t) {}
#endif

} // namespace framesort
