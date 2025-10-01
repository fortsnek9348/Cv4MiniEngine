#include "CvVFS.h"
#include "Common.h"

#include <CvString.h>

#include <CommonStuff/Hashing.h>
#include <CommonStuff/StringConversion.h>

#include <algorithm>
#include <ranges>
#include <fstream>
#include <unordered_set>
#include <set>
#include <map>
#include <cctype>
#include <cwctype>
#include <memory>

namespace fs = std::filesystem;

#ifndef _WIN32
#define IS_CASE_SENSITIVE_PLATFORM 1
#endif

constinit const CvVFS* ::gVFS = nullptr;

#define BUILD_FILE_CATALOG 0

namespace
{
	[[nodiscard]] wchar_t vfsCaseFoldWChar(wchar_t c)
	{
		return static_cast<wchar_t>(towupper(c));
	}

	[[nodiscard]] std::string toLower(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

#if BUILD_FILE_CATALOG
	// VFS file cataloging. I don't like it, it's slow for startup, and it's only needed because Linux.
	// This could probably be avoided by doing case-insensitive file IO open/enumeration.

	// VFS will mimic NTFS which uses UCS-2: Doing case folding on char16_t should be enough.
	// (for example: "𐐀𐐀𐐀" and "𐐨𐐨𐐨" are different filenames because the characters are outside the BMP) 
	// For reference: https://www.unicode.org/Public/12.1.0/ucd/CaseFolding.txt
	// VfsString is always stored case folded, using single-character mappings.
	using VfsString = std::u16string;
	using VfsStringView = std::u16string_view;

	struct VfsDirEntry;

	using VfsDirEntriesMap = std::unordered_map<VfsString, VfsDirEntry>;

	struct MountRelPath
	{
		size_t fileMountPointI = SIZE_MAX;
		fs::path relPath;

		friend bool operator==(const MountRelPath&, const MountRelPath&) = default;

		explicit operator bool() const
		{
			return fileMountPointI != SIZE_MAX;
		}
	};

	struct MountRelPathHash
	{
		size_t operator()(const MountRelPath& x) const noexcept
		{
			return heck::theIronBornHash32(static_cast<uint32_t>(x.fileMountPointI)) + std::hash<fs::path>()(x.relPath);
		}
	};

	struct VfsDirEntry
	{
		MountRelPath fileMountRelPath;
		std::unique_ptr<VfsDirEntriesMap> folderDirEntries;

		// NOTE: Can be both a file and a folder: Sid Meier's Civilization 4\Beyond the Sword\Assets\assets0.fpk\

		//bool isFolder() const
		//{
		//	return !!folderDirEntries;
		//}
	};

	static VfsString vfsCaseFold(std::wstring_view s)
	{
#ifdef _WIN32
		return VfsString(std::from_range, s | std::views::transform(vfsCaseFoldWChar));
#else
		std::wstring caseFoldedWideStr(s);
		for (wchar_t& c : caseFoldedWideStr)
			c = vfsCaseFoldWChar(c);
		// Need to convert UTF-32 to UTF-16.
		return heck::toUtf16(caseFoldedWideStr);
#endif
	}

	VfsDirEntriesMap enumeratePhysical(const fs::path& mountRoot, const fs::path& mountRelPath, size_t fileMountPointI)
	{
		VfsDirEntriesMap map;

		std::error_code ec{};
		auto it = fs::directory_iterator(mountRoot / mountRelPath, fs::directory_options::skip_permission_denied, ec);
		if (ec == std::errc::no_such_file_or_directory)
			return map;

		for (const fs::directory_entry& dirEntry : it)
		{
			const fs::path& path = dirEntry.path();
			fs::path dirEntryMountRelPath = mountRelPath / path.filename();
			VfsDirEntry vfsDirEntry;
			if (dirEntry.is_directory())
				vfsDirEntry.folderDirEntries = std::make_unique<VfsDirEntriesMap>(enumeratePhysical(mountRoot, dirEntryMountRelPath, fileMountPointI));
			else
				vfsDirEntry.fileMountRelPath = { fileMountPointI, std::move(dirEntryMountRelPath) };
			map.emplace(vfsCaseFold(path.filename().wstring()), std::move(vfsDirEntry));
		}
		return map;
	}

	void vfsMerge(VfsDirEntriesMap& target, VfsDirEntriesMap& src, const fs::path& srcPhysPath)
	{
		for (auto& [srcName, srcDirEntry] : src)
		{
			if (const auto targetIt = target.find(srcName); targetIt != target.end())
			{
				// assets0.fpk
				//if (targetIt->second.isFolder() != srcDirEntry.isFolder())
				//	throw std::filesystem::filesystem_error("VFS merge error. File/folder directory entry type mismatch.", srcPhysPath / srcName, std::error_code());

				if (targetIt->second.folderDirEntries && srcDirEntry.folderDirEntries)
					vfsMerge(*targetIt->second.folderDirEntries, *srcDirEntry.folderDirEntries, srcPhysPath / srcName);
				else if (srcDirEntry.folderDirEntries)
					targetIt->second.folderDirEntries = std::move(srcDirEntry.folderDirEntries);

				if (targetIt->second.fileMountRelPath && srcDirEntry.fileMountRelPath && targetIt->second.fileMountRelPath.fileMountPointI == srcDirEntry.fileMountRelPath.fileMountPointI)
					throw std::filesystem::filesystem_error("VFS merge error. Two files from the same mount merge to the same VFS entry. Likely to be filenames that differ in only case.", srcPhysPath / srcName, std::error_code());
				else if (srcDirEntry.fileMountRelPath)
					targetIt->second.fileMountRelPath = std::move(srcDirEntry.fileMountRelPath);
			}
			else
			{
				// New entry.
				target.emplace(srcName, std::move(srcDirEntry));
			}
		}
	}

	std::vector<VfsString> vfsSegmentPath(fs::path path)
	{
		if (!path.is_relative())
			throw std::runtime_error("VFS should not be given physical/absolute paths.");

		std::vector<VfsString> segments;
		while (path.has_filename())
		{
			segments.push_back(vfsCaseFold(path.filename().wstring()));
			path = path.parent_path();
		}
		std::ranges::reverse(segments);
		return segments;
	}


	const MountRelPath* vfsFindFile(const VfsDirEntriesMap& root, std::span<const VfsString> path)
	{
		if (path.empty())
			return nullptr;

		if (const auto targetIt = root.find(path.front()); targetIt != root.end())
		{
			if (path.size() == 1 && targetIt->second.fileMountRelPath)
				return &targetIt->second.fileMountRelPath;

			if (targetIt->second.folderDirEntries)
				return vfsFindFile(*targetIt->second.folderDirEntries, path.subspan(1));
		}

		return nullptr;
	}

	const VfsDirEntriesMap* vfsFindFolder(const VfsDirEntriesMap* root, std::span<const VfsString> path)
	{
		if (path.empty())
			return root;

		if (const auto targetIt = root->find(path.front()); targetIt != root->end())
			if (targetIt->second.folderDirEntries)
				return vfsFindFolder(targetIt->second.folderDirEntries.get(), path.subspan(1));

		return nullptr;
	}
#else
	

	//struct CIPathEq
	//{
	//	static bool operator()(const fs::path& a, const fs::path& b) noexcept
	//	{
	//		return a == b || std::ranges::equal(a.generic_wstring(), b.generic_wstring(), std::equal_to<>(), vfsCaseFoldWChar, vfsCaseFoldWChar);
	//	}
	//};

	struct CIPathCmp
	{
		static bool operator()(const fs::path& a, const fs::path& b) noexcept
		{
			return std::ranges::lexicographical_compare(a.generic_wstring(), b.generic_wstring(), std::less<>(), vfsCaseFoldWChar, vfsCaseFoldWChar);
		}
	};

	//struct CIPathHash
	//{
	//	static size_t operator()(const fs::path& a) noexcept
	//	{
	//		std::wstring str = a.generic_wstring();
	//		std::ranges::for_each(str, vfsCaseFoldWChar);
	//		return std::hash<decltype(str)>()(str);
	//	}
	//};
#endif

	std::optional<fs::recursive_directory_iterator> getRecursiveDirectoryIteratorIfExists(const fs::path& root)
	{
		try
		{
			return fs::recursive_directory_iterator(root, fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied);
		}
		catch (const fs::filesystem_error& ex)
		{
			if (ex.code() == std::errc::no_such_file_or_directory)
				return std::nullopt;
			throw;
		}
	}
}

struct CvVFS::Internals
{
	explicit Internals(const std::filesystem::path& cv4EngineRootDir, const std::filesystem::path& vanillaCiv4RootDir, fs::path optModRelPath)
	{
		const fs::path btsRoot = vanillaCiv4RootDir / "Beyond the Sword";

		// https://forums.civfanatics.com/threads/differences-between-assets-folders.685453/#post-16505661
		mMountings.insert(mMountings.end(), {
			vanillaCiv4RootDir / "Assets"                  /**/,
			vanillaCiv4RootDir / kPublicMapsDirName        /**/,
			vanillaCiv4RootDir / "Warlords" / "Assets"     /**/,
			vanillaCiv4RootDir / "Warlords" / kPublicMapsDirName /**/,
			btsRoot / "Assets"                             /**/,
			btsRoot / kPublicMapsDirName                   /**/,
			getUserConfigDir() / kCustomAssetsDirName     /**/,
			getUserConfigDir() / kPublicMapsDirName       /**/,
		});

		// Remove trailing slash.
		if (optModRelPath.has_parent_path() && !optModRelPath.has_filename())
			optModRelPath = optModRelPath.parent_path();

		if (!optModRelPath.empty())
		{
			if (!fs::exists(btsRoot / optModRelPath))
				throw std::runtime_error("Mod does not exist.");
			mMountings.insert(mMountings.end(), {
				btsRoot / optModRelPath / "Assets"      /**/,
				btsRoot / optModRelPath / kPublicMapsDirName  /**/,
				btsRoot / optModRelPath / "PrivateMaps" /**/,
			});
		}

		// Make this absolute so that file enumeration always returns absolute physical paths.
		// And it looks like empty paths are unchanged by absolute.
		const fs::path absCv4EngineRootDir = cv4EngineRootDir.empty() ? fs::current_path() : fs::absolute(cv4EngineRootDir);

		// Cv4MiniEngine overrides.
		mMountings.insert(mMountings.end(), {
			absCv4EngineRootDir / kCustomAssetsDirName /**/,
			absCv4EngineRootDir / kPublicMapsDirName         /**/,
		});

		mPythonModuleLookup.reserve(200);

#if BUILD_FILE_CATALOG
		for (size_t i = 0; i < mMountings.size(); ++i)
		{
			VfsDirEntriesMap mountTree = enumeratePhysical(mMountings[i], fs::path(), i);
			vfsMerge(mVfsRoot, mountTree, mMountings[i]);
		}
		
		buildPythonModuleLookup(mVfsRoot, false);
		buildPythonModuleLookup(*mVfsRoot.at(vfsCaseFold(L"Python")).folderDirEntries, true); // Let's hope there are no conflicts with maps.
#else
		buildPythonModuleLookup("", false);
		buildPythonModuleLookup("Python", true); // Let's hope there are no conflicts with maps.
#endif

		mModName = optModRelPath.filename().wstring();
		if (!mModName.empty())
		{
			mModFullPath = (btsRoot / optModRelPath).wstring();
			mModRelPath = optModRelPath.wstring();
			// Used for save files, we need a specific format.
			std::ranges::replace(mModRelPath, L'/', L'\\');
			mModRelPath += L'\\';
		}
	}

