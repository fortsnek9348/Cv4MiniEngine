#include "IniReader.h"

#include <CvString.h>
#include <CvGlobals.h>

#include <algorithm>
#include <fstream>

//static void toAsciiLowerInplace(std::string& s)
//{
//	for (char& c : s)
//		if ('A' <= c && c <= 'Z')
//			c = c - 'A' + 'a';
//}

static char toAsciiLower(char c)
{
	if ('A' <= c && c <= 'Z')
		c = c - 'A' + 'a';
	return c;
}


size_t IniData::CICmp::operator()(std::string_view a, std::string_view b) noexcept
{
	return std::ranges::lexicographical_compare(a, b, std::less<>(), toAsciiLower, toAsciiLower);
}

static bool isSpace(char c)
{
	return (bool)std::isspace(static_cast<unsigned char>(c));
}

static void trim(std::string& s)
{
	while (s.size() && isSpace(s.back()))
		s.pop_back();
	s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), isSpace));
}

IniData::Section& IniData::grabSection(const IniDocKey& section)
{
	const auto mapIt = mSectionLookup.find(section.name);
	if (mapIt == mSectionLookup.end())
	{
		Section newSection{ .name{ section.name } };
		if (section.doc.size())
			newSection.comments.emplace_back(section.doc);
		const auto listIt = mSections.insert(mSections.end(), std::move(newSection));
		mSectionLookup.emplace(listIt->name, listIt);
		return *listIt;
	}
	else
		return *mapIt->second;
}

IniData::Section::Prop& IniData::Section::grabProp(const IniDocKey& propInfo)
{
	const auto mapIt = propLookup.find(propInfo.name);
	if (mapIt == propLookup.end())
	{
		const auto listIt = props.insert(props.end(), Prop{ .comments{ std::string(propInfo.doc) }, .name = std::string(propInfo.name) });
		propLookup.emplace(listIt->name, listIt);
		return *listIt;
	}
	else
		return *mapIt->second;
}

void IniData::set(const IniDocKey& section, const IniDocKey& key, std::string value)
{
	auto& stored = grabSection(section).grabProp(key).value;
	if (stored != value)
	{
		stored  = std::move(value);
		mChanged = true;
	}
}

void IniData::set(const IniDocKey& section, const IniDocKey& key, std::wstring value)
{
	set(section, key, CvString(CvWString(std::move(value))));
}

void IniData::setDefault(const IniDocKey& sectionInfo, const IniDocKey& key, std::string value)
{
	Section& section = grabSection(sectionInfo);
	if (!section.propLookup.contains(key.name))
	{
		const auto it = section.props.insert(section.props.end(), Section::Prop{ { std::string(key.doc) }, std::string(key.name), value });
		section.propLookup.emplace(it->name, it);
		mChanged = true;
	}
}

void IniData::setDefault(const IniDocKey& sectionInfo, const IniDocKey& key, std::wstring value)
{
	Section& section = grabSection(sectionInfo);
	if (!section.propLookup.contains(key.name))
	{
		const auto it = section.props.insert(section.props.end(), Section::Prop{ { std::string(key.doc) }, std::string(key.name), CvString(value) });
		section.propLookup.emplace(it->name, it);
		mChanged = true;
	}
}

const std::string& IniData::get(const IniDocKey& section, const IniDocKey& key, const std::string& def) const
{
	if (const auto sectionIt = mSectionLookup.find(section.name); sectionIt != mSectionLookup.end())
		if (const auto keyIt = sectionIt->second->propLookup.find(key.name); keyIt != sectionIt->second->propLookup.end())
			return keyIt->second->value;

	return def;
}

std::wstring IniData::get(const IniDocKey& section, const IniDocKey& key, const std::wstring& def) const
{
	if (const auto sectionIt = mSectionLookup.find(section.name); sectionIt != mSectionLookup.end())
		if (const auto keyIt = sectionIt->second->propLookup.find(key.name); keyIt != sectionIt->second->propLookup.end())
			// TODO: What is the encoding of the INI when it contains non-ASCII?
			return CvWString(keyIt->second->value);

	return def;
}

int IniData::get(const IniDocKey& section, const IniDocKey& key, int def) const
{
	std::string s = get(section, key, std::to_string(def));
	trim(s);
	size_t len = 0;
	try
	{
		const int value = std::stoi(s, &len);
		if (len != s.size())
			throw std::logic_error("Junk at end of string.");
		return value;
	}
	catch (const std::logic_error& ex)
	{
		throw std::runtime_error("Failed to parse INI value at " + std::string(section.name) + '/' + std::string(key.name) + " as int: " + ex.what());
	}
}

unsigned int IniData::getUnsigned(const IniDocKey& section, const IniDocKey& key, unsigned int def) const
{
	std::string s = get(section, key, std::to_string(def));
	trim(s);
	size_t len = 0;
	try
	{
		const unsigned long value = std::stoul(s, &len);
		if (len != s.size())
			throw std::logic_error("Junk at end of string.");
		if constexpr (UINT_MAX < ULONG_MAX)
			if (value > UINT_MAX)
				throw std::out_of_range("Out of range.");
		return value;
	}
	catch (const std::logic_error& ex)
	{
		throw std::runtime_error("Failed to parse INI value at " + std::string(section.name) + '/' + std::string(key.name) + " as int: " + ex.what());
	}
}

int IniData::getEnum(const IniDocKey& section, const IniDocKey& key, int num, const std::string& def) const
{
	if (const auto sectionIt = mSectionLookup.find(section.name); sectionIt != mSectionLookup.end())
		if (const auto keyIt = sectionIt->second->propLookup.find(key.name); keyIt != sectionIt->second->propLookup.end())
		{
			const int value = GC.getInfoTypeForString(keyIt->second->value.c_str());
			if (0 <= value && value < num)
				return value;
		}
	return std::max(0, GC.getInfoTypeForString(def.c_str()));
}

int IniData::getEnum(const IniDocKey& section, const IniDocKey& key, int num, int def) const
{
	if (const auto sectionIt = mSectionLookup.find(section.name); sectionIt != mSectionLookup.end())
		if (const auto keyIt = sectionIt->second->propLookup.find(key.name); keyIt != sectionIt->second->propLookup.end())
		{
			const int value = GC.getInfoTypeForString(keyIt->second->value.c_str());
			if (0 <= value && value < num)
				return value;
		}
	return def;
}

const std::string& IniData::grab(const IniDocKey& section, const IniDocKey& key, const std::string& def)
{
	setDefault(section, key, def);
	return get(section, key, def);
}
std::wstring IniData::grab(const IniDocKey& section, const IniDocKey& key, const std::wstring& def)
{
	setDefault(section, key, def);
	return get(section, key, def);
}
int IniData::grab(const IniDocKey& section, const IniDocKey& key, int def)
{
	setDefault(section, key, std::to_string(def));
	return get(section, key, def);
}

bool IniData::isChanged() const
{
	return mChanged;
}
void IniData::clearChanged()
{
	mChanged = false;
}

void IniData::createIfNew(const std::filesystem::path& path)
{
	(void)std::ofstream(path, std::ios::noreplace);
}

IniData (::loadINI)(const std::filesystem::path& path)
{
	std::ifstream file(path);

	if (!file)
		throw std::runtime_error("Failed to open INI file.");

	std::string lineBuf;

	IniData iniData;

	IniData::Section* currentSection = nullptr;

	std::vector<std::string> comments;

	while (std::getline(file, lineBuf))
	{
		trim(lineBuf);

		if (lineBuf.empty())
			continue;

		switch (lineBuf.front())
		{
		case ';':
		{
			std::string value = lineBuf.substr(1);
			trim(value);
			comments.push_back(std::move(value));
			break;
		}
		case '[':
		{
			if (lineBuf.back() != ']')
				throw std::runtime_error("Invalid section syntax.");

			const auto it = iniData.mSections.emplace(iniData.mSections.end());
			currentSection = &*it;
			currentSection->comments = std::move(comments);
			currentSection->name = lineBuf.substr(1, lineBuf.size() - 2);
			iniData.mSectionLookup.emplace(currentSection->name, it);
			break;
		}
		default:
		{
			if (!currentSection)
				throw std::runtime_error("Invalid INI syntax.");

			const size_t eqOffset = lineBuf.find('=');
			if (eqOffset >= lineBuf.size())
				throw std::runtime_error("Invalid key=value syntax.");
			std::string key = lineBuf.substr(0, eqOffset);
			std::string value = lineBuf.substr(eqOffset + 1);
			trim(key);
			trim(value);
			const auto it = currentSection->props.emplace(currentSection->props.end(), std::move(comments), std::move(key), std::move(value));
			currentSection->propLookup.emplace(it->name, it);
			break;
		}
		}
	}

	if (!file.eof())
		throw std::runtime_error("Did not reach EOF.");

	iniData.mTrailingComments = std::move(comments);

	return iniData;
}

void ::saveINI(const std::filesystem::path& path, const IniData& ini)
{
	std::filesystem::path tempPath = path;
	tempPath += ".tmp";

	{
		std::ofstream file(tempPath);

		for (const IniData::Section& section : ini.mSections)
		{
			file << '\n';
			for (const std::string_view line : section.comments)
				file << "; " << line << '\n';
			file << '[' << section.name << "]\n";
			for (const IniData::Section::Prop& prop : section.props)
			{
				file << '\n';
				for (const std::string_view line : prop.comments)
					file << "; " << line << '\n';
				file << prop.name << " = " << prop.value << '\n';
			}
		}

		file << '\n';
		for (const std::string_view line : ini.mTrailingComments)
			file << line << '\n';
	}

	// Backup original, then replace original, then delete temp.
	std::filesystem::path backupPath = path;
	backupPath += ".bak";
	std::filesystem::copy_file(path, backupPath, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::rename(tempPath, path);
	std::filesystem::remove(tempPath);
}