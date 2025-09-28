#include "CvVFS.h"
#include "Common.h"

#include <CvString.h>

#include <CommonStuff/Hashing.h>

#ifndef _WIN32
#include <CommonStuff/StringConversion.h>
#endif

#include <algorithm>
#include <ranges>
#include <fstream>
#include <unordered_set>
#include <cwctype>
#include <memory>

namespace fs = std::filesystem;

constinit const CvVFS* ::gVFS = nullptr;

namespace
{
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
		return VfsString(std::from_range, s | std::views::transform([](wchar_t c) { return static_cast<wchar_t>(towupper(c)); }));
#else
		// Don't have string range construct yet.
		// And we need to convert UTF-32 to UTF-16.
		std::wstring caseFoldedWideStr(s);
		for (wchar_t& c : caseFoldedWideStr)
			c = static_cast<wchar_t>(towupper(c));
		return heck::toUtf16(caseFoldedWideStr);
#endif
	}

	VfsDirEntriesMap enumeratePhysical(const fs::path& mountRoot, const fs::path& mountRelPath, size_t fileMountPointI)
	{
		VfsDirEntriesMap map;
		for (const fs::directory_entry& dirEntry : fs::directory_iterator(mountRoot / mountRelPath, fs::directory_options::skip_permission_denied))
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
}

struct CvVFS::Internals
{
	explicit Internals(const std::filesystem::path& cv4EngineRootDir, const std::filesystem::path& vanillaCiv4RootDir, const std::wstring& optRelModName)
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

		if (!optRelModName.empty())
		{
			mMountings.insert(mMountings.end(), {
				btsRoot / "Mods" / optRelModName / "Assets"      /**/,
				btsRoot / "Mods" / optRelModName / kPublicMapsDirName  /**/,
				btsRoot / "Mods" / optRelModName / "PrivateMaps" /**/,
			});
		}

		// Cv4MiniEngine overrides.
		mMountings.insert(mMountings.end(), {
			cv4EngineRootDir / kCustomAssetsDirName /**/,
			cv4EngineRootDir / kPublicMapsDirName         /**/,
		});

		for (size_t i = 0; i < mMountings.size(); ++i)
		{
			VfsDirEntriesMap mountTree = enumeratePhysical(mMountings[i], fs::path(), i);
			vfsMerge(mVfsRoot, mountTree, mMountings[i]);
		}

		mPythonModuleLookup.reserve(200);
		buildPythonModuleLookup(mVfsRoot);

