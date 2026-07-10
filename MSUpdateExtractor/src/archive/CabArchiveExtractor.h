#pragma once

#include "ArchiveExtractor.h"

#include <memory>

namespace msupdate {

class MspackCabDecompressor;

class CabArchiveExtractor final : public ArchiveExtractor {
public:
    CabArchiveExtractor();
    ~CabArchiveExtractor() override;

    CabArchiveExtractor(const CabArchiveExtractor&) = delete;
    CabArchiveExtractor& operator=(const CabArchiveExtractor&) = delete;

    int CountFiles(const fs::path& archivePath) override;
    bool Extract(const fs::path& archivePath, const fs::path& outputDirectory, ProgressCallback progress) override;

private:
    std::unique_ptr<MspackCabDecompressor> decompressor_;
};

} // namespace msupdate