	// We'll now store these as wstrings. If anybody needs ASCII/codepage strings, they'll have to convert where used.
	std::wstring mModName;
	std::wstring mModFullPath;
	std::wstring mModRelPath;

	std::vector<fs::path> mMountings;

	// VFS file tree implementation
#if BUILD_FILE_CATALOG
	VfsDirEntriesMap mVfsRoot;

	std::unordered_map<std::string, const MountRelPath*> mPythonModuleLookup;

	void buildPythonModuleLookup(const VfsDirEntriesMap& vfsFolder, bool recursive)
	{
		static const VfsString kExt = vfsCaseFold(L".py");

		for (const auto& [filename, dirEntry] : vfsFolder)
		{
			if (recursive && dirEntry.folderDirEntries)
				buildPythonModuleLookup(*dirEntry.folderDirEntries, recursive);
			
			if (dirEntry.fileMountRelPath && filename.ends_with(kExt))
				mPythonModuleLookup.emplace(toLower(dirEntry.fileMountRelPath.relPath.stem().string()), &dirEntry.fileMountRelPath);
		}

		//for (const Mounting& mounting : mMountings | std::views::reverse)
		//{
		//	std::error_code ec;
		//	// Directory might not exist.
		//	for (const fs::directory_entry& dirEntry : fs::recursive_directory_iterator(mounting.physPath, ec))
		//		if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".py")
		//			mPythonModuleLookup.emplace(dirEntry.path().stem().string(), PythonModuleSourceEntry{ mounting, dirEntry.path() });
		//}
	}

	

