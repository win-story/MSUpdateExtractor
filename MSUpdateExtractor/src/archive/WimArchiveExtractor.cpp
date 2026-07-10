#include "WimArchiveExtractor.h"

#include "../common/Console.h"
#include "../common/FileUtil.h"

namespace msupdate {
namespace {

constexpr DWORD WIM_GENERIC_READ = 0x80000000L;
constexpr DWORD WIM_OPEN_EXISTING = 3;
constexpr DWORD WIM_FLAG_SHARE_WRITE = 0x00000040;
constexpr DWORD WIM_MSG_PROGRESS = 0x9478;
constexpr DWORD WIM_ALLOW_LZMS = 0x20000000;

struct WimProgressContext {
    ProgressCallback callback;
};

} // namespace

WimArchiveExtractor::WimArchiveExtractor() = default;

WimArchiveExtractor::~WimArchiveExtractor() {
    if (module_) {
        FreeLibrary(module_);
        module_ = nullptr;
    }
}

bool WimArchiveExtractor::EnsureLoaded() {
    if (module_) {
        return true;
    }
    module_ = LoadLibraryW(L"wimgapi.dll");
    if (!module_) {
        Console::Err(L"Failed to load wimgapi.dll. Error: %lu\n", GetLastError());
        return false;
    }
    return ResolveFunctions();
}

bool WimArchiveExtractor::ResolveFunctions() {
    WIMCreateFile_ = reinterpret_cast<WIMCreateFileFn>(GetProcAddress(module_, "WIMCreateFile"));
    WIMCloseHandle_ = reinterpret_cast<WIMCloseHandleFn>(GetProcAddress(module_, "WIMCloseHandle"));
    WIMLoadImage_ = reinterpret_cast<WIMLoadImageFn>(GetProcAddress(module_, "WIMLoadImage"));
    WIMApplyImage_ = reinterpret_cast<WIMApplyImageFn>(GetProcAddress(module_, "WIMApplyImage"));
    WIMGetImageCount_ = reinterpret_cast<WIMGetImageCountFn>(GetProcAddress(module_, "WIMGetImageCount"));
    WIMSetTemporaryPath_ = reinterpret_cast<WIMSetTemporaryPathFn>(GetProcAddress(module_, "WIMSetTemporaryPath"));
    WIMRegisterMessageCallback_ = reinterpret_cast<WIMRegisterMessageCallbackFn>(GetProcAddress(module_, "WIMRegisterMessageCallback"));
    WIMUnregisterMessageCallback_ = reinterpret_cast<WIMUnregisterMessageCallbackFn>(GetProcAddress(module_, "WIMUnregisterMessageCallback"));

    if (!WIMCreateFile_ || !WIMCloseHandle_ || !WIMLoadImage_ || !WIMApplyImage_) {
        Console::Err(L"wimgapi.dll does not expose the required WIM functions.\n");
        return false;
    }
    return true;
}

DWORD CALLBACK WimArchiveExtractor::ProgressThunk(DWORD messageId, WPARAM wParam, LPARAM, PVOID userData) {
    auto* context = reinterpret_cast<WimProgressContext*>(userData);
    if (context && context->callback && messageId == WIM_MSG_PROGRESS) {
        context->callback(static_cast<int>(wParam), 100);
    }
    return TRUE;
}

int WimArchiveExtractor::CountFiles(const fs::path&) {
    return 1;
}

bool WimArchiveExtractor::Extract(const fs::path& archivePath, const fs::path& outputDirectory, ProgressCallback progress) {
    if (!EnsureLoaded()) {
        return false;
    }

    EnsureDirectory(outputDirectory);

    DWORD creationResult = 0;
    HANDLE wimHandle = WIMCreateFile_(archivePath.c_str(), WIM_GENERIC_READ, WIM_OPEN_EXISTING, WIM_FLAG_SHARE_WRITE | WIM_ALLOW_LZMS, 0, &creationResult);
    if (!wimHandle) {
        Console::Err(L"Failed to open WIM file %s. Error: %lu\n", archivePath.c_str(), GetLastError());
        return false;
    }

    WCHAR tempPath[MAX_PATH] = {};
    if (WIMSetTemporaryPath_ && GetTempPathW(MAX_PATH, tempPath) > 0) {
        WIMSetTemporaryPath_(wimHandle, tempPath);
    }

    WimProgressContext progressContext{ progress };
    bool callbackRegistered = false;
    if (WIMRegisterMessageCallback_ && progress) {
        DWORD handle = WIMRegisterMessageCallback_(wimHandle, reinterpret_cast<FARPROC>(&WimArchiveExtractor::ProgressThunk), &progressContext);
        callbackRegistered = (handle != 0xFFFFFFFF);
    }

    HANDLE imageHandle = WIMLoadImage_(wimHandle, 1);
    if (!imageHandle) {
        Console::Err(L"Failed to load WIM image 1 from %s. Error: %lu\n", archivePath.c_str(), GetLastError());
        if (callbackRegistered && WIMUnregisterMessageCallback_) {
            WIMUnregisterMessageCallback_(wimHandle, reinterpret_cast<FARPROC>(&WimArchiveExtractor::ProgressThunk));
        }
        WIMCloseHandle_(wimHandle);
        return false;
    }

    BOOL ok = WIMApplyImage_(imageHandle, outputDirectory.c_str(), 0);

    WIMCloseHandle_(imageHandle);
    if (callbackRegistered && WIMUnregisterMessageCallback_) {
        WIMUnregisterMessageCallback_(wimHandle, reinterpret_cast<FARPROC>(&WimArchiveExtractor::ProgressThunk));
    }
    WIMCloseHandle_(wimHandle);

    if (!ok) {
        Console::Err(L"Failed to extract WIM file %s. Error: %lu\n", archivePath.c_str(), GetLastError());
        return false;
    }

    return true;
}

} // namespace msupdate
