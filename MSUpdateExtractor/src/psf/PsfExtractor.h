#pragma once

#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>

namespace msupdate {
namespace fs = std::filesystem;

enum class DeltaSourceType {
    Raw,
    Pa19,
    Pa30
};

struct DeltaFile {
    std::wstring name;
    FILETIME time{};
    DeltaSourceType type = DeltaSourceType::Raw;
    ULARGE_INTEGER offset{};
    DWORD length = 0;
    fs::path basisPath;
    int id = 0;
    int basis = 0;
};

class PsfParser {
public:
    std::vector<DeltaFile> ParseLegacyPsm(const fs::path& descriptionPath) const;
    std::vector<DeltaFile> ParsePsfXml(const fs::path& descriptionPath) const;
    std::vector<DeltaFile> ParseNonPsfXml(const fs::path& descriptionPath, const fs::path& extractedRoot) const;
};

class DeltaApplier {
public:
    bool ApplyFromPayload(const std::vector<DeltaFile>& files, const fs::path& targetDirectory, const fs::path& payloadFile) const;
    bool ApplyFromExtractedFiles(const std::vector<DeltaFile>& files, const fs::path& targetDirectory) const;

private:
    bool ApplyOne(const DeltaFile& item, const std::vector<unsigned char>& deltaBuffer, const std::vector<unsigned char>& basisBuffer, const fs::path& outputPath) const;
    bool ReadPayloadSlice(HANDLE payloadHandle, const DeltaFile& item, std::vector<unsigned char>& buffer) const;
    fs::path OutputPathFor(const fs::path& targetDirectory, const std::wstring& fileName) const;
};

} // namespace msupdate
