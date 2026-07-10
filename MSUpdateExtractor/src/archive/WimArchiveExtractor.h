#pragma once

#include "ArchiveExtractor.h"
#include <windows.h>

namespace msupdate {

class WimArchiveExtractor final : public ArchiveExtractor {
public:
    WimArchiveExtractor();
    ~WimArchiveExtractor();

    int CountFiles(const fs::path& archivePath) override;
    bool Extract(const fs::path& archivePath, const fs::path& outputDirectory, ProgressCallback progress) override;

private:
    using WIMCreateFileFn = HANDLE(WINAPI*)(PCWSTR, DWORD, DWORD, DWORD, DWORD, PDWORD);
    using WIMCloseHandleFn = BOOL(WINAPI*)(HANDLE);
    using WIMLoadImageFn = HANDLE(WINAPI*)(HANDLE, DWORD);
    using WIMApplyImageFn = BOOL(WINAPI*)(HANDLE, PCWSTR, DWORD);
    using WIMGetImageCountFn = BOOL(WINAPI*)(HANDLE, PDWORD);
    using WIMSetTemporaryPathFn = BOOL(WINAPI*)(HANDLE, PCWSTR);
    using WIMRegisterMessageCallbackFn = DWORD(WINAPI*)(HANDLE, FARPROC, PVOID);
    using WIMUnregisterMessageCallbackFn = BOOL(WINAPI*)(HANDLE, FARPROC);

    bool EnsureLoaded();
    bool ResolveFunctions();

    static DWORD CALLBACK ProgressThunk(DWORD messageId, WPARAM wParam, LPARAM lParam, PVOID userData);

    HMODULE module_ = nullptr;
    WIMCreateFileFn WIMCreateFile_ = nullptr;
    WIMCloseHandleFn WIMCloseHandle_ = nullptr;
    WIMLoadImageFn WIMLoadImage_ = nullptr;
    WIMApplyImageFn WIMApplyImage_ = nullptr;
    WIMGetImageCountFn WIMGetImageCount_ = nullptr;
    WIMSetTemporaryPathFn WIMSetTemporaryPath_ = nullptr;
    WIMRegisterMessageCallbackFn WIMRegisterMessageCallback_ = nullptr;
    WIMUnregisterMessageCallbackFn WIMUnregisterMessageCallback_ = nullptr;
};

} // namespace msupdate
