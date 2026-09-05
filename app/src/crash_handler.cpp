#include "crash_handler.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dbghelp.h>

namespace app {

namespace {

// Symbolized walk of `context`'s stack to stderr. Shared by every hook below -- the SEH filter
// walks the faulting context; the CRT/terminate hooks walk their own current context (the crash
// site is then a few frames below the hook itself, which is fine for a readable report).
//
// By-REFERENCE parameter + aligned local copy on purpose: CONTEXT is declspec(align(16)), and a
// by-value parameter is not guaranteed that alignment on x64 MSVC -- passing it by value made
// StackWalk64 unwind garbage (raw stack addresses instead of frames), found by task 20's
// deliberate crash test, not by review.
void walk_and_print(const CONTEXT& contextIn) {
    CONTEXT context = contextIn; // local declaration carries CONTEXT's required 16-byte alignment
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (SymInitialize(process, nullptr, TRUE) == FALSE) {
        // ERROR_INVALID_PARAMETER (87) here means the process symbol handler is ALREADY
        // initialized -- Tracy's client initializes dbghelp for its own callstack capture when
        // linked in. Its init doesn't necessarily load the module list, so refresh it; the walk
        // below then symbolizes against whoever's session is active. Found by task 20's
        // deliberate crash test: without this, every frame printed as a raw address.
        SymRefreshModuleList(process);
    }

    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    for (int depth = 0; depth < 48; ++depth) {
        if (StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context, nullptr,
                        SymFunctionTableAccess64, SymGetModuleBase64, nullptr) == FALSE ||
            frame.AddrPC.Offset == 0) {
            break;
        }

        alignas(SYMBOL_INFO) char symbolStorage[sizeof(SYMBOL_INFO) + 256] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolStorage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;

        char moduleName[MAX_PATH] = "?";
        if (const DWORD64 moduleBase = SymGetModuleBase64(process, frame.AddrPC.Offset); moduleBase != 0) {
            GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleBase), moduleName, MAX_PATH);
        }

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        const bool haveLine =
            SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line) != FALSE;

        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) != FALSE) {
            std::fprintf(stderr, "  #%02d %s + 0x%llx [%s]%s%s:%lu\n", depth, symbol->Name,
                         static_cast<unsigned long long>(displacement), moduleName, haveLine ? " " : "",
                         haveLine ? line.FileName : "", haveLine ? line.LineNumber : 0);
        } else {
            std::fprintf(stderr, "  #%02d 0x%llx [%s]\n", depth,
                         static_cast<unsigned long long>(frame.AddrPC.Offset), moduleName);
        }
    }
    std::fflush(stderr);
}

void print_current_stack(const char* reason) {
    std::fprintf(stderr, "\n=== FATAL: %s (thread %lu) ===\n", reason, GetCurrentThreadId());
    CONTEXT context{};
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);
    walk_and_print(context);
}

LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* info) {
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    std::fprintf(stderr, "\n=== FATAL: unhandled exception 0x%08lX at %p (thread %lu) ===\n", code,
                 info->ExceptionRecord->ExceptionAddress, GetCurrentThreadId());
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        std::fprintf(stderr, "access violation: %s address %p\n",
                     info->ExceptionRecord->ExceptionInformation[0] == 0   ? "reading"
                     : info->ExceptionRecord->ExceptionInformation[0] == 1 ? "writing"
                                                                           : "executing",
                     reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    }
    walk_and_print(*info->ContextRecord);
    return EXCEPTION_EXECUTE_HANDLER; // die, but with the report above in hand
}

// Group J task 20: the failure classes the plain SEH filter never saw -- identified by Subagent
// 2's source audit of backward-cpp's Windows hook set (SIGABRT, std::terminate, pure virtual
// call, CRT invalid parameter), re-implemented on the existing reporting path instead of
// adopting the library (research/engine-hardening-log.md, task 19's written decision).
[[noreturn]] void terminate_hook() {
    print_current_stack("std::terminate (unhandled exception or noexcept violation)");
    _exit(3);
}

extern "C" void abort_signal_hook(int) {
    print_current_stack("abort()");
    _exit(3);
}

[[noreturn]] void purecall_hook() {
    print_current_stack("pure virtual call");
    _exit(3);
}

void invalid_parameter_hook(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    print_current_stack("CRT invalid parameter");
    _exit(3);
}

} // namespace

void install_crash_handler() {
    SetUnhandledExceptionFilter(&unhandled_filter);
    std::set_terminate(&terminate_hook);
    std::signal(SIGABRT, &abort_signal_hook);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT); // our report replaces the CRT popup
    _set_purecall_handler(&purecall_hook);
    _set_invalid_parameter_handler(&invalid_parameter_hook);
}

} // namespace app
