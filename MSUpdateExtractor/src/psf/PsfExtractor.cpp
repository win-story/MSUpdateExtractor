#include "PsfExtractor.h"

#include "../common/Console.h"
#include "../common/FileUtil.h"
#include "../../rapidxml.hpp"

#include <msdelta.h>
#include <patchapi.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <sstream>

#pragma comment(lib, "msdelta")
#pragma comment(lib, "mspatcha")

namespace msupdate {
namespace {

std::wstring Trim(std::wstring value) {
    auto first = std::find_if_not(value.begin(), value.end(), [](wchar_t ch) { return iswspace(ch) != 0; });
    auto last = std::find_if_not(value.rbegin(), value.rend(), [](wchar_t ch) { return iswspace(ch) != 0; }).base();
    if (first >= last) {
        return L"";
    }
    return std::wstring(first, last);
}

DeltaSourceType ParseDeltaSourceType(const wchar_t* value) {
    if (!value) {
        return DeltaSourceType::Raw;
    }
    if (_wcsicmp(value, L"PA19") == 0) {
        return DeltaSourceType::Pa19;
    }
    if (_wcsicmp(value, L"PA30") == 0) {
        return DeltaSourceType::Pa30;
    }
    return DeltaSourceType::Raw;
}

FILETIME FileTimeFromString(const wchar_t* value) {
    ULARGE_INTEGER time{};
    if (value) {
        time.QuadPart = _wcstoui64(value, nullptr, 10);
    }
    FILETIME fileTime{ time.LowPart, time.HighPart };
    return fileTime;
}

rapidxml::xml_node<wchar_t>* FirstChildElement(rapidxml::xml_node<wchar_t>* parent, const wchar_t* name) {
    return parent ? parent->first_node(name) : nullptr;
}

const wchar_t* AttributeValue(rapidxml::xml_node<wchar_t>* node, const wchar_t* name) {
    auto* attribute = node ? node->first_attribute(name) : nullptr;
    return attribute ? attribute->value() : nullptr;
}

std::vector<unsigned char> EmptyOrRead(const fs::path& path) {
    if (path.empty() || !FileExists(path)) {
        return {};
    }
    return ReadBinaryFile(path);
}

} // namespace

std::vector<DeltaFile> PsfParser::ParseLegacyPsm(const fs::path& descriptionPath) const {
    std::vector<DeltaFile> result;
    std::wistringstream stream(ReadTextFile(descriptionPath));
    std::wstring line;
    std::wstring currentSection;

    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == L';' || line[0] == L'#') {
            continue;
        }
        if (line.front() == L'[') {
            const size_t end = line.find(L']');
            if (end != std::wstring::npos) {
                currentSection = line.substr(1, end - 1);
            }
            continue;
        }

        const size_t equals = line.find(L'=');
        if (equals == std::wstring::npos || currentSection.empty()) {
            continue;
        }

        std::wstring key = Trim(line.substr(0, equals));
        std::wstring value = Trim(line.substr(equals + 1));
        const size_t comma = value.find(L',');
        if (comma == std::wstring::npos) {
            continue;
        }

        DeltaFile item{};
        item.name = currentSection;
        item.offset.QuadPart = _wcstoui64(Trim(value.substr(0, comma)).c_str(), nullptr, 16);
        item.length = static_cast<DWORD>(wcstoul(Trim(value.substr(comma + 1)).c_str(), nullptr, 16));

        if (_wcsicmp(key.c_str(), L"p0") == 0) {
            item.type = DeltaSourceType::Pa19;
        } else if (_wcsicmp(key.c_str(), L"full") == 0) {
            item.type = DeltaSourceType::Raw;
        } else {
            continue;
        }

        result.push_back(item);
        currentSection.clear();
    }

    return result;
}

std::vector<DeltaFile> PsfParser::ParsePsfXml(const fs::path& descriptionPath) const {
    std::vector<DeltaFile> result;
    std::wstring xmlText = ReadTextFile(descriptionPath);
    rapidxml::xml_document<wchar_t> document;
    if (!xmlText.empty()) {
        xmlText.push_back(L'\0');
        document.parse<0>(xmlText.data());
    }

    auto* filesNode = FirstChildElement(FirstChildElement(&document, L"Container"), L"Files");
    for (auto* node = filesNode ? filesNode->first_node() : nullptr; node; node = node->next_sibling()) {
        auto* deltaNode = FirstChildElement(node, L"Delta");
        auto* sourceNode = FirstChildElement(deltaNode, L"Source");
        if (!sourceNode) {
            continue;
        }

        DeltaFile item{};
        item.name = AttributeValue(node, L"name") ? AttributeValue(node, L"name") : L"";
        item.time = FileTimeFromString(AttributeValue(node, L"time"));
        item.type = ParseDeltaSourceType(AttributeValue(sourceNode, L"type"));
        item.offset.QuadPart = _wcstoui64(AttributeValue(sourceNode, L"offset") ? AttributeValue(sourceNode, L"offset") : L"0", nullptr, 10);
        item.length = static_cast<DWORD>(wcstoul(AttributeValue(sourceNode, L"length") ? AttributeValue(sourceNode, L"length") : L"0", nullptr, 10));
        result.push_back(item);
    }

    return result;
}

