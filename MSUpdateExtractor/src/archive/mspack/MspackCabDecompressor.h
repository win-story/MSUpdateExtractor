#pragma once

#include "../ArchiveExtractor.h"

#include <memory>
#include <sys/types.h>
#include <string>
#include <vector>

struct mscab_decompressor;
struct mscabd_cabinet;
struct mspack_file;
struct mspack_system;

namespace msupdate {

class MspackCabDecompressor final {
public:
    MspackCabDecompressor();
    ~MspackCabDecompressor();

    MspackCabDecompressor(const MspackCabDecompressor&) = delete;
    MspackCabDecompressor& operator=(const MspackCabDecompressor&) = delete;

    int CountFiles(const fs::path& archivePath);
    bool Extract(const fs::path& archivePath, const fs::path& outputDirectory, ProgressCallback progress);

private:
    class WideFile;
    class ScopedDecompressor;

    static const char* ErrorText(mscab_decompressor* decompressor);
    static mspack_file* Open(mspack_system* system, const char* fileName, int mode);
    static void Close(mspack_file* file);
    static int Read(mspack_file* file, void* buffer, int bytes);
    static int Write(mspack_file* file, void* buffer, int bytes);
    static int Seek(mspack_file* file, off_t offset, int mode);
    static off_t Tell(mspack_file* file);
    static void Message(mspack_file* file, const char* format, ...);
    static void* Alloc(mspack_system* system, size_t bytes);
    static void Free(void* buffer);
    static void Copy(void* source, void* destination, size_t bytes);

    static mspack_system BuildSystem();
    static std::wstring DecodeSystemPath(const char* fileName);
    static std::unique_ptr<char[]> MakeOwnedUtf8(const fs::path& path);

    bool AttachSpanningCabinets(mscab_decompressor* decompressor,
                                mscabd_cabinet* cabinet,
                                const fs::path& archivePath,
                                std::vector<std::unique_ptr<char[]>>& ownedNames) const;
};

} // namespace msupdate
