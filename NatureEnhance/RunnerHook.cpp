#include "RunnerHook.h"

#include <cstring>

#include <windows.h>

namespace gm80hook {

std::uint8_t* base() {
    return reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
}

namespace {

// One small RWX pool shared by all thunk stubs; hooks are installed once at
// startup and never removed, so a bump allocator is enough.
std::uint8_t* g_pool = nullptr;
std::size_t g_poolUsed = 0;

std::uint8_t* allocThunkMem() {
    constexpr std::size_t kPoolSize = 4096;
    constexpr std::size_t kThunkSize = 32;

    if (!g_pool) {
        g_pool = reinterpret_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, kPoolSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!g_pool)
            return nullptr;
    }
    if (g_poolUsed + kThunkSize > kPoolSize)
        return nullptr;

    std::uint8_t* mem = g_pool + g_poolUsed;
    g_poolUsed += kThunkSize;
    return mem;
}

bool rel32Fits(std::intptr_t rel) {
    return rel >= INT32_MIN && rel <= INT32_MAX;
}

} // namespace

bool InlineHook::install(std::uint32_t rva, const unsigned char* expectSig, std::size_t sigLen, void* detourFn) {
    if (thunk_)
        return false;

    target_ = base() + rva;

    // Foreign runner version: leave the process completely untouched.
    if (std::memcmp(target_, expectSig, sigLen) != 0)
        return false;

    std::uint8_t* mem = allocThunkMem();
    if (!mem)
        return false;

    // Thunk: relocated prologue + jmp back to target+9. Both jumps are
    // range-checked before any write happens.
    std::memcpy(orig_, target_, kPrologueLen);
    std::memcpy(mem, orig_, kPrologueLen);
    std::uint8_t* backJmp = mem + kPrologueLen;
    std::intptr_t backRel = (target_ + kPrologueLen) - (backJmp + 5);
    std::intptr_t detourRel = reinterpret_cast<std::uint8_t*>(detourFn) - (target_ + 5);
    if (!rel32Fits(backRel) || !rel32Fits(detourRel))
        return false;

    backJmp[0] = 0xE9;
    std::memcpy(backJmp + 1, &backRel, 4);

    DWORD oldProtect = 0;
    if (!VirtualProtect(target_, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    target_[0] = 0xE9;
    std::memcpy(target_ + 1, &detourRel, 4);
    VirtualProtect(target_, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target_, 16);

    thunk_ = mem;
    return true;
}

void InlineHook::uninstall() {
    if (!thunk_)
        return;

    DWORD oldProtect = 0;
    if (VirtualProtect(target_, 16, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::memcpy(target_, orig_, kPrologueLen);
        VirtualProtect(target_, 16, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), target_, 16);
    }
    target_ = nullptr;
    thunk_ = nullptr;
}

bool patchBytes(std::uint32_t rva, const unsigned char* expect, const unsigned char* repl, std::size_t len) {
    std::uint8_t* at = base() + rva;

    if (std::memcmp(at, expect, len) != 0)
        return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(at, len, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    std::memcpy(at, repl, len);
    VirtualProtect(at, len, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), at, len);
    return true;
}

} // namespace gm80hook
