#include "MspackCabDecompressor.h"

#include "../../common/Console.h"
#include "../../common/FileUtil.h"
#include "../../../cab/cab.h"
#include "../../../cab/mspack.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace msupdate {
namespace {

DWORD AccessForMode(int mode) {
    switch (mode) {
    case MSPACK_SYS_OPEN_READ:
        return GENERIC_READ;
    case MSPACK_SYS_OPEN_WRITE:
        return GENERIC_WRITE;
    case MSPACK_SYS_OPEN_UPDATE:
        return GENERIC_READ | GENERIC_WRITE;
    case MSPACK_SYS_OPEN_APPEND:
        return FILE_APPEND_DATA | GENERIC_READ;
    default:
        return 0;
    }
}

DWORD CreationForMode(int mode) {
    switch (mode) {
    case MSPACK_SYS_OPEN_READ:
    case MSPACK_SYS_OPEN_UPDATE:
        return OPEN_EXISTING;
    case MSPACK_SYS_OPEN_WRITE:
        return CREATE_ALWAYS;
    case MSPACK_SYS_OPEN_APPEND:
        return OPEN_ALWAYS;
    default:
        return 0;
    }
}

} // namespace

class MspackCabDecompressor::WideFile final {
public:
    WideFile(HANDLE fileHandle, std::wstring filePath)
        : handle_(fileHandle), path_(std::move(filePath)) {}

    ~WideFile() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    WideFile(const WideFile&) = delete;
    WideFile& operator=(const WideFile&) = delete;

    static WideFile* From(mspack_file* file) {
        return reinterpret_cast<WideFile*>(file);
    }

    mspack_file* AsMspackFile() {
        return reinterpret_cast<mspack_file*>(this);
    }

    HANDLE Handle() const {
        return handle_;
    }

    const std::wstring& Path() const {
        return path_;
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::wstring path_;
};

class MspackCabDecompressor::ScopedDecompressor final {
public:
    explicit ScopedDecompressor(mspack_system* system)
        : decompressor_(mspack_create_cab_decompressor(system)) {}

    ~ScopedDecompressor() {
        if (decompressor_) {
            mspack_destroy_cab_decompressor(decompressor_);
        }
    }

    ScopedDecompressor(const ScopedDecompressor&) = delete;
    ScopedDecompressor& operator=(const ScopedDecompressor&) = delete;