	//template<class Iterator>
	//std::vector<std::filesystem::path> enumerateExt(const std::filesystem::path& path, const std::filesystem::path& ext, bool phys) const;

	static void enumerateExt(std::vector<fs::path>& foundVfsFilePathsOut, const VfsDirEntriesMap& vfsFolder, const fs::path& vfsPath,
		const VfsString& filenamePrefix, const VfsString& filenameSuffix, bool recursive)
	{
		//return enumerateExt<fs::directory_iterator>(path, ext, false);

		for (const auto& [name, dirEntry] : vfsFolder)
		{
			if (recursive && dirEntry.folderDirEntries)
				enumerateExt(foundVfsFilePathsOut, *dirEntry.folderDirEntries, vfsPath, filenamePrefix, filenameSuffix, recursive);
			
			if (dirEntry.fileMountRelPath && name.starts_with(filenamePrefix) && name.ends_with(filenameSuffix))
				foundVfsFilePathsOut.push_back(vfsPath / dirEntry.fileMountRelPath.relPath.filename()); // USe the filename with the original casing, for original map script names.
		}
	}

	void enumeratePhysicalDirsContainingExt(std::unordered_set<MountRelPath, MountRelPathHash>& physDirPathsSet, std::vector<fs::path>& physDirPathsList, const VfsDirEntriesMap& vfsFolder, const VfsString& ext) const
	{
		for (const auto& [name, dirEntry] : vfsFolder)
		{
			if (dirEntry.folderDirEntries)
				enumeratePhysicalDirsContainingExt(physDirPathsSet, physDirPathsList, *dirEntry.folderDirEntries, ext);

			if (dirEntry.fileMountRelPath && name.ends_with(ext))
			{
				const fs::path folderMountRelPath = dirEntry.fileMountRelPath.relPath.parent_path();
				// physDirPathsList shall be deterministically ordered based on VFS tree.
				if (physDirPathsSet.emplace(dirEntry.fileMountRelPath.fileMountPointI, folderMountRelPath).second)
					physDirPathsList.push_back(mMountings[dirEntry.fileMountRelPath.fileMountPointI] / folderMountRelPath);
			}
		}
	}
	std::vector<fs::path> enumeratePhysicalDirsContainingExt(const std::wstring& ext) const
	{
		//return enumerateExt<fs::directory_iterator>(path, ext, false);

		std::unordered_set<MountRelPath, MountRelPathHash> set;
		std::vector<fs::path> list;
		enumeratePhysicalDirsContainingExt(set, list, mVfsRoot, vfsCaseFold(ext));
		return list;
	}

