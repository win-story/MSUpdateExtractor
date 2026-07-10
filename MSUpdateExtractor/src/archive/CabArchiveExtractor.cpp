#include "CabArchiveExtractor.h"

#include "mspack/MspackCabDecompressor.h"

#include <utility>

namespace msupdate {

CabArchiveExtractor::CabArchiveExtractor()
    : decompressor_(std::make_unique<MspackCabDecompressor>()) {}

CabArchiveExtractor::~CabArchiveExtractor() = default;

int CabArchiveExtractor::CountFiles(const fs::path& archivePath) {
    return decompressor_->CountFiles(archivePath);
}

bool CabArchiveExtractor::Extract(const fs::path& archivePath, const fs::path& outputDirectory, ProgressCallback progress) {
    return decompressor_->Extract(archivePath, outputDirectory, std::move(progress));
}

} // namespace msupdate
