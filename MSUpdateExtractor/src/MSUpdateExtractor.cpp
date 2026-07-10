#include "common/Console.h"
#include "packages/PackageExtractor.h"

#include <windows.h>
#include <DbgHelp.h>
#include <crtdbg.h>
#include <cstdlib>
#include <exception>
#include <filesystem>

#pragma comment(lib, "DbgHelp")

namespace fs = std::filesystem;

namespace {

void PrintUsage() {
    msupdate::Console::Out(
        L"MSUpdateExtractor\n"
        L"Usage:\n"
        L"  MSUpdateExtractor.exe <package.psm|package.cab|package.wim>\n\n"
        L"Supported layouts:\n"
        L"  * PSM + PSF\n"
        L"  * CAB + PSF\n"
        L"  * WIM + PSF\n"
        L"  * Single CAB, including cabinet.cablist.ini child cabinet sets\n");
}

LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    msupdate::Console::Err(L"Unhandled exception: 0x%08X\n", exceptionInfo->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

void InvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t) {
    msupdate::Console::Err(L"Invalid CRT parameter in %s at %s:%u. Expression: %s\n",
        function ? function : L"<unknown>",
        file ? file : L"<unknown>",
        line,
        expression ? expression : L"<unknown>");
    std::abort();
}

void ConfigureRuntimeHandlers() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_invalid_parameter_handler(InvalidParameterHandler);
    SetUnhandledExceptionFilter(UnhandledExceptionFilter);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    ConfigureRuntimeHandlers();

    if (argc < 2 || !argv[1] || wcslen(argv[1]) == 0) {
        PrintUsage();
        return 1;
    }

    try {
        msupdate::ExtractorRunner runner;
        return runner.Run(fs::path(argv[1])) ? 0 : 1;
    } catch (const std::exception& ex) {
        msupdate::Console::Err(L"Fatal error: %S\n", ex.what());
        return 1;
    } catch (...) {
        msupdate::Console::Err(L"Fatal error: unknown exception.\n");
        return 1;
    }
}
