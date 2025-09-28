#pragma once

#include <CommonStuff/NoCopy.h>

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <span>

class CvVFS : public heck::NoCopyNoMove // self-referencing
{
public:
	explicit CvVFS(const std::filesystem::path& cv4EngineRootDir, const std::filesystem::path& vanillaCiv4RootDir, const std::wstring& optModName);
	~CvVFS() noexcept;

	std::filesystem::path resolve(const std::filesystem::path& path) const;
	bool exists(const std::filesystem::path& path) const;
	//std::filesystem::path resolveAsset(const std::filesystem::path& path);
	std::ifstream open(const std::filesystem::path& path, std::ios_base::openmode mode) const;

	std::vector<std::filesystem::path> enumerateExtRecursive(const std::filesystem::path& path, const std::wstring& filenamePrefix, const std::wstring& filenameSuffix) const;
	std::vector<std::filesystem::path> enumerateExtNonRecursive(const std::filesystem::path& path, const std::wstring& ext) const;
	std::vector<std::filesystem::path> enumeratePhysicalDirsContainingExt(const std::wstring& ext) const;

	std::optional<std::string> loadPythonCodeIfExists(const std::string& filename, std::filesystem::path& vfsPathOut) const;

	const char* getModName(bool fullPath) const;

	// "Mods\Next War\". Used for save files.
	std::string getModRelPathString() const;

private:
	struct Internals;
	std::unique_ptr<Internals> mInternals;

	
};

// Set once in main.
extern const CvVFS* gVFS;