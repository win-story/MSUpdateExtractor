#include "FileUtil.h"

#include <algorithm>
#include <cwctype>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

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

bool StartsWithI(const std::wstring& value, const std::wstring& prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    return ToLower(value.substr(0, prefix.size())) == ToLower(prefix);
}

std::wstring MultiByteToWideChecked(const char* value, int length, UINT codePage, DWORD flags) {
    if (!value) {
        return L"";
    }
    int required = MultiByteToWideChar(codePage, flags, value, length, nullptr, 0);
    if (required <= 0) {
        return L"";
    }
    std::wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(codePage, flags, value, length, result.data(), required);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

std::string WideToMultiByte(const std::wstring& value, UINT codePage) {
    if (value.empty()) {
        return {};
    }
    int required = WideCharToMultiByte(codePage, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(codePage, 0, value.c_str(), -1, result.data(), required, nullptr, nullptr);
    if (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

} // namespace

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

bool HasExtension(const fs::path& path, const std::wstring& extension) {
    return ToLower(path.extension().wstring()) == ToLower(extension);
}

fs::path ChangeExtension(const fs::path& path, const std::wstring& extension) {
    fs::path result = path;
    result.replace_extension(extension);
    return result;
}

fs::path OutputDirectoryForInput(const fs::path& inputPath) {
    fs::path result = inputPath;
    result.replace_extension(L"");
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    return WideToMultiByte(value, CP_UTF8);
}

std::wstring Utf8ToWide(const std::string& value) {
    return MultiByteToWideChecked(value.c_str(), -1, CP_UTF8, MB_ERR_INVALID_CHARS);
}

std::wstring MultiByteToWideBestEffort(const char* value, UINT preferredCodePage) {
    if (!value) {
        return L"";
    }
    std::wstring converted = MultiByteToWideChecked(value, -1, preferredCodePage, preferredCodePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0);
    if (!converted.empty()) {
        return converted;
    }
    converted = MultiByteToWideChecked(value, -1, CP_ACP, 0);
    if (!converted.empty()) {
        return converted;
    }
    return MultiByteToWideChecked(value, -1, CP_UTF8, 0);
}

std::wstring CabinetNameToWide(const char* name, bool utf8Name) {
    return MultiByteToWideBestEffort(name, utf8Name ? CP_UTF8 : CP_ACP);
}

fs::path SanitizeRelativePath(std::wstring rawPath) {
    std::replace(rawPath.begin(), rawPath.end(), L'/', L'\\');
    while (!rawPath.empty() && (rawPath.front() == L'\\' || rawPath.front() == L'/')) {
        rawPath.erase(rawPath.begin());
    }

    fs::path input(rawPath);
    fs::path safe;
    for (const auto& part : input) {
        std::wstring piece = part.wstring();
        if (piece.empty() || piece == L"." || piece == L".." || piece == L"\\" || piece == L"/") {
            continue;
        }
        if (piece.size() == 2 && piece[1] == L':') {
            continue;
        }
        safe /= piece;
    }
    return safe;
}

fs::path MakeSafeOutputPath(const fs::path& root, std::wstring rawRelativePath) {
    return root / SanitizeRelativePath(std::move(rawRelativePath));
}

bool FileExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

bool DirectoryExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_directory(path, ec);
}

bool EnsureDirectory(const fs::path& path) {
    std::error_code ec;
    if (path.empty()) {
        return true;
    }
    if (fs::exists(path, ec)) {
        return fs::is_directory(path, ec);
    }
    return fs::create_directories(path, ec) || fs::exists(path, ec);
}

bool RemoveFileIfExists(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return true;
    }
    return fs::remove(path, ec);
}

void RemoveDirectoryIfExists(const fs::path& path) {
    std::error_code ec;
    if (fs::exists(path, ec)) {
        fs::remove_all(path, ec);
    }
}

std::vector<fs::path> FindFilesByExtension(const fs::path& root, const std::wstring& extension, bool recursive) {
    std::vector<fs::path> result;
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        return result;
    }

    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec) && HasExtension(entry.path(), extension)) {
                result.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec) && HasExtension(entry.path(), extension)) {
                result.push_back(entry.path());
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<fs::path> FindFilesByName(const fs::path& root, const std::wstring& fileName, bool recursive) {
    std::vector<fs::path> result;
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        return result;
    }
    const std::wstring wanted = ToLower(fileName);
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec) && ToLower(entry.path().filename().wstring()) == wanted) {
                result.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec) && ToLower(entry.path().filename().wstring()) == wanted) {
                result.push_back(entry.path());
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

fs::path FindSiblingFileCaseInsensitive(const fs::path& directory, const std::wstring& fileName) {
    std::error_code ec;
    if (!fs::exists(directory, ec)) {
        return {};
    }
    const std::wstring wanted = ToLower(fileName);
    for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec) && ToLower(entry.path().filename().wstring()) == wanted) {
            return entry.path();
        }
    }
    return directory / fileName;
}

