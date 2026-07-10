#pragma once

#include "../archive/CabArchiveExtractor.h"
#include "../archive/WimArchiveExtractor.h"
#include "../psf/PsfExtractor.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace msupdate {
namespace fs = std::filesystem;

struct ExtractionRequest {
    fs::path inputPath;
    fs::path outputDirectory;
};

class PackageExtractor {
public:
    virtual ~PackageExtractor() = default;
    virtual const wchar_t* Name() const = 0;
    virtual bool CanHandle(const fs::path& inputPath) const = 0;
    virtual bool Extract(const ExtractionRequest& request) = 0;
};

class PsmPsfPackageExtractor final : public PackageExtractor {
public:
    const wchar_t* Name() const override;
    bool CanHandle(const fs::path& inputPath) const override;
    bool Extract(const ExtractionRequest& request) override;
};

class CabPsfPackageExtractor final : public PackageExtractor {
public:
    explicit CabPsfPackageExtractor(CabArchiveExtractor& cabExtractor);
    const wchar_t* Name() const override;
    bool CanHandle(const fs::path& inputPath) const override;
    bool Extract(const ExtractionRequest& request) override;
private:
    CabArchiveExtractor& cabExtractor_;
};

class WimPsfPackageExtractor final : public PackageExtractor {
public:
    explicit WimPsfPackageExtractor(WimArchiveExtractor& wimExtractor);
    const wchar_t* Name() const override;
    bool CanHandle(const fs::path& inputPath) const override;
    bool Extract(const ExtractionRequest& request) override;
private:
    WimArchiveExtractor& wimExtractor_;
};

class SingleCabPackageExtractor final : public PackageExtractor {
public:
    explicit SingleCabPackageExtractor(CabArchiveExtractor& cabExtractor);
    const wchar_t* Name() const override;
    bool CanHandle(const fs::path& inputPath) const override;
    bool Extract(const ExtractionRequest& request) override;
private:
    bool ProcessCabinetList(const fs::path& outputDirectory, const fs::path& listFile);
    bool ProcessChildCabinet(const fs::path& outputDirectory, const fs::path& childCabinet, size_t index);
    CabArchiveExtractor& cabExtractor_;
};

class ExtractorRunner {
public:
    ExtractorRunner();
    bool Run(const fs::path& inputPath);
private:
    CabArchiveExtractor cabExtractor_;
    WimArchiveExtractor wimExtractor_;
    std::vector<std::unique_ptr<PackageExtractor>> extractors_;
};

} // namespace msupdate