	std::optional<fs::path> tryResolve(const fs::path& path) const
	{
		const std::vector<VfsString> segments = vfsSegmentPath(path);
		if (const MountRelPath* const entry = vfsFindFile(mVfsRoot, segments))
			return mMountings[entry->fileMountPointI] / entry->relPath;
		else
			return std::nullopt;
	}

#else
	struct EnumeratedFile
	{
		fs::path vfsPath;
		fs::path physPath;
	};

	// Lower-case
	std::unordered_map<std::string, EnumeratedFile> mPythonModuleLookup;

	void buildPythonModuleLookup(const fs::path& vfsRoot, bool recursive)
	{
		std::vector<fs::path> vfsPaths;
		std::vector<fs::path> physPaths = enumerateFilesWithExt(vfsRoot, {}, L".py", recursive, &vfsPaths);

		for (auto&& [vfsPath, physPath] : std::views::zip(vfsPaths, physPaths))
		{
			std::string name = toLower(vfsPath.stem().string());
			mPythonModuleLookup.emplace(std::move(name), EnumeratedFile{ std::move(vfsPath), std::move(physPath) });
		}
	}

	std::vector<fs::path> enumerateFilesWithExt(const fs::path& vfsPath, const std::wstring& filenamePrefix, const std::wstring& filenameSuffix, bool recursive,
		std::vector<fs::path>* vfsPathsOut) const
	{
		//return enumerateExt<fs::recursive_directory_iterator>(path, ext, false);

		//const auto vfsPathSegments = vfsSegmentPath(vfsPath);
		//std::vector<fs::path> foundVfsFilePaths;
		//if (const VfsDirEntriesMap* const folder = vfsFindFolder(&mInternals->mVfsRoot, vfsPathSegments))
		//	Internals::enumerateExt(foundVfsFilePaths, *folder, vfsPath, vfsCaseFold(filenamePrefix), vfsCaseFold(filenameSuffix), true);
		//return foundVfsFilePaths;

		const heck::ci_wstring_view filenamePrefixCI(filenamePrefix);
		const heck::ci_wstring_view filenameSuffixCI(filenameSuffix);

		std::map<fs::path, fs::path, CIPathCmp> files;

		for (const fs::path& mountPhysRoot : mMountings | std::views::reverse)
		{
			if (auto optIt = getRecursiveDirectoryIteratorIfExists(mountPhysRoot / vfsPath))
			{
				for (const fs::directory_entry& dirEntry : *optIt)
				{
					const fs::path& physPath = dirEntry.path();
					const std::wstring filename = physPath.filename().wstring();
					if (heck::ci_wstring_view(filename).starts_with(filenamePrefixCI) && heck::ci_wstring_view(filename).ends_with(filenameSuffixCI))
						files.emplace(physPath.lexically_proximate(mountPhysRoot), physPath);

					if (!recursive)
						optIt->disable_recursion_pending();
				}
			}
		}

		if (vfsPathsOut)
			vfsPathsOut->assign_range(files | std::views::keys | std::views::as_rvalue);

		return { std::from_range, files | std::views::values | std::views::as_rvalue };
	}