std::vector<fs::path> ParseCabinetListIni(const fs::path& iniPath) {
    std::vector<std::pair<int, fs::path>> ordered;
    const std::wstring text = ReadTextFile(iniPath);
    std::wistringstream stream(text);
    std::wstring line;

    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == L';' || line[0] == L'#' || line[0] == L'[') {
            continue;
        }
        const size_t equals = line.find(L'=');
        if (equals == std::wstring::npos) {
            continue;
        }
        std::wstring key = Trim(line.substr(0, equals));
        std::wstring value = Trim(line.substr(equals + 1));
        if (value.size() >= 2 && ((value.front() == L'"' && value.back() == L'"') || (value.front() == L'\'' && value.back() == L'\''))) {
            value = value.substr(1, value.size() - 2);
        }
        if (!StartsWithI(key, L"Cabinet")) {
            continue;
        }
        int index = 0;
        try {
            index = std::stoi(key.substr(7));
        } catch (...) {
            index = static_cast<int>(ordered.size()) + 1;
        }
        fs::path safe = SanitizeRelativePath(value);
        ordered.emplace_back(index, safe);
    }

    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::vector<fs::path> result;
    for (const auto& item : ordered) {
        result.push_back(item.second);
    }
    return result;
}

std::wstring ReadTextFile(const fs::path& path) {
    std::vector<unsigned char> data = ReadBinaryFile(path);
    if (data.empty()) {
        return L"";
    }

    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        size_t charCount = (data.size() - 2) / sizeof(wchar_t);
        std::wstring result(charCount, L'\0');
        memcpy(result.data(), data.data() + 2, charCount * sizeof(wchar_t));
        return result;
    }
    if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return MultiByteToWideChecked(reinterpret_cast<const char*>(data.data() + 3), static_cast<int>(data.size() - 3), CP_UTF8, 0);
    }
    std::wstring utf8 = MultiByteToWideChecked(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), CP_UTF8, MB_ERR_INVALID_CHARS);
    if (!utf8.empty()) {
        return utf8;
    }
    return MultiByteToWideChecked(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), CP_ACP, 0);
}

std::vector<unsigned char> ReadBinaryFile(const fs::path& path) {
    HANDLE file = CreateFileW(ToExtendedPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return {};
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > SIZE_MAX) {
        CloseHandle(file);
        return {};
    }

    std::vector<unsigned char> data(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    bool ok = data.empty() || ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != data.size()) {
        return {};
    }
    return data;
}

bool WriteBinaryFile(const fs::path& path, const void* data, size_t size) {
    EnsureDirectory(path.parent_path());
    HANDLE file = CreateFileW(ToExtendedPath(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    bool ok = size == 0 || WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr);
    CloseHandle(file);
    return ok && written == size;
}

bool SetFileTimeUtc(const fs::path& path, const FILETIME& writeTime) {
    HANDLE file = CreateFileW(ToExtendedPath(path).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    BOOL ok = SetFileTime(file, nullptr, nullptr, &writeTime);
    CloseHandle(file);
    return ok != FALSE;
}

std::wstring ToExtendedPath(const fs::path& path) {
    std::wstring text = path.wstring();
    if (text.empty() || StartsWithI(text, L"\\\\?\\")) {
        return text;
    }
    if (StartsWithI(text, L"\\\\")) {
        return L"\\\\?\\UNC\\" + text.substr(2);
    }
    return L"\\\\?\\" + fs::absolute(path).wstring();
}

} // namespace msupdate
