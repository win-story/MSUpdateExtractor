#pragma once

#include <windows.h>
#include <string>

namespace msupdate {

class Console {
public:
    static void Out(const wchar_t* format, ...);
    static void Err(const wchar_t* format, ...);
    static void StartProgress(const std::wstring& action = L"");
    static void UpdateProgress(int current, int total);
    static void FinishProgress();
};

} // namespace msupdate
