#pragma once

// GM8.0 runner inline-hook plumbing and the RVA address table.
// Single source of truth for every runner offset used by ErrorTraceback.
// All RVAs and signatures were verified against the GM8.0 runner
// (empty-project.exe, image base 0x400000); see the IDA database there.

#include <cstddef>
#include <cstdint>

namespace gm80hook {

// ---- Detour targets ----
constexpr std::uint32_t RVA_CallOpDispatch = 0x126FC0; // sub_526FC0 (eax=ctx, edx=callNode, ecx=result), plain ret
constexpr std::uint32_t RVA_ShowError      = 0x13F44C; // ShowError  (eax=msg AnsiString, dl=fatal), plain ret
constexpr std::uint32_t RVA_ErrorDisplaySink = 0x13F2AC; // GM80_ErrorDisplaySink (eax=text AnsiString, dl=fatal) - final display funnel for every error text

// ScriptCallWrapper (sub_5290FC, RVA 0x1290FC) is deliberately NOT hooked:
// a trampoline on it froze the game (cause unknown); the dispatch hook above
// provides everything its error path needs. Only its 2-byte suppress patch
// (RVA_SuppressJnz below) is applied.
constexpr std::uint32_t RVA_ScriptCallWrap = 0x1290FC;

// ---- Patch site: skip the native "In script <name>:" prepend on script failure ----
constexpr std::uint32_t RVA_SuppressJnz = 0x129248; // 75 3C (jnz) -> EB 3C (jmp)

// ---- Runner string helpers invoked through pointers ----
constexpr std::uint32_t RVA_LStrCatN = 0x005CB8; // eax=&destVar, edx=count, stack sources; push order == text order; callee-clean
constexpr std::uint32_t RVA_LStrClr  = 0x005934; // eax=&strVar

// Native implementation of show_error(str, fatal), identified via its
// SetFunction registration (edx operand at the "show_error" registration
// call). Used to skip the location block for intentional show_error calls:
// the runtime builtin table entry (+68) holds this pointer.
constexpr std::uint32_t RVA_ShowErrorImpl = 0x163B28;
inline constexpr unsigned char SIG_ShowErrorImpl[12] = {
    0x55, 0x8B, 0xEC, 0x8B, 0x55, 0x10, 0xDD, 0x42, 0x20, 0xD8, 0x1D, 0x4C };

// Decompresses a code object's stored source. The compiled exe does NOT keep
// source text plainly at [codeObj+0x0C] - it is a compressed stream, and the
// native error reporter decompresses it before counting lines. ABI:
// eax = code object, edx = &AnsiString-variable; the callee clears the
// variable first (safe to reuse) and returns with a plain ret.
constexpr std::uint32_t RVA_SourceDecompress = 0x143CB0; // sub_543CB0
inline constexpr unsigned char SIG_SourceDecompress[12] = {
    0x53, 0x56, 0x8B, 0xF2, 0x8B, 0xD8, 0x8B, 0xD6, 0x8B, 0x43, 0x0C, 0xE8 };

// ---- Runner string literals reused as concat sources ----
constexpr std::uint32_t RVA_LitInScript = 0x129300; // "In script "
constexpr std::uint32_t RVA_LitColon    = 0x129314; // ":"
constexpr std::uint32_t RVA_LitCR       = 0x129320; // "\r"

// ---- Runtime globals ----
constexpr std::uint32_t RVA_ScriptResArray  = 0x18F13C; // dword: script resource* array; [res+8]=code object
constexpr std::uint32_t RVA_ScriptNameArray = 0x18F140; // dword: AnsiString name array
constexpr std::uint32_t RVA_ScriptCount     = 0x18F144; // dword
constexpr std::uint32_t RVA_ErrorInProgress = 0x18F194; // byte: set while a code error text is being assembled
constexpr std::uint32_t RVA_ErrBufVarPtr    = 0x18FC98; // dword: address of the error-text AnsiString variable
constexpr std::uint32_t RVA_BuiltinTable    = 0x18FBE8; // DOUBLE indirection: dword here -> array variable -> heap entries (same as the dispatcher's resolution)

// ---- Builtin table entry ----
constexpr std::uint32_t OFF_BuiltinFnPtr    = 0x44; // [entry+0x44] = implementation function pointer

// ---- Struct offsets ----
constexpr std::uint32_t OFF_CtxCodeObj   = 0x0C; // [ctx+0x0C]    = code object of the executing code
constexpr std::uint32_t OFF_CodeSource   = 0x0C; // [codeObj+0x0C] = stored (compressed) source AnsiString
constexpr std::uint32_t OFF_NodeFuncId   = 0x04; // [callNode+0x04] = function id
constexpr std::uint32_t OFF_NodePos      = 0x30; // [callNode+0x30] = 1-based char position of the call site
constexpr std::uint32_t OFF_ScriptResCode = 0x08; // [scriptRes+0x08] = the script body's code object itself
                                                 // (verified via sub_528B3C: codeObj+4=type, +8=compiled, +0xC=source)

// ---- Function id ranges ----
constexpr std::uint32_t FID_SCRIPT_BASE = 100000; // 100000..499999: script, id-100000 = script index
constexpr std::uint32_t FID_EXT_BASE    = 500000; // >= 500000: extension function

// ---- Expected bytes at the detour/patch sites (GM8.0 runner) ----
inline constexpr unsigned char SIG_CallOpDispatch[16] = {
    0x55, 0x8B, 0xEC, 0x81, 0xC4, 0x58, 0xFE, 0xFF, 0xFF, 0x53, 0x56, 0x57, 0x33, 0xDB, 0x89, 0x9D };
inline constexpr unsigned char SIG_ScriptCallWrap[16] = {
    0x55, 0x8B, 0xEC, 0x81, 0xC4, 0x6C, 0xFE, 0xFF, 0xFF, 0x53, 0x56, 0x57, 0x33, 0xDB, 0x89, 0x9D };
inline constexpr unsigned char SIG_ShowError[16] = {
    0x55, 0x8B, 0xEC, 0x33, 0xC9, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x53, 0x8B, 0xDA };
inline constexpr unsigned char SIG_ErrorDisplaySink[16] = {
    0x55, 0x8B, 0xEC, 0x81, 0xC4, 0x2C, 0xFE, 0xFF, 0xFF, 0x53, 0x56, 0x57, 0x88, 0x55, 0xFB, 0x89 };
inline constexpr unsigned char SIG_SuppressJnz[2] = { 0x75, 0x3C };
inline constexpr unsigned char PATCH_SuppressJmp[2] = { 0xEB, 0x3C };

// Runtime base of the game executable (0x400000 unless relocated).
std::uint8_t* base();

// Inline hook: 5-byte rel32 jmp detour on the target plus a thunk that
// replays the relocated 9-byte prologue and jumps back to target+9.
class InlineHook {
public:
    // Verifies the expected signature before touching anything; on mismatch
    // (foreign runner version) it returns false and writes no memory.
    bool install(std::uint32_t rva, const unsigned char* expectSig, std::size_t sigLen, void* detourFn);
    void uninstall();
    bool installed() const { return thunk_ != nullptr; }

    // Address to call to execute the original function (original ABI).
    void* target() const { return thunk_; }

private:
    static constexpr std::size_t kPrologueLen = 9;

    std::uint8_t* target_ = nullptr;
    std::uint8_t* thunk_ = nullptr;
    unsigned char orig_[kPrologueLen] = {};
};

// Verifies then applies a small byte patch.
bool patchBytes(std::uint32_t rva, const unsigned char* expect, const unsigned char* repl, std::size_t len);

} // namespace gm80hook
