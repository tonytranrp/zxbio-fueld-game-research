#include "crash_handler.hpp"

#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dbghelp.h>

namespace app {

namespace {

LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* info) {
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    std::fprintf(stderr, "\n=== FATAL: unhandled exception 0x%08lX at %p (thread %lu) ===\n", code,
                 info->ExceptionRecord->ExceptionAddress, GetCurrentThreadId());
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        std::fprintf(stderr, "access violation: %s address %p\n",
                     info->ExceptionRecord->ExceptionInformation[0] == 0 ? "reading"
                     : info->ExceptionRecord->ExceptionInformation[0] == 1 ? "writing"
                                                                          : "executing",
                     reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    }

    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    // Walk the faulting thread's stack from the exception context.
    CONTEXT context = *info->ContextRecord;
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
        const bool haveLine = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line) != FALSE;

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
    return EXCEPTION_EXECUTE_HANDLER; // die, but with the report above in hand
}

} // namespace

void install_crash_handler() {
    SetUnhandledExceptionFilter(&unhandled_filter);
}

} // namespace app
