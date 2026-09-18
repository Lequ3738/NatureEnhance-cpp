#include "ErrorTraceback.h"
#include "RunnerHook.h"

#include <cstdio>
#include <cstring>

#include <windows.h>

// Error-report enhancement for the GM8.0 runner.
//
// The runner already knows where every GML call site is, but drops that
// information when assembling error text: script unwinding prepends only
// "In script <name>:", and builtin functions (ds_* etc.) call ShowError
// directly with no location at all. The dispatch path is detoured to keep a
// shadow stack of (codeObj, pos, funcId) for every executing GML call, which
// reconstructs the lost details:
//
//  1. CallOpDispatch detour - shadow stack; on a failed script call it
//     prepends "In script <name> (line N, position P):" (a 2-byte patch
//     suppresses the native location-less prepend).
//  2. ShowError detour - when a builtin/extension call fails, rebuilds the
//     native-style location block (source line + caret) into the runner's
//     error buffer and prepends the script chain. show_error() keeps its
//     chain but omits source line and caret.
//  3. ErrorDisplaySink detour - splices the event-level call site into the
//     "for object X:" header line at display time.

namespace errtrace {

using gm80hook::base;

// ---------------------------------------------------------------- shadow stack

struct Frame {
    std::uint32_t codeObj; // code object the call site lives in ([ctx+0xC])
    std::uint32_t pos;     // 1-based char position of the call site
    std::uint32_t funcId;  // id of what the frame calls ([callNode+4])
};

constexpr int kMaxFrames = 256;

static Frame g_frames[kMaxFrames];
static int g_depth = 0; // counts every active call frame, even past kMaxFrames
static bool g_installed = false;
static bool g_busy = false; // true while the ShowError hook composes its text

// ---------------------------------------------------------------- asm-visible symbols

extern "C" {
	void* g_fnLStrCatN = nullptr; // runner LStrCatN
	void* g_fnSourceDecompress = nullptr; // runner code-source decompressor
	void* g_callOpOrig = nullptr; // thunks back into the original functions
	void* g_showErrOrig = nullptr;
	void* g_sinkOrig = nullptr;
}

// Resolved once at install (impl address is static); the builtin table itself
// is read at error time since the runner may reseat it during init.
static std::uintptr_t g_fnShowErrorImpl = 0;

// ---------------------------------------------------------------- crafted strings

// Delphi literal-style AnsiString: refcount -1 is never freed, so the runner
// can copy these sources without ever deallocating them.
template <std::size_t N>
struct FakeStr {
    long ref = -1;
    long len = 0;
    char data[N] = {};
};

static FakeStr<64> g_linePiece;  // " (line N, position P)"
static FakeStr<16> g_undefName;  // "<undefined>"
static FakeStr<16384> g_block;   // full "Error in code ..." block
static FakeStr<32768> g_finalText; // full display text with the event header rewritten

// The non-script frame where the call chain left event code - spliced into
// the "for object X:" header at display time.
static Frame g_eventLoc = {0, 0, 0};
static bool g_pendingEventLoc = false;

// ---------------------------------------------------------------- runner helpers

static char** ErrBufVar() {
    // *RVA_ErrBufVarPtr is the address of the error-text AnsiString variable.
    auto slot = reinterpret_cast<char** volatile*>(base() + gm80hook::RVA_ErrBufVarPtr);
    return reinterpret_cast<char**>(*slot);
}

// LStrCatN(&destVar, count, sources): dest := sources[0] + ... + sources[count-1].
// The runner ABI takes the count in edx and the sources on the stack pushed
// in text order (first pushed ends deepest and is copied first).
// Callee-clean: GM80_LStrCatN pops the return address itself, skips the count
// stack args (lea esp,[esp+edx*4] before jmp eax), so the sources must NOT be
// cleaned here - doing both corrupts the stack.
__declspec(naked) static void __cdecl catCall(void* destVar, int count, void* const* sources) {
    __asm {
        push ebx
        push esi
        push edi
        mov  eax, [esp + 16]      ; destVar
        mov  esi, [esp + 20]      ; count
        mov  edi, [esp + 24]      ; sources
        xor  ecx, ecx
    pushLoop:
        cmp  ecx, esi
        jae  pushedAll
        push dword ptr [edi + ecx * 4]
        inc  ecx
        jmp  pushLoop
    pushedAll:
        mov  edx, esi
        call dword ptr [g_fnLStrCatN]
        pop  edi
        pop  esi
        pop  ebx
        ret
    }
}

// The stored source is a compressed stream; route it through the runner's own
// decompressor into a runner-managed AnsiString slot (cleared on each call).
static void* g_sourceVar = nullptr;

__declspec(naked) static void __cdecl decompressSource(const void* codeObj, void* outVar) {
    __asm {
        mov  eax, [esp + 4]       ; codeObj
        mov  edx, [esp + 8]       ; &AnsiString var
        call dword ptr [g_fnSourceDecompress]
        ret
    }
}

static bool sourceOf(std::uint32_t codeObj, const char** outSrc, long* outLen) {
    if (!codeObj || !g_fnSourceDecompress)
        return false;
    __try {
        decompressSource(reinterpret_cast<const void*>(codeObj), &g_sourceVar);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    const char* src = *reinterpret_cast<char* volatile*>(&g_sourceVar);
    if (!src)
        return false;
    long len = *reinterpret_cast<const long*>(src - 4);
    if (len < 0)
        return false;
    *outSrc = src;
    *outLen = len;
    return true;
}

struct LineCol {
    int line;
    int col;
};

// Mirrors the runner's counting: CR always breaks a line; LF breaks only when
// not preceded by CR. `col` equals the reported "at position" value.
static LineCol lineAt(const char* src, long len, std::uint32_t pos) {
    LineCol lc = {1, 0};
    if (!src || len <= 0)
        return lc;
    if (pos < 1)
        pos = 1;
    if (pos > static_cast<std::uint32_t>(len))
        pos = len;
    for (long i = 1; i <= static_cast<long>(pos); ++i) {
        unsigned char c = static_cast<unsigned char>(src[i - 1]);
        if (c == 0x0D || (c == 0x0A && (i == 1 || static_cast<unsigned char>(src[i - 2]) != 0x0D))) {
            ++lc.line;
            lc.col = 0;
        } else {
            ++lc.col;
        }
    }
    return lc;
}

static long copySourceLine(const char* src, long len, std::uint32_t pos, char* dst, std::size_t cap) {
    if (pos < 1)
        pos = 1;
    if (pos > static_cast<std::uint32_t>(len))
        pos = len;
    long b = static_cast<long>(pos) - 1;
    while (b > 0 && src[b - 1] != 0x0D && src[b - 1] != 0x0A)
        --b;
    long e = static_cast<long>(pos) - 1;
    while (e < len && src[e] != 0x0D && src[e] != 0x0A)
        ++e;
    long n = e - b;
    if (static_cast<std::size_t>(n) > cap - 1)
        n = static_cast<long>(cap) - 1;
    std::memcpy(dst, src + b, n);
    dst[n] = 0;
    return n;
}

static int scriptCount() {
    return *reinterpret_cast<int volatile*>(base() + gm80hook::RVA_ScriptCount);
}

static const char* scriptNameAt(int idx) {
    auto names = *reinterpret_cast<std::uintptr_t volatile*>(base() + gm80hook::RVA_ScriptNameArray);
    if (!names || idx < 0 || idx >= scriptCount())
        return nullptr;
    return *reinterpret_cast<const char* volatile*>(names + static_cast<std::uintptr_t>(idx) * 4);
}

// Finds the script whose stored code object equals `codeObj`; -1 when the code
// belongs to an event action, execute_string text, etc. [res+8] IS the code
// object itself (no extra indirection - verified via sub_528B3C).
static int scriptIndexOfCodeObj(std::uint32_t codeObj) {
    if (!codeObj)
        return -1;
    int count = scriptCount();
    if (count <= 0)
        return -1;
    auto resArr = *reinterpret_cast<std::uintptr_t volatile*>(base() + gm80hook::RVA_ScriptResArray);
    if (!resArr)
        return -1;
    for (int i = 0; i < count; ++i) {
        auto res = *reinterpret_cast<std::uint32_t volatile*>(resArr + static_cast<std::uintptr_t>(i) * 4);
        if (!res)
            continue;
        auto code = *reinterpret_cast<std::uint32_t volatile*>(
            static_cast<std::uintptr_t>(res) + gm80hook::OFF_ScriptResCode);
        if (code == codeObj)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------- hook 1: call-op shadow stack

static void PrependScriptFrame(unsigned scriptIdx, std::uint32_t codeObj, std::uint32_t callPos);

extern "C" void __cdecl errtraceCallOpEnter(const unsigned* saved) {
    // saved[0]=eax(ctx) saved[1]=edx(callNode) saved[2]=ecx(result)
    ++g_depth;
    if (g_depth > kMaxFrames)
        return;

    Frame f = {0, 0, 0};
    __try {
        std::uintptr_t ctx = saved[0];
        std::uintptr_t node = saved[1];
        if (ctx && node) {
            f.codeObj = *reinterpret_cast<std::uint32_t*>(ctx + gm80hook::OFF_CtxCodeObj);
            f.pos = *reinterpret_cast<std::uint32_t*>(node + gm80hook::OFF_NodePos);
            f.funcId = *reinterpret_cast<std::uint32_t*>(node + gm80hook::OFF_NodeFuncId);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        f = Frame{0, 0, 0};
    }
    g_frames[g_depth - 1] = f;
}

// The dispatch returns -1 when the call failed. For script calls
// (funcId 100000..499999) that is our cue to prepend "In script <name>
// (line N):" - the frame just popped carries the exact call site.
extern "C" void __cdecl errtraceCallOpLeave(unsigned retval) {
    if (g_depth <= 0)
        return;
    --g_depth;

    if (retval != 0xFFFFFFFFu)
        return;
    if (g_depth >= kMaxFrames)
        return; // the slot was not written for this call (overflow path)

    Frame f = g_frames[g_depth];
    if (f.funcId < gm80hook::FID_SCRIPT_BASE || f.funcId >= gm80hook::FID_EXT_BASE)
        return;

    __try {
        PrependScriptFrame(f.funcId - gm80hook::FID_SCRIPT_BASE, f.codeObj, f.pos);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

__declspec(naked) static void CallOpDetour() {
    __asm {
        push ecx
        push edx
        push eax
        mov  eax, esp
        push eax
        call errtraceCallOpEnter
        add  esp, 4
        mov  eax, [esp]         ; ctx
        mov  edx, [esp + 4]     ; callNode
        mov  ecx, [esp + 8]     ; result
        call dword ptr [g_callOpOrig]
        push eax                ; keep the return value
        push eax                ; arg for errtraceCallOpLeave
        call errtraceCallOpLeave
        add  esp, 4
        pop  eax
        add  esp, 12
        ret
    }
}

// ---------------------------------------------------------------- "In script" prepend

// Formats " (line N, position P)" for a call site into g_linePiece. The
// stored call position points one char into the call token, so the reported
// position is the token start (GM8 positions are 1-based).
static void FormatLoc(std::uint32_t codeObj, std::uint32_t callPos) {
    g_linePiece.data[0] = 0;
    g_linePiece.len = 0;
    const char* src = nullptr;
    long len = 0;
    if (callPos != 0 && codeObj != 0 && sourceOf(codeObj, &src, &len)) {
        LineCol lc = lineAt(src, len, callPos);
        if (lc.line >= 1) {
            int startPos = lc.col > 1 ? lc.col - 1 : lc.col;
            std::snprintf(g_linePiece.data, sizeof g_linePiece.data, " (line %d, position %d)",
                          lc.line, startPos);
            g_linePiece.len = static_cast<long>(std::strlen(g_linePiece.data));
        }
    }
}

static void PrependScriptFrame(unsigned scriptIdx, std::uint32_t codeObj, std::uint32_t callPos) {
    const char* name = scriptNameAt(static_cast<int>(scriptIdx));
    if (!name) {
        std::strcpy(g_undefName.data, "<undefined>");
        g_undefName.len = 12;
        name = g_undefName.data;
    }

    void* sources[6];
    int n = 0;
    sources[n++] = base() + gm80hook::RVA_LitInScript;
    sources[n++] = const_cast<char*>(name);

    // The call site position lives in the caller's code object.
    FormatLoc(codeObj, callPos);
    if (g_linePiece.data[0])
        sources[n++] = g_linePiece.data;

    sources[n++] = base() + gm80hook::RVA_LitColon;
    sources[n++] = base() + gm80hook::RVA_LitCR;
    sources[n++] = *ErrBufVar();

    catCall(ErrBufVar(), n, sources);
}

// ---------------------------------------------------------------- hook 2: builtin ShowError (location block)

// Rebuilds the native-style error block from the shadow stack. `top` is the
// frame of the failing builtin call; frames below it describe the script chain.
// For intentional errors (show_error) the source line and caret are omitted -
// the chain plus the message is all the user needs.
static const char* BuildBuiltinErrorBlock(const Frame& top, const char* msg, bool intentional) {
    const char* src = nullptr;
    long len = 0;
    if (!sourceOf(top.codeObj, &src, &len))
        return msg;

    // The compiler stores the call position one char INTO the call token
    // (empirical: pos = identifier start + 1); the caret column keeps the raw
    // value so the caret lands on the call token's first character.
    LineCol lc = lineAt(src, len, top.pos);
    unsigned caretCol = lc.col > 501 ? 501u : static_cast<unsigned>(lc.col);

    char lineBuf[512];
    long lineLen = copySourceLine(src, len, top.pos, lineBuf, sizeof lineBuf);
    if (lineLen > 500)
        lineLen = 500;

    char* p = g_block.data;
    // Leading blank line: separates the indicator lines (header/chain) from
    // the code block (or the message, for intentional errors).
    *p++ = '\r';
    if (!intentional) {
        std::memcpy(p, "   ", 3);
        p += 3;
        std::memcpy(p, lineBuf, lineLen);
        p += lineLen;
        *p++ = '\r';
        // Caret kept visually aligned with the call-token start (the raw position).
        for (int i = 0; i <= static_cast<int>(caretCol); ++i)
            *p++ = ' ';
        *p++ = '^';
        *p++ = '\r';
    }
    std::size_t rem = sizeof g_block.data - static_cast<std::size_t>(p - g_block.data) - 1;
    std::size_t msgLen = std::strlen(msg);
    if (msgLen > rem)
        msgLen = rem;
    std::memcpy(p, msg, msgLen);
    p += msgLen;
    *p = 0;
    g_block.len = static_cast<long>(p - g_block.data);

    char** buf = ErrBufVar();
    void* one[1] = {g_block.data};
    catCall(buf, 1, one);

    // Every chain level carries its call site; the innermost one locates the
    // failing call itself. The walk stops at the first non-script code (event
    // action, execute_string text...) - that frame is where the chain left
    // event code and gets spliced into the "for object X:" header at display
    // time.
    g_pendingEventLoc = false;
    for (int i = g_depth - 1; i >= 0; --i) {
        int idx = scriptIndexOfCodeObj(g_frames[i].codeObj);
        if (idx < 0) {
            g_eventLoc = g_frames[i];
            g_pendingEventLoc = true;
            break;
        }
        const char* name = scriptNameAt(idx);
        if (!name)
            break;

        void* parts[6];
        int n = 0;
        parts[n++] = base() + gm80hook::RVA_LitInScript;
        parts[n++] = const_cast<char*>(name);
        FormatLoc(g_frames[i].codeObj, g_frames[i].pos);
        if (g_linePiece.data[0])
            parts[n++] = g_linePiece.data;
        parts[n++] = base() + gm80hook::RVA_LitColon;
        parts[n++] = base() + gm80hook::RVA_LitCR;
        parts[n++] = *buf;
        catCall(buf, static_cast<int>(n), parts);
    }
    return *buf;
}

extern "C" const char* __cdecl errtraceShowErrorPre(const char* msg, unsigned fatal) {
    (void)fatal;
    if (!msg)
        return msg;
    if (!g_installed || g_busy)
        return msg;

    // A code error is already assembling its own text; leave it alone.
    if (*reinterpret_cast<volatile unsigned char*>(base() + gm80hook::RVA_ErrorInProgress) != 0)
        return msg;
    if (g_depth <= 0 || g_depth > kMaxFrames)
        return msg;

    const Frame& top = g_frames[g_depth - 1];
    bool builtinOrExt =
        top.funcId < gm80hook::FID_SCRIPT_BASE || top.funcId >= gm80hook::FID_EXT_BASE;
    if (!builtinOrExt || !top.codeObj)
        return msg;

    // show_error() is an intentional error: keep the call stack, but skip the
    // source line and caret - the message itself is the point. The table is
    // doubly indirect (0x58FBE8 -> array variable -> heap entries), exactly as
    // the dispatcher resolves it.
    bool intentional = false;
    if (top.funcId < gm80hook::FID_SCRIPT_BASE && g_fnShowErrorImpl) {
        auto tableVar = *reinterpret_cast<std::uintptr_t volatile*>(base() + gm80hook::RVA_BuiltinTable);
        if (tableVar) {
            auto arrBase = *reinterpret_cast<std::uintptr_t volatile*>(tableVar);
            if (arrBase) {
                auto impl = *reinterpret_cast<std::uintptr_t volatile*>(
                    arrBase + 80ULL * top.funcId + gm80hook::OFF_BuiltinFnPtr);
                if (impl == g_fnShowErrorImpl)
                    intentional = true;
            }
        }
    }

    const char* result = msg;
    g_busy = true;
    __try {
        result = BuildBuiltinErrorBlock(top, msg, intentional);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = msg;
    }
    g_busy = false;
    return result;
}

// ---------------------------------------------------------------- hook 4: display sink (header splice)

// Final funnel for every error text. When a builtin-path error is pending,
// splice " (line N, position P)" - the site where the chain left event code -
// into the "for object X:" header line.
extern "C" const char* __cdecl errtraceSinkPre(const char* text, unsigned fatal) {
    (void)fatal;
    if (!g_pendingEventLoc || !text)
        return text;
    g_pendingEventLoc = false;

    const char* anchor = std::strstr(text, "for object ");
    if (!anchor)
        return text;
    const char* colon = std::strchr(anchor, ':');
    if (!colon)
        return text;

    FormatLoc(g_eventLoc.codeObj, g_eventLoc.pos);
    if (!g_linePiece.data[0])
        return text;

    std::size_t preLen = static_cast<std::size_t>(colon - text);
    std::size_t locLen = static_cast<std::size_t>(g_linePiece.len);
    const char* after = colon + 1;
    std::size_t afterLen = std::strlen(after);
    // The native header emits ":\r\n\r\n"; with the chain following directly
    // underneath, collapse it to a single line break (the blank line then
    // sits between the indicator lines and the code block, where our block's
    // leading \r puts it).
    if (afterLen >= 4 && after[0] == '\r' && after[1] == '\n' && after[2] == '\r' && after[3] == '\n') {
        after += 2;
        afterLen -= 2;
    }
    if (preLen + locLen + 1 + afterLen + 1 > sizeof g_finalText.data)
        return text;

    std::memcpy(g_finalText.data, text, preLen);
    std::memcpy(g_finalText.data + preLen, g_linePiece.data, locLen);
    g_finalText.data[preLen + locLen] = ':';
    std::memcpy(g_finalText.data + preLen + locLen + 1, after, afterLen + 1);
    g_finalText.len = static_cast<long>(preLen + locLen + 1 + afterLen);
    return g_finalText.data;
}

__declspec(naked) static void SinkDetour() {
    // Same contract as ShowErrorDetour: text in eax, fatal flag in dl, and
    // the caller's ebx/esi/edi untouched.
    __asm {
        push ebp
        mov  ebp, esp
        push ecx                 ; [ebp-4]  original ecx
        push edx                 ; [ebp-8]  original edx (fatal flag in dl)
        push ebx                 ; [ebp-12] original ebx
        mov  ebx, eax            ; ebx = text
        push edx                 ; fatal
        push ebx                 ; text
        call errtraceSinkPre
        add  esp, 8              ; eax = text to display
        push eax                 ; [ebp-16] final text
        mov  eax, [ebp - 12]
        mov  ebx, eax            ; restore original ebx
        mov  edx, [ebp - 8]      ; restore original edx
        mov  ecx, [ebp - 4]      ; restore original ecx
        pop  eax                 ; eax = final text
        call dword ptr [g_sinkOrig]
        mov  esp, ebp
        pop  ebp
        ret
    }
}

__declspec(naked) static void ShowErrorDetour() {
    // The original ShowError preserves the caller's ebx/esi/edi per the Delphi
    // ABI (and consumes an incoming ebx register param), so all of ecx/edx/ebx
    // must reach both the pre-hook and the original exactly as the caller set
    // them; only eax carries the (possibly replaced) message.
    __asm {
        push ebp
        mov  ebp, esp
        push ecx                 ; [ebp-4]  original ecx
        push edx                 ; [ebp-8]  original edx (fatal flag in dl)
        push ebx                 ; [ebp-12] original ebx
        mov  ebx, eax            ; ebx = original message
        push edx                 ; fatal
        push ebx                 ; msg
        call errtraceShowErrorPre
        add  esp, 8              ; eax = message to display
        push eax                 ; [ebp-16] final message
        mov  eax, [ebp - 12]
        mov  ebx, eax            ; restore original ebx
        mov  edx, [ebp - 8]      ; restore original edx
        mov  ecx, [ebp - 4]      ; restore original ecx
        pop  eax                 ; eax = final message
        call dword ptr [g_showErrOrig]
        mov  esp, ebp
        pop  ebp
        ret
    }
}

// ---------------------------------------------------------------- installation

static gm80hook::InlineHook g_hookCallOp;
static gm80hook::InlineHook g_hookShowError;
static gm80hook::InlineHook g_hookSink;

// Install report, so a silent disable (foreign runner build) is diagnosable
// without a debugger. Written next to this DLL - that is the folder the user
// actually deploys into. A GM8 IDE test run loads the DLL from a gm_ttt_*
// temp folder that is deleted afterwards, and a protected exe folder may be
// unwritable, so those cases fall back to %TEMP%.
static bool AppendLine(const char* path, const char* line, int len) {
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    WriteFile(h, line, static_cast<DWORD>(len), &written, nullptr);
    CloseHandle(h);
    return true;
}

static void Log(const char* msg) {
    char exePath[MAX_PATH] = "?";
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    HMODULE self = nullptr;
    char dllPath[MAX_PATH] = "?";
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
                            "[%04u-%02u-%02u %02u:%02u:%02u] %s\r\n[dll] %s\r\n[exe] %s\r\n",
                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                            msg, dllPath, exePath);
    if (len <= 0)
        return;

    bool ideTempRun = std::strstr(dllPath, "gm_ttt_") != nullptr;
    if (!ideTempRun) {
        char path[MAX_PATH];
        std::snprintf(path, MAX_PATH, "%s", dllPath);
        char* slash = std::strrchr(path, '\\');
        if (slash) {
            std::strcpy(slash + 1, "errtrace.log");
            if (AppendLine(path, line, len))
                return;
        }
    }

    char temp[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp)) {
        std::strncat(temp, "errtrace-gm80.log", MAX_PATH - std::strlen(temp) - 1);
        AppendLine(temp, line, len);
    }
}

static void LogSiteMismatch(const char* site, std::uint32_t rva, const unsigned char* expect, std::size_t len) {
    char text[768];
    int n = std::snprintf(text, sizeof text,
                          "errtrace: signature mismatch at %s (rva 0x%X), feature disabled.", site, rva);
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

bool Install() {
    if (g_installed)
        return true;

    g_fnLStrCatN = base() + gm80hook::RVA_LStrCatN;

    // Line numbers need the decompressor; every feature path reads source text.
    if (std::memcmp(base() + gm80hook::RVA_SourceDecompress, gm80hook::SIG_SourceDecompress,
                    sizeof gm80hook::SIG_SourceDecompress) != 0) {
        LogSiteMismatch("SourceDecompress", gm80hook::RVA_SourceDecompress,
                        gm80hook::SIG_SourceDecompress, sizeof gm80hook::SIG_SourceDecompress);
        return false;
    }
    g_fnSourceDecompress = base() + gm80hook::RVA_SourceDecompress;

    // show_error() impl, for skipping the location block on intentional errors.
    if (std::memcmp(base() + gm80hook::RVA_ShowErrorImpl, gm80hook::SIG_ShowErrorImpl,
                    sizeof gm80hook::SIG_ShowErrorImpl) != 0) {
        LogSiteMismatch("ShowErrorImpl", gm80hook::RVA_ShowErrorImpl,
                        gm80hook::SIG_ShowErrorImpl, sizeof gm80hook::SIG_ShowErrorImpl);
    } else {
        g_fnShowErrorImpl = reinterpret_cast<std::uintptr_t>(base() + gm80hook::RVA_ShowErrorImpl);
    }

    if (!g_hookCallOp.install(gm80hook::RVA_CallOpDispatch, gm80hook::SIG_CallOpDispatch,
                              sizeof gm80hook::SIG_CallOpDispatch, CallOpDetour)) {
        LogSiteMismatch("CallOpDispatch", gm80hook::RVA_CallOpDispatch,
                        gm80hook::SIG_CallOpDispatch, sizeof gm80hook::SIG_CallOpDispatch);
        return false;
    }
    g_callOpOrig = g_hookCallOp.target();

    // Pairs with the dispatch-side "In script" chain: from this point the
    // native plain prepend is gone and the dispatch hook owns the text.
    if (!gm80hook::patchBytes(gm80hook::RVA_SuppressJnz, gm80hook::SIG_SuppressJnz,
                              gm80hook::PATCH_SuppressJmp, sizeof gm80hook::PATCH_SuppressJmp)) {
        g_hookCallOp.uninstall();
        LogSiteMismatch("SuppressJnz", gm80hook::RVA_SuppressJnz,
                        gm80hook::SIG_SuppressJnz, sizeof gm80hook::SIG_SuppressJnz);
        return false;
    }

    if (!g_hookShowError.install(gm80hook::RVA_ShowError, gm80hook::SIG_ShowError,
                                 sizeof gm80hook::SIG_ShowError, ShowErrorDetour)) {
        g_hookCallOp.uninstall();
        LogSiteMismatch("ShowError", gm80hook::RVA_ShowError,
                        gm80hook::SIG_ShowError, sizeof gm80hook::SIG_ShowError);
        return false;
    }
    g_showErrOrig = g_hookShowError.target();

    if (!g_hookSink.install(gm80hook::RVA_ErrorDisplaySink, gm80hook::SIG_ErrorDisplaySink,
                            sizeof gm80hook::SIG_ErrorDisplaySink, SinkDetour)) {
        g_hookShowError.uninstall();
        g_hookCallOp.uninstall();
        LogSiteMismatch("ErrorDisplaySink", gm80hook::RVA_ErrorDisplaySink,
                        gm80hook::SIG_ErrorDisplaySink, sizeof gm80hook::SIG_ErrorDisplaySink);
        return false;
    }
    g_sinkOrig = g_hookSink.target();

    g_installed = true;
    Log("errtrace: installed (GM8.0 error traceback active)");
    return true;
}

} // namespace errtrace