std::vector<DeltaFile> PsfParser::ParseNonPsfXml(const fs::path& descriptionPath, const fs::path& extractedRoot) const {
    std::vector<DeltaFile> result;
    std::wstring xmlText = ReadTextFile(descriptionPath);
    rapidxml::xml_document<wchar_t> document;
    if (!xmlText.empty()) {
        xmlText.push_back(L'\0');
        document.parse<0>(xmlText.data());
    }

    auto* filesNode = FirstChildElement(FirstChildElement(&document, L"Container"), L"Files");
    for (auto* node = filesNode ? filesNode->first_node() : nullptr; node; node = node->next_sibling()) {
        auto* deltaNode = FirstChildElement(node, L"Delta");
        auto* sourceNode = FirstChildElement(deltaNode, L"Source");
        auto* basisNode = FirstChildElement(deltaNode, L"Basis");

        DeltaFile item{};
        item.name = AttributeValue(node, L"name") ? AttributeValue(node, L"name") : L"";
        item.id = static_cast<int>(_wtoi(AttributeValue(node, L"id") ? AttributeValue(node, L"id") : L"0"));
        item.time = FileTimeFromString(AttributeValue(node, L"time"));
        item.type = sourceNode ? ParseDeltaSourceType(AttributeValue(sourceNode, L"type")) : DeltaSourceType::Raw;
        item.basis = basisNode ? static_cast<int>(_wtoi(AttributeValue(basisNode, L"file") ? AttributeValue(basisNode, L"file") : L"0")) : 0;

        const wchar_t* sourceName = sourceNode ? AttributeValue(sourceNode, L"name") : nullptr;
        if (sourceName && *sourceName) {
            item.basisPath = MakeSafeOutputPath(extractedRoot, sourceName);
        }
        result.push_back(item);
    }

    return result;
}

fs::path DeltaApplier::OutputPathFor(const fs::path& targetDirectory, const std::wstring& fileName) const {
    return MakeSafeOutputPath(targetDirectory, fileName);
}

bool DeltaApplier::ReadPayloadSlice(HANDLE payloadHandle, const DeltaFile& item, std::vector<unsigned char>& buffer) const {
    buffer.assign(item.length, 0);
    LARGE_INTEGER offset{};
    offset.QuadPart = item.offset.QuadPart;
    if (!SetFilePointerEx(payloadHandle, offset, nullptr, FILE_BEGIN)) {
        Console::Err(L"Failed to seek the PSF payload for %s. Error: %lu\n", item.name.c_str(), GetLastError());
        return false;
    }
    DWORD read = 0;
    if (item.length > 0 && !ReadFile(payloadHandle, buffer.data(), item.length, &read, nullptr)) {
        Console::Err(L"Failed to read the PSF payload for %s. Error: %lu\n", item.name.c_str(), GetLastError());
        return false;
    }
    return read == item.length;
}