	std::optional<fs::path> tryResolve(const fs::path& path) const
	{
		//const std::vector<VfsString> segments = vfsSegmentPath(path);
	//
	//if (const MountRelPath* const result = vfsFindFile(mInternals->mVfsRoot, segments))
	//	return mInternals->mMountings[result->fileMountPointI] / result->relPath;
	//else
	//	throw std::filesystem::filesystem_error("VFS failed to resolve path.", path, std::make_error_code(std::errc::no_such_file_or_directory));

#if IS_CASE_SENSITIVE_PLATFORM
		const std::wstring genericPath = path.generic_wstring();
		const std::vector pathSegs = genericPath | std::views::split(L'/') | std::ranges::to<std::vector>();
		if (pathSegs.empty())
			throw std::filesystem::filesystem_error("Empty path.", path, std::make_error_code(std::errc::no_such_file_or_directory));
#endif

		for (const fs::path& mountPhysRoot : mMountings | std::views::reverse)
		{
			if (fs::path trivialPhysPath = mountPhysRoot / path; fs::exists(trivialPhysPath))
				return trivialPhysPath;

#if IS_CASE_SENSITIVE_PLATFORM
			// Need to enumerate dirs ourselves to do case-insensitive file lookup.
			fs::path incrementalPhysPath = mountPhysRoot;
			bool found = true;
			for (const std::ranges::subrange segSR : pathSegs)
			{
				const std::basic_string_view seg(segSR);
				// Try the obvious first.
				if (fs::exists(incrementalPhysPath / seg))
					incrementalPhysPath /= seg;
				else
				{
					fs::path::string_type chosenName;
					for (const fs::directory_entry& dirEntry : fs::directory_iterator(incrementalPhysPath, fs::directory_options::skip_permission_denied))
					{
						const fs::path name = dirEntry.path().filename();
						if (heck::ci_wstring_view(name.wstring()) == heck::ci_wstring_view(seg))
						{
							if (!chosenName.empty())
								throw std::filesystem::filesystem_error("Ambiguous file lookup in VFS on case-sensitive platform.", path, mountPhysRoot, std::make_error_code(std::errc::no_such_file_or_directory));
							chosenName = name;
						}
					}

					if (chosenName.empty())
					{
						found = false;
						break;
					}

					incrementalPhysPath /= chosenName;
				}
			}

			if (found)
				return incrementalPhysPath;
#endif
		}
		
		return std::nullopt;
	}