    mscab_decompressor* Get() const {
        return decompressor_;
    }

private:
    mscab_decompressor* decompressor_ = nullptr;
};

MspackCabDecompressor::MspackCabDecompressor() = default;
MspackCabDecompressor::~MspackCabDecompressor() = default;

const char* MspackCabDecompressor::ErrorText(mscab_decompressor* decompressor) {
    switch (decompressor ? decompressor->last_error(decompressor) : MSPACK_ERR_ARGS) {
    case MSPACK_ERR_OK: return "success";
    case MSPACK_ERR_ARGS: return "invalid argument";
    case MSPACK_ERR_OPEN: return "file open error";
    case MSPACK_ERR_READ: return "file read error";
    case MSPACK_ERR_WRITE: return "file write error";
    case MSPACK_ERR_SEEK: return "file seek error";
    case MSPACK_ERR_NOMEMORY: return "out of memory";
    case MSPACK_ERR_SIGNATURE: return "bad CAB signature";
    case MSPACK_ERR_DATAFORMAT: return "invalid CAB data";
    case MSPACK_ERR_CHECKSUM: return "checksum error";
    case MSPACK_ERR_DECRUNCH: return "decompression error";
    default: return "unknown CAB error";
    }
}

std::wstring MspackCabDecompressor::DecodeSystemPath(const char* fileName) {
    return MultiByteToWideBestEffort(fileName, CP_UTF8);
}

mspack_file* MspackCabDecompressor::Open(mspack_system*, const char* fileName, int mode) {
    const DWORD access = AccessForMode(mode);
    const DWORD creation = CreationForMode(mode);
    if (access == 0 || creation == 0) {
        return nullptr;
    }

    std::wstring wideName = DecodeSystemPath(fileName);
    if (wideName.empty()) {
        return nullptr;
    }

    HANDLE handle = CreateFileW(
        ToExtendedPath(fs::path(wideName)).c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        creation,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    if (mode == MSPACK_SYS_OPEN_APPEND) {
        SetFilePointer(handle, 0, nullptr, FILE_END);
    }

    auto file = std::make_unique<WideFile>(handle, std::move(wideName));
    return file.release()->AsMspackFile();
}

void MspackCabDecompressor::Close(mspack_file* file) {
    delete WideFile::From(file);
}

int MspackCabDecompressor::Read(mspack_file* file, void* buffer, int bytes) {
    WideFile* wideFile = WideFile::From(file);
    if (!wideFile || !buffer || bytes < 0) {
        return -1;
    }

    DWORD read = 0;
    if (!ReadFile(wideFile->Handle(), buffer, static_cast<DWORD>(bytes), &read, nullptr)) {
        return -1;
    }
    return static_cast<int>(read);
}

int MspackCabDecompressor::Write(mspack_file* file, void* buffer, int bytes) {
    WideFile* wideFile = WideFile::From(file);
    if (!wideFile || !buffer || bytes < 0) {
        return -1;
    }

    DWORD written = 0;
    if (!WriteFile(wideFile->Handle(), buffer, static_cast<DWORD>(bytes), &written, nullptr)) {
        return -1;
    }
    return static_cast<int>(written);
}

int MspackCabDecompressor::Seek(mspack_file* file, off_t offset, int mode) {
    WideFile* wideFile = WideFile::From(file);
    if (!wideFile) {
        return -1;
    }

    DWORD moveMethod = FILE_BEGIN;
    switch (mode) {
    case MSPACK_SYS_SEEK_START:
        moveMethod = FILE_BEGIN;
        break;
    case MSPACK_SYS_SEEK_CUR:
        moveMethod = FILE_CURRENT;
        break;
    case MSPACK_SYS_SEEK_END:
        moveMethod = FILE_END;
        break;
    default:
        return -1;
    }

    LARGE_INTEGER distance{};
    distance.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(wideFile->Handle(), distance, nullptr, moveMethod) ? 0 : -1;
}

off_t MspackCabDecompressor::Tell(mspack_file* file) {
    WideFile* wideFile = WideFile::From(file);
    if (!wideFile) {
        return 0;
    }

    LARGE_INTEGER zero{};
    LARGE_INTEGER current{};
    if (!SetFilePointerEx(wideFile->Handle(), zero, &current, FILE_CURRENT)) {
        return 0;
    }
    return static_cast<off_t>(current.QuadPart);
}

void MspackCabDecompressor::Message(mspack_file* file, const char* format, ...) {
    std::wstring fileName;
    if (file) {
        fileName = WideFile::From(file)->Path();
    }

    char message[1024] = {};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);

    Console::Err(L"%s: %s\n", fileName.c_str(), MultiByteToWideBestEffort(message).c_str());
}

void* MspackCabDecompressor::Alloc(mspack_system*, size_t bytes) {
    return std::malloc(bytes);
}

void MspackCabDecompressor::Free(void* buffer) {
    std::free(buffer);
}

void MspackCabDecompressor::Copy(void* source, void* destination, size_t bytes) {
    std::memcpy(destination, source, bytes);
}

mspack_system MspackCabDecompressor::BuildSystem() {
    mspack_system system{};
    system.open = &MspackCabDecompressor::Open;
    system.close = &MspackCabDecompressor::Close;
    system.read = &MspackCabDecompressor::Read;
    system.write = &MspackCabDecompressor::Write;
    system.seek = &MspackCabDecompressor::Seek;
    system.tell = &MspackCabDecompressor::Tell;
    system.message = &MspackCabDecompressor::Message;
    system.alloc = &MspackCabDecompressor::Alloc;
    system.free = &MspackCabDecompressor::Free;
    system.copy = &MspackCabDecompressor::Copy;
    return system;
}

std::unique_ptr<char[]> MspackCabDecompressor::MakeOwnedUtf8(const fs::path& path) {
    std::string utf8 = WideToUtf8(path.wstring());
    auto owned = std::make_unique<char[]>(utf8.size() + 1);
    std::memcpy(owned.get(), utf8.c_str(), utf8.size() + 1);
    return owned;
}

bool MspackCabDecompressor::AttachSpanningCabinets(mscab_decompressor* decompressor,
                                                   mscabd_cabinet* cabinet,
                                                   const fs::path& archivePath,
                                                   std::vector<std::unique_ptr<char[]>>& ownedNames) const {
    const fs::path directory = archivePath.parent_path();

    for (mscabd_cabinet* current = cabinet; current && (current->flags & MSCAB_HDR_PREVCAB); current = current->prevcab) {
        fs::path previous = FindSiblingFileCaseInsensitive(directory, MultiByteToWideBestEffort(current->prevname));
        auto previousName = MakeOwnedUtf8(previous);
        mscabd_cabinet* loaded = decompressor->open(decompressor, previousName.get());
        if (!loaded || decompressor->prepend(decompressor, current, loaded)) {
            Console::Err(L"Failed to attach previous cabinet %s: %S\n", previous.c_str(), ErrorText(decompressor));
            if (loaded) {
                decompressor->close(decompressor, loaded);
            }
            return false;
        }
        ownedNames.push_back(std::move(previousName));
    }

    for (mscabd_cabinet* current = cabinet; current && (current->flags & MSCAB_HDR_NEXTCAB); current = current->nextcab) {
        fs::path next = FindSiblingFileCaseInsensitive(directory, MultiByteToWideBestEffort(current->nextname));
        auto nextName = MakeOwnedUtf8(next);
        mscabd_cabinet* loaded = decompressor->open(decompressor, nextName.get());
        if (!loaded || decompressor->append(decompressor, current, loaded)) {
            Console::Err(L"Failed to attach next cabinet %s: %S\n", next.c_str(), ErrorText(decompressor));
            if (loaded) {
                decompressor->close(decompressor, loaded);
            }
            return false;
        }
        ownedNames.push_back(std::move(nextName));
    }

    return true;
}

int MspackCabDecompressor::CountFiles(const fs::path& archivePath) {
    mspack_system system = BuildSystem();
    ScopedDecompressor scoped(&system);
    mscab_decompressor* decompressor = scoped.Get();
    if (!decompressor) {
        return 0;
    }

    std::string archiveName = WideToUtf8(archivePath.wstring());
    mscabd_cabinet* baseCabinet = decompressor->search(decompressor, archiveName.c_str());
    if (!baseCabinet) {
        return 0;
    }

    int total = 0;
    std::vector<std::unique_ptr<char[]>> ownedNames;
    for (mscabd_cabinet* cabinet = baseCabinet; cabinet; cabinet = cabinet->next) {
        AttachSpanningCabinets(decompressor, cabinet, archivePath, ownedNames);
        for (mscabd_file* file = cabinet->files; file; file = file->next) {
            ++total;
        }
    }

    decompressor->close(decompressor, baseCabinet);
    return total;
}

bool MspackCabDecompressor::Extract(const fs::path& archivePath, const fs::path& outputDirectory, ProgressCallback progress) {
    EnsureDirectory(outputDirectory);

    mspack_system system = BuildSystem();
    ScopedDecompressor scoped(&system);
    mscab_decompressor* decompressor = scoped.Get();
    if (!decompressor) {
        Console::Err(L"Failed to initialize the CAB decompressor.\n");
        return false;
    }

    std::string archiveName = WideToUtf8(archivePath.wstring());
    mscabd_cabinet* baseCabinet = decompressor->search(decompressor, archiveName.c_str());
    if (!baseCabinet) {
        Console::Err(L"No valid cabinet found in %s: %S\n", archivePath.c_str(), ErrorText(decompressor));
        return false;
    }

    const int totalFiles = CountFiles(archivePath);
    int currentFile = 0;
    std::vector<std::unique_ptr<char[]>> ownedNames;

    for (mscabd_cabinet* cabinet = baseCabinet; cabinet; cabinet = cabinet->next) {
        if (!AttachSpanningCabinets(decompressor, cabinet, archivePath, ownedNames)) {
            decompressor->close(decompressor, baseCabinet);
            return false;
        }

        for (mscabd_file* file = cabinet->files; file; file = file->next) {
            ++currentFile;
            if (progress) {
                progress(currentFile, totalFiles);
            }

            const bool utf8Name = (file->attribs & MSCAB_ATTRIB_UTF_NAME) != 0;
            fs::path outputFile = MakeSafeOutputPath(outputDirectory, CabinetNameToWide(file->filename, utf8Name));
            EnsureDirectory(outputFile.parent_path());

            std::string outputName = WideToUtf8(outputFile.wstring());
            if (decompressor->extract(decompressor, file, outputName.c_str())) {
                Console::Err(L"Failed to extract %s: %S\n", outputFile.c_str(), ErrorText(decompressor));
                decompressor->close(decompressor, baseCabinet);
                return false;
            }
        }
    }

    decompressor->close(decompressor, baseCabinet);
    return true;
}

} // namespace msupdate