bool DeltaApplier::ApplyOne(const DeltaFile& item, const std::vector<unsigned char>& deltaBuffer, const std::vector<unsigned char>& basisBuffer, const fs::path& outputPath) const {
    EnsureDirectory(outputPath.parent_path());

    DWORD written = 0;
    HANDLE output = CreateFileW(ToExtendedPath(outputPath).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        Console::Err(L"Failed to create %s. Error: %lu\n", outputPath.c_str(), GetLastError());
        return false;
    }

    bool ok = false;
    if (item.type == DeltaSourceType::Pa19) {
        PBYTE patched = nullptr;
        ULONG patchedSize = 0;
        PBYTE oldBytes = basisBuffer.empty() ? nullptr : const_cast<PBYTE>(basisBuffer.data());
        ULONG oldSize = static_cast<ULONG>(basisBuffer.size());
        if (!ApplyPatchToFileByBuffers(const_cast<PBYTE>(deltaBuffer.data()), static_cast<ULONG>(deltaBuffer.size()), oldBytes, oldSize, &patched, 0, &patchedSize, const_cast<FILETIME*>(&item.time), NULL, nullptr, nullptr)) {
            Console::Err(L"Failed to apply PA19 patch for %s. Error: %lu\n", item.name.c_str(), GetLastError());
            CloseHandle(output);
            return false;
        }
        ok = WriteFile(output, patched, patchedSize, &written, nullptr) && written == patchedSize;
        VirtualFree(patched, 0, MEM_RELEASE);
    } else if (item.type == DeltaSourceType::Pa30) {
        DELTA_INPUT deltaInput{ const_cast<unsigned char*>(deltaBuffer.data()), deltaBuffer.size(), FALSE };
        DELTA_INPUT basisInput{ basisBuffer.empty() ? nullptr : const_cast<unsigned char*>(basisBuffer.data()), basisBuffer.size(), FALSE };
        DELTA_OUTPUT deltaOutput{};
        if (!ApplyDeltaB(0, basisInput, deltaInput, &deltaOutput)) {
            Console::Err(L"Failed to apply PA30 delta for %s. Error: %lu\n", item.name.c_str(), GetLastError());
            CloseHandle(output);
            return false;
        }
        ok = WriteFile(output, deltaOutput.lpStart, static_cast<DWORD>(deltaOutput.uSize), &written, nullptr) && written == deltaOutput.uSize;
        DeltaFree(deltaOutput.lpStart);
    } else {
        const std::vector<unsigned char>& data = !deltaBuffer.empty() ? deltaBuffer : basisBuffer;
        ok = data.empty() || (WriteFile(output, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) && written == data.size());
    }

    if (!ok) {
        Console::Err(L"Failed to write %s. Error: %lu\n", outputPath.c_str(), GetLastError());
        CloseHandle(output);
        return false;
    }

    SetFileTime(output, nullptr, nullptr, &item.time);
    CloseHandle(output);
    return true;
}

bool DeltaApplier::ApplyFromPayload(const std::vector<DeltaFile>& files, const fs::path& targetDirectory, const fs::path& payloadFile) const {
    HANDLE payload = CreateFileW(ToExtendedPath(payloadFile).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (payload == INVALID_HANDLE_VALUE) {
        Console::Err(L"Failed to open PSF payload %s. Error: %lu\n", payloadFile.c_str(), GetLastError());
        return false;
    }

    Console::Out(L"Writing %zu files...\n", files.size());
    Console::StartProgress();

    int current = 0;
    for (const auto& item : files) {
        std::vector<unsigned char> deltaBuffer;
        if (!ReadPayloadSlice(payload, item, deltaBuffer)) {
            CloseHandle(payload);
            return false;
        }
        if (!ApplyOne(item, deltaBuffer, {}, OutputPathFor(targetDirectory, item.name))) {
            CloseHandle(payload);
            return false;
        }
        Console::UpdateProgress(++current, static_cast<int>(files.size()));
    }

    Console::FinishProgress();
    CloseHandle(payload);
    return true;
}

bool DeltaApplier::ApplyFromExtractedFiles(const std::vector<DeltaFile>& files, const fs::path& targetDirectory) const {
    std::map<int, DeltaFile> byId;
    for (const auto& item : files) {
        byId[item.id] = item;
    }

    Console::Out(L"Writing %zu files...\n", files.size());
    Console::StartProgress();

    int current = 0;
    for (const auto& item : files) {
        std::vector<unsigned char> deltaBuffer = EmptyOrRead(item.basisPath);
        std::vector<unsigned char> basisBuffer;

        if (item.basis != 0) {
            auto basisIt = byId.find(item.basis);
            if (basisIt != byId.end()) {
                basisBuffer = EmptyOrRead(OutputPathFor(targetDirectory, basisIt->second.name));
            }
        }

        if (item.basisPath.empty() && item.basis == 0 && deltaBuffer.empty()) {
            Console::UpdateProgress(++current, static_cast<int>(files.size()));
            continue;
        }
        else if (item.basisPath == OutputPathFor(targetDirectory, item.name)) {
            Console::UpdateProgress(++current, static_cast<int>(files.size()));
            continue;
        }

        if (!ApplyOne(item, deltaBuffer, basisBuffer, OutputPathFor(targetDirectory, item.name))) {
            return false;
        }

        if (!item.basisPath.empty()) {
            RemoveFileIfExists(item.basisPath);
        }

        Console::UpdateProgress(++current, static_cast<int>(files.size()));
    }

    Console::FinishProgress();
    return true;
}

} // namespace msupdate
