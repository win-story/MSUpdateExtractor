#include "Console.h"

#include <strsafe.h>
#include <cstdarg>
#include <vector>
#include <algorithm>

namespace msupdate {
namespace {

void WriteToHandle(HANDLE handle, const wchar_t* format, va_list args) {
    if (!handle || !format) {
        return;
    }

    std::vector<wchar_t> buffer(1024);
    while (true) {
        va_list retryArgs;
        va_copy(retryArgs, args);
        HRESULT hr = StringCchVPrintfW(buffer.data(), buffer.size(), format, retryArgs);
        va_end(retryArgs);
        if (SUCCEEDED(hr)) {
            DWORD written = 0;
            WriteConsoleW(handle, buffer.data(), static_cast<DWORD>(wcslen(buffer.data())), &written, nullptr);
            return;
        }
        if (hr != STRSAFE_E_INSUFFICIENT_BUFFER) {
            return;
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace

void Console::Out(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    WriteToHandle(GetStdHandle(STD_OUTPUT_HANDLE), format, args);
    va_end(args);
}

void Console::Err(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    WriteToHandle(GetStdHandle(STD_ERROR_HANDLE), format, args);
    va_end(args);
}

void Console::StartProgress(const std::wstring& action) {
    if (!action.empty()) {
        Out(L"%s\n", action.c_str());
    }
}

void Console::UpdateProgress(int current, int total) {
    if (total <= 0) {
        total = 1;
    }
    current = std::clamp(current, 0, total);
    const double ratio = static_cast<double>(current) / static_cast<double>(total);
    const int width = 58;
    const int filled = static_cast<int>(ratio * width);

    std::wstring bar(width, L' ');
    for (int i = 0; i < filled && i < width; ++i) {
        bar[i] = L'=';
    }

    Out(L"\r[%s] %5.1f%%", bar.c_str(), ratio * 100.0);
}

void Console::FinishProgress() {
    Out(L"\n");
}

} // namespace msupdate
