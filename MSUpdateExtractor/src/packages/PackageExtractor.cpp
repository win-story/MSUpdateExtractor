#include "PackageExtractor.h"

#include "../common/Console.h"
#include "../common/FileUtil.h"

#include <algorithm>
#include <chrono>

namespace msupdate {
namespace {

fs::path PsfFor(const fs::path& inputPath) {
    return ChangeExtension(inputPath, L".psf");
}

std::vector<fs::path> FindDescriptionXmlFiles(const fs::path& directory) {
    auto xmlFiles = FindFilesByExtension(directory, L".xml", false);
    std::stable_sort(xmlFiles.begin(), xmlFiles.end(), [](const fs::path& a, const fs::path& b) {
        const bool aExpress = ToLower(a.filename().wstring()).find(L"_express") != std::wstring::npos;
        const bool bExpress = ToLower(b.filename().wstring()).find(L"_express") != std::wstring::npos;
        if (aExpress != bExpress) {
            return aExpress;
        }
        return a.filename().wstring() < b.filename().wstring();
    });
    return xmlFiles;
}

bool ExtractArchiveWithProgress(ArchiveExtractor& extractor, const fs::path& archivePath, const fs::path& outputDirectory) {
    const int total = extractor.CountFiles(archivePath);
    Console::Out(L"Extracting %d files from %s...\n", total, archivePath.filename().c_str());
    Console::StartProgress();
    bool ok = extractor.Extract(archivePath, outputDirectory, [](int current, int totalFiles) {
        Console::UpdateProgress(current, totalFiles);
    });
    Console::FinishProgress();
    return ok;
}

void RemoveCabinetListArtifacts(const fs::path& outputDirectory, const fs::path& listFile, const std::vector<fs::path>& childCabinets) {
    for (const auto& childCabinet : childCabinets) {
        RemoveFileIfExists(outputDirectory / childCabinet);
    }
    RemoveFileIfExists(listFile);
}

} // namespace

const wchar_t* PsmPsfPackageExtractor::Name() const {
    return L"PSM + PSF";
}

bool PsmPsfPackageExtractor::CanHandle(const fs::path& inputPath) const {
    return HasExtension(inputPath, L".psm");
}

bool PsmPsfPackageExtractor::Extract(const ExtractionRequest& request) {
    fs::path payload = PsfFor(request.inputPath);
    if (!FileExists(payload)) {
        Console::Err(L"The matching PSF payload was not found: %s\n", payload.c_str());
        return false;
    }

    PsfParser parser;
    DeltaApplier applier;
    auto files = parser.ParseLegacyPsm(request.inputPath);
    if (files.empty()) {
        Console::Err(L"The PSM description did not contain any files.\n");
        return false;
    }
    EnsureDirectory(request.outputDirectory);
    return applier.ApplyFromPayload(files, request.outputDirectory, payload);
}

CabPsfPackageExtractor::CabPsfPackageExtractor(CabArchiveExtractor& cabExtractor) : cabExtractor_(cabExtractor) {}

const wchar_t* CabPsfPackageExtractor::Name() const {
    return L"CAB + PSF";
}

bool CabPsfPackageExtractor::CanHandle(const fs::path& inputPath) const {
    return HasExtension(inputPath, L".cab") && FileExists(PsfFor(inputPath));
}

bool CabPsfPackageExtractor::Extract(const ExtractionRequest& request) {
    fs::path payload = PsfFor(request.inputPath);
    if (!ExtractArchiveWithProgress(cabExtractor_, request.inputPath, request.outputDirectory)) {
        return false;
    }

    auto xmlFiles = FindDescriptionXmlFiles(request.outputDirectory);
    if (xmlFiles.empty()) {
        Console::Err(L"No XML description file was found in the CAB package.\n");
        return false;
    }

    PsfParser parser;
    DeltaApplier applier;
    auto files = parser.ParsePsfXml(xmlFiles.front());
    if (files.empty()) {
        Console::Err(L"The XML description did not contain any PSF entries.\n");
        return false;
    }
    return applier.ApplyFromPayload(files, request.outputDirectory, payload);
}

WimPsfPackageExtractor::WimPsfPackageExtractor(WimArchiveExtractor& wimExtractor) : wimExtractor_(wimExtractor) {}

const wchar_t* WimPsfPackageExtractor::Name() const {
    return L"WIM + PSF";
}

bool WimPsfPackageExtractor::CanHandle(const fs::path& inputPath) const {
    return HasExtension(inputPath, L".wim");
}

bool WimPsfPackageExtractor::Extract(const ExtractionRequest& request) {
    fs::path payload = PsfFor(request.inputPath);
    if (!FileExists(payload)) {
        Console::Err(L"The matching PSF payload was not found: %s\n", payload.c_str());
        return false;
    }

    if (!ExtractArchiveWithProgress(wimExtractor_, request.inputPath, request.outputDirectory)) {
        return false;
    }

    auto xmlFiles = FindDescriptionXmlFiles(request.outputDirectory);
    if (xmlFiles.empty()) {
        Console::Err(L"No XML description file was found in the WIM package.\n");
        return false;
    }

    PsfParser parser;
    DeltaApplier applier;
    auto files = parser.ParsePsfXml(xmlFiles.front());
    if (files.empty()) {
        Console::Err(L"The XML description did not contain any PSF entries.\n");
        return false;
    }
    return applier.ApplyFromPayload(files, request.outputDirectory, payload);
}

SingleCabPackageExtractor::SingleCabPackageExtractor(CabArchiveExtractor& cabExtractor) : cabExtractor_(cabExtractor) {}

const wchar_t* SingleCabPackageExtractor::Name() const {
    return L"Single CAB";
}

bool SingleCabPackageExtractor::CanHandle(const fs::path& inputPath) const {
    return HasExtension(inputPath, L".cab");
}

bool SingleCabPackageExtractor::Extract(const ExtractionRequest& request) {
    if (!ExtractArchiveWithProgress(cabExtractor_, request.inputPath, request.outputDirectory)) {
        return false;
    }

    fs::path cabList = request.outputDirectory / L"cabinet.cablist.ini";
    if (FileExists(cabList)) {
        return ProcessCabinetList(request.outputDirectory, cabList);
    }

    auto xmlFiles = FindDescriptionXmlFiles(request.outputDirectory);
    if (xmlFiles.empty()) {
        Console::Out(L"No XML description file was found. The CAB was extracted as a normal cabinet.\n");
        return true;
    }

    PsfParser parser;
    DeltaApplier applier;
    auto files = parser.ParseNonPsfXml(xmlFiles.front(), request.outputDirectory);
    if (files.empty()) {
        Console::Out(L"The XML description contained no delta entries. The CAB contents were left as extracted.\n");
        return true;
    }
    return applier.ApplyFromExtractedFiles(files, request.outputDirectory);
}

bool SingleCabPackageExtractor::ProcessCabinetList(const fs::path& outputDirectory, const fs::path& listFile) {
    auto childCabinets = ParseCabinetListIni(listFile);
    if (childCabinets.empty()) {
        Console::Err(L"cabinet.cablist.ini was found, but it did not list any child cabinets.\n");
        return false;
    }

    Console::Out(L"Processing %zu child cabinets from cabinet.cablist.ini...\n", childCabinets.size());
    for (size_t i = 0; i < childCabinets.size(); ++i) {
        fs::path childCab = outputDirectory / childCabinets[i];
        if (!FileExists(childCab)) {
            Console::Err(L"Listed child cabinet was not found: %s\n", childCab.c_str());
            return false;
        }
        if (!ProcessChildCabinet(outputDirectory, childCab, i + 1)) {
            return false;
        }
    }

    RemoveCabinetListArtifacts(outputDirectory, listFile, childCabinets);
    return true;
}

bool SingleCabPackageExtractor::ProcessChildCabinet(const fs::path& outputDirectory, const fs::path& childCabinet, size_t index) {
    fs::path tempRoot = outputDirectory / L".msupdateextractor";
    fs::path tempDirectory = tempRoot / (std::wstring(L"cab_") + std::to_wstring(index));
    RemoveDirectoryIfExists(tempDirectory);
    EnsureDirectory(tempDirectory);

    if (!ExtractArchiveWithProgress(cabExtractor_, childCabinet, tempDirectory)) {
        RemoveDirectoryIfExists(tempRoot);
        return false;
    }

    auto xmlFiles = FindDescriptionXmlFiles(tempDirectory);
    if (xmlFiles.empty()) {
        Console::Err(L"No XML description file was found in child cabinet %s.\n", childCabinet.filename().c_str());
        RemoveDirectoryIfExists(tempRoot);
        return false;
    }

    PsfParser parser;
    DeltaApplier applier;
    auto files = parser.ParseNonPsfXml(xmlFiles.front(), tempDirectory);
    if (!files.empty() && !applier.ApplyFromExtractedFiles(files, outputDirectory)) {
        RemoveDirectoryIfExists(tempRoot);
        return false;
    }

    RemoveDirectoryIfExists(tempDirectory);
    if (FindFilesByExtension(tempRoot, L".cab", true).empty() && FindFilesByExtension(tempRoot, L".xml", true).empty()) {
        RemoveDirectoryIfExists(tempRoot);
    }
    return true;
}

ExtractorRunner::ExtractorRunner() {
    extractors_.push_back(std::make_unique<PsmPsfPackageExtractor>());
    extractors_.push_back(std::make_unique<WimPsfPackageExtractor>(wimExtractor_));
    extractors_.push_back(std::make_unique<CabPsfPackageExtractor>(cabExtractor_));
    extractors_.push_back(std::make_unique<SingleCabPackageExtractor>(cabExtractor_));
}

bool ExtractorRunner::Run(const fs::path& inputPath) {
    if (!FileExists(inputPath)) {
        Console::Err(L"Input file does not exist: %s\n", inputPath.c_str());
        return false;
    }

    ExtractionRequest request{ inputPath, OutputDirectoryForInput(inputPath) };
    for (const auto& extractor : extractors_) {
        if (extractor->CanHandle(inputPath)) {
            Console::Out(L"Selected package type: %s\n", extractor->Name());
            Console::Out(L"Output directory: %s\n", request.outputDirectory.c_str());
            const auto start = std::chrono::steady_clock::now();
            bool ok = extractor->Extract(request);
            const auto end = std::chrono::steady_clock::now();
            const double seconds = std::chrono::duration<double>(end - start).count();
            Console::Out(L"Extraction time: %.3f seconds\n", seconds);
            return ok;
        }
    }

    Console::Err(L"Unsupported input. Expected .psm, .cab, or .wim.\n");
    return false;
}

} // namespace msupdate