		mModName = CvString(optRelModName);
		if (!optRelModName.empty())
		{
			mModFullPath = (btsRoot / "Mods" / optRelModName).string();
			mModRelPath = (fs::path("Mods") / optRelModName).string() + '\\';
			std::ranges::replace(mModRelPath, '/', '\\'); // Used for save files, we need a specific format.
		}
	}

	std::vector<fs::path> mMountings;
	VfsDirEntriesMap mVfsRoot;

	std::unordered_map<std::string, const MountRelPath*> mPythonModuleLookup;

	void buildPythonModuleLookup(const VfsDirEntriesMap& vfsFolder)
	{
		static const VfsString kExt = vfsCaseFold(L".py");

		for (const auto& [filename, dirEntry] : vfsFolder)
		{
			if (dirEntry.folderDirEntries)
				buildPythonModuleLookup(*dirEntry.folderDirEntries);
			
			if (dirEntry.fileMountRelPath && filename.ends_with(kExt))
				mPythonModuleLookup.emplace(dirEntry.fileMountRelPath.relPath.stem().string(), &dirEntry.fileMountRelPath);
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

	std::string mModName;
	std::string mModFullPath;
	std::string mModRelPath;

	//template<class Iterator>
	//std::vector<std::filesystem::path> enumerateExt(const std::filesystem::path& path, const std::filesystem::path& ext, bool phys) const;

	static void enumerateExt(std::vector<fs::path>& foundVfsFilePathsOut, const VfsDirEntriesMap& vfsFolder, const fs::path& vfsPath, const VfsString& filenamePrefix, const VfsString& filenameSuffix, bool recursive)
	{
		//return enumerateExt<fs::directory_iterator>(path, ext, false);

		for (const auto& [name, dirEntry] : vfsFolder)
		{
			if (dirEntry.folderDirEntries)
			{
				if (recursive)
					enumerateExt(foundVfsFilePathsOut, *dirEntry.folderDirEntries, vfsPath, filenamePrefix, filenameSuffix, recursive);
			}
			
			if (dirEntry.fileMountRelPath && name.starts_with(filenamePrefix) && name.ends_with(filenameSuffix))
				foundVfsFilePathsOut.push_back(vfsPath / dirEntry.fileMountRelPath.relPath.filename()); // USe the filename with the original casing, for original map script names.
		}
	}

	void enumeratePhysicalDirsContainingExt(std::unordered_set<MountRelPath, MountRelPathHash>& physDirPathsSet, std::vector<fs::path>& physDirPathsList, const VfsDirEntriesMap& vfsFolder, const VfsString& ext) const
	{
		//return enumerateExt<fs::directory_iterator>(path, ext, false);

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



CvVFS::CvVFS(const std::filesystem::path& cv4EngineRootDir, const std::filesystem::path& vanillaCiv4RootDir, const std::wstring& optRelModName)
	: mInternals(std::make_unique<Internals>(cv4EngineRootDir, vanillaCiv4RootDir, optRelModName))
{
}

CvVFS::~CvVFS() noexcept = default;

fs::path CvVFS::resolve(const fs::path& path) const
{
	const std::vector<VfsString> segments = vfsSegmentPath(path);

	if (const MountRelPath* const result = vfsFindFile(mInternals->mVfsRoot, segments))
		return mInternals->mMountings[result->fileMountPointI] / result->relPath;
	else
		throw std::filesystem::filesystem_error("VFS failed to resolve path.", path, std::make_error_code(std::errc::no_such_file_or_directory));


	//if (path.is_relative())
	//{
	//	for (const Mounting& mounting : mMountings | std::views::reverse)
	//		if (const std::optional<fs::path> optRel = mounting.rel(path))
	//			if (fs::path absPath = mounting.physPath / *optRel; fs::exists(absPath))
	//				return absPath;
	//	throw std::runtime_error("Failed to resolve path.");
	//}
	//
	//return path;
}

bool CvVFS::exists(const std::filesystem::path& path) const
{
	const std::vector<VfsString> segments = vfsSegmentPath(path);
	return !!vfsFindFile(mInternals->mVfsRoot, segments);

	//if (path.is_relative())
	//{
	//	for (const Mounting& mounting : mMountings | std::views::reverse)
	//		if (const std::optional<fs::path> optRel = mounting.rel(path))
	//			if (fs::path absPath = mounting.physPath / *optRel; fs::exists(absPath))
	//				return true;
	//	return false;
	//}
	//
	//return fs::exists(path);
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

std::vector<fs::path> CvVFS::enumerateExtRecursive(const fs::path& vfsPath, const std::wstring& filenamePrefix, const std::wstring& filenameSuffix) const
{
	//return enumerateExt<fs::recursive_directory_iterator>(path, ext, false);
	
	const auto vfsPathSegments = vfsSegmentPath(vfsPath);
	std::vector<fs::path> foundVfsFilePaths;
	if (const VfsDirEntriesMap* const folder = vfsFindFolder(&mInternals->mVfsRoot, vfsPathSegments))
		Internals::enumerateExt(foundVfsFilePaths, *folder, vfsPath, vfsCaseFold(filenamePrefix), vfsCaseFold(filenameSuffix), true);
	return foundVfsFilePaths;
}

std::vector<fs::path> CvVFS::enumerateExtNonRecursive(const fs::path& vfsPath, const std::wstring& ext) const
{
	//return enumerateExt<fs::directory_iterator>(path, ext, false);

	const auto vfsPathSegments = vfsSegmentPath(vfsPath);
	std::vector<fs::path> foundVfsFilePaths;
	if (const VfsDirEntriesMap* const folder = vfsFindFolder(&mInternals->mVfsRoot, vfsPathSegments))
		Internals::enumerateExt(foundVfsFilePaths, *folder, vfsPath, {}, vfsCaseFold(ext), false);
	return foundVfsFilePaths;
}

std::vector<fs::path> CvVFS::enumeratePhysicalDirsContainingExt(const std::wstring& ext) const
{
	std::unordered_set<MountRelPath, MountRelPathHash> set;
	std::vector<fs::path> list;
	mInternals->enumeratePhysicalDirsContainingExt(set, list, mInternals->mVfsRoot, vfsCaseFold(ext));
	return list;
}


std::optional<std::string> CvVFS::loadPythonCodeIfExists(const std::string& filename, std::filesystem::path& vfsPathOut) const
{
	if (const auto it = mInternals->mPythonModuleLookup.find(filename); it != mInternals->mPythonModuleLookup.end())
	{
		std::stringstream ss;
		// Important encoding override for BUG mod files.
		ss << "# coding=cp1252\n";
		const fs::path physPath = mInternals->mMountings[it->second->fileMountPointI] / it->second->relPath;
		std::ifstream file(physPath);
		ss << file.rdbuf();
		if (!file)
			throw fs::filesystem_error("Failed to read python module, but the file was listed in the VFS.", physPath, std::error_code());
		vfsPathOut = it->second->relPath; // This is also the VFS path as mounts always mount at the root.
		return std::move(ss).str();
	}
	else
		return std::nullopt;
}

const char* CvVFS::getModName(bool fullPath) const
{
	return (fullPath ? mInternals->mModFullPath : mInternals->mModName).c_str();
}

std::string CvVFS::getModRelPathString() const
{
	return mInternals->mModRelPath;
}