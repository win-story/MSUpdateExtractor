#pragma once

#include <filesystem>
#include <functional>

namespace msupdate {
namespace fs = std::filesystem;

using ProgressCallback = std::function<void(int current, int total)>;

class ArchiveExtractor {
public:
    virtual ~ArchiveExtractor() = default;
    virtual int CountFiles(const fs::path& archivePath) = 0;
    virtual bool Extract(const fs::path& archivePath, const fs::path& outputDirectory, ProgressCallback progress) = 0;
};

} // namespace msupdate