	std::vector<fs::path> enumeratePhysicalDirsContainingExt(const std::wstring& ext) const
	{
		const heck::ci_wstring_view extCI(ext);

		std::unordered_set<fs::path> visitSet;
		std::vector<fs::path> list;

		for (const fs::path& mountPhysRoot : mMountings | std::views::reverse)
		{
			if (auto optIt = getRecursiveDirectoryIteratorIfExists(mountPhysRoot))
			{
				for (const fs::directory_entry& dirEntry : *optIt)
				{
					const fs::path& physPath = dirEntry.path();
					const std::wstring physExt = physPath.extension().wstring();
					if (heck::ci_wstring_view(physExt) == extCI)
						if (const fs::path& physDir = physPath.parent_path(); visitSet.insert(physDir).second)
							list.push_back(physDir.parent_path());
				}
			}
		}

		return list;
	}

#endif
};

//bool CvVFS::Mounting::isInsideMountPoint(const fs::path& path) const
//{
//	return !path.lexically_relative(mountPoint).empty();
//}
//
//std::optional<fs::path> CvVFS::Mounting::rel(const fs::path& path) const
//{
//	if (auto rel = path.lexically_relative(mountPoint); !rel.empty())
//		return rel;
//	else
//		return std::nullopt;
//}



CvVFS::CvVFS(const std::filesystem::path& cv4EngineRootDir, const std::filesystem::path& vanillaCiv4RootDir, const fs::path& optRelModPath)
	: mInternals(std::make_unique<Internals>(cv4EngineRootDir, vanillaCiv4RootDir, optRelModPath))
{
}

CvVFS::~CvVFS() noexcept = default;

fs::path CvVFS::resolve(const fs::path& path) const
{
	if (path.is_relative())
	{
		if (auto optPath = mInternals->tryResolve(path))
			return std::move(*optPath);
		throw std::filesystem::filesystem_error("Could not find file in VFS.", path, std::make_error_code(std::errc::no_such_file_or_directory));
	}
	else
		return path;
}

bool CvVFS::exists(const std::filesystem::path& path) const
{
	if (path.is_relative())
		return mInternals->tryResolve(path).has_value();
	else
		return fs::exists(path);
}

/*fs::path CvVFS::resolveAsset(const fs::path& path)
{
	if (path.is_relative())
		return resolve(path);
	else
		return path;
}*/

std::ifstream CvVFS::open(const fs::path& path, std::ios_base::openmode mode) const
{
	return std::ifstream(resolve(path), mode);
}

std::vector<fs::path> CvVFS::enumeratePhysExtRecursive(const fs::path& vfsPath, const std::wstring& filenamePrefix, const std::wstring& filenameSuffix) const
{
#if BUILD_FILE_CATALOG
	const auto vfsPathSegments = vfsSegmentPath(vfsPath);
	std::vector<fs::path> foundVfsFilePaths;
	if (const VfsDirEntriesMap* const folder = vfsFindFolder(&mInternals->mVfsRoot, vfsPathSegments))
		Internals::enumerateExt(foundVfsFilePaths, *folder, vfsPath, vfsCaseFold(filenamePrefix), vfsCaseFold(filenameSuffix), true);
	return foundVfsFilePaths;
#else
	return mInternals->enumerateFilesWithExt(vfsPath, filenamePrefix, filenameSuffix, true, nullptr);
#endif
}

std::vector<fs::path> CvVFS::enumeratePhysExtNonRecursive(const fs::path& vfsPath, const std::wstring& ext) const
{
#if BUILD_FILE_CATALOG
	const auto vfsPathSegments = vfsSegmentPath(vfsPath);
	std::vector<fs::path> foundVfsFilePaths;
	if (const VfsDirEntriesMap* const folder = vfsFindFolder(&mInternals->mVfsRoot, vfsPathSegments))
		Internals::enumerateExt(foundVfsFilePaths, *folder, vfsPath, {}, vfsCaseFold(ext), false);
	return foundVfsFilePaths;
#else
	return mInternals->enumerateFilesWithExt(vfsPath, {}, ext, false, nullptr);
#endif
}

std::vector<fs::path> CvVFS::enumeratePhysicalDirsContainingExt(const std::wstring& ext) const
{
	return mInternals->enumeratePhysicalDirsContainingExt(ext);
}


std::optional<std::string> CvVFS::loadPythonCodeIfExists(const std::string& filename, std::filesystem::path& vfsPathOut) const
{
	if (const auto it = mInternals->mPythonModuleLookup.find(toLower(filename)); it != mInternals->mPythonModuleLookup.end())
	{
		std::stringstream ss;
		// Important encoding override for BUG mod files.
		ss << "# coding=cp1252\n";
#if BUILD_FILE_CATALOG
		const fs::path physPath = mInternals->mMountings[it->second->fileMountPointI] / it->second->relPath;
#else
		const fs::path& physPath = it->second.physPath;
#endif
		std::ifstream file(physPath);
		ss << file.rdbuf();
		if (!file)
			throw fs::filesystem_error("Failed to read python module, but the file was listed in the VFS.", physPath, std::error_code());
#if BUILD_FILE_CATALOG
		vfsPathOut = it->second->relPath; // This is also the VFS path as mounts always mount at the root.
#else
		vfsPathOut = it->second.vfsPath;
#endif
		return std::move(ss).str();
	}
	else
		return std::nullopt;
}

const std::wstring& CvVFS::getModName(bool fullPath) const
{
	return fullPath ? mInternals->mModFullPath : mInternals->mModName;
}

const std::wstring& CvVFS::getModRelPathString() const
{
	return mInternals->mModRelPath;
}