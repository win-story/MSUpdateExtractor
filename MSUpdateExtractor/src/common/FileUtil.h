#pragma once

#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>

namespace msupdate {
namespace fs = std::filesystem;

std::wstring ToLower(std::wstring value);
bool HasExtension(const fs::path& path, const std::wstring& extension);
fs::path ChangeExtension(const fs::path& path, const std::wstring& extension);
fs::path OutputDirectoryForInput(const fs::path& inputPath);

std::string WideToUtf8(const std::wstring& value);
std::wstring Utf8ToWide(const std::string& value);
std::wstring MultiByteToWideBestEffort(const char* value, UINT preferredCodePage = CP_UTF8);
std::wstring CabinetNameToWide(const char* name, bool utf8Name);

fs::path SanitizeRelativePath(std::wstring rawPath);
fs::path MakeSafeOutputPath(const fs::path& root, std::wstring rawRelativePath);

bool FileExists(const fs::path& path);
bool DirectoryExists(const fs::path& path);
bool EnsureDirectory(const fs::path& path);
bool RemoveFileIfExists(const fs::path& path);
void RemoveDirectoryIfExists(const fs::path& path);
std::vector<fs::path> FindFilesByExtension(const fs::path& root, const std::wstring& extension, bool recursive = false);
std::vector<fs::path> FindFilesByName(const fs::path& root, const std::wstring& fileName, bool recursive = false);
fs::path FindSiblingFileCaseInsensitive(const fs::path& directory, const std::wstring& fileName);
std::vector<fs::path> ParseCabinetListIni(const fs::path& iniPath);
std::wstring ReadTextFile(const fs::path& path);
std::vector<unsigned char> ReadBinaryFile(const fs::path& path);
bool WriteBinaryFile(const fs::path& path, const void* data, size_t size);
bool SetFileTimeUtc(const fs::path& path, const FILETIME& writeTime);
std::wstring ToExtendedPath(const fs::path& path);

} // namespace msupdate
