#pragma once

#include <map>
#include <list>
#include <string>
#include <filesystem>
#include <vector>

namespace cvengine
{
	struct IniDocKey
	{
		std::string_view name;
		std::string_view doc{};
	};

	class IniData
	{
	public:
		// NOTE: Strings are now UTF-8.

		void set(const IniDocKey& section, const IniDocKey& key, std::string value);
		void set(const IniDocKey& section, const IniDocKey& key, std::wstring value);

		void setDefault(const IniDocKey& section, const IniDocKey& key, std::string value);
		void setDefault(const IniDocKey& section, const IniDocKey& key, std::wstring value);

		const std::string& get(const IniDocKey& section, const IniDocKey& key, const std::string& def) const;
		std::wstring get(const IniDocKey& section, const IniDocKey& key, const std::wstring& def) const;
		int get(const IniDocKey& section, const IniDocKey& key, int def) const;
		unsigned int getUnsigned(const IniDocKey& section, const IniDocKey& key, unsigned int def) const;
		int getEnum(const IniDocKey& section, const IniDocKey& key, int num, const std::string& def) const;
		int getEnum(const IniDocKey& section, const IniDocKey& key, int num, int def) const;

		const std::string& grab(const IniDocKey& section, const IniDocKey& key, const std::string& def);
		std::wstring grab(const IniDocKey& section, const IniDocKey& key, const std::wstring& def);
		int grab(const IniDocKey& section, const IniDocKey& key, int def);

		bool isChanged() const;
		void clearChanged();

		static void createIfNew(const std::filesystem::path& path);
		friend IniData loadINI(const std::filesystem::path& path);
		friend void saveINI(const std::filesystem::path& path, const IniData& ini);

	private:
		bool mChanged = false;

		struct CICmp
		{
			static size_t operator()(std::string_view, std::string_view) noexcept;
		};

		struct Section
		{
			std::vector<std::string> comments{};
			std::string name;

			struct Prop
			{
				std::vector<std::string> comments;
				std::string name;
				std::string value{};
			};

			std::list<Prop> props{};
			std::map<std::string_view, std::list<Prop>::iterator, CICmp> propLookup{};

			Prop& grabProp(const IniDocKey& propInfo);
		};

		std::list<Section> mSections;
		std::map<std::string_view, std::list<Section>::iterator, CICmp> mSectionLookup;

		std::vector<std::string> mTrailingComments;

		Section& grabSection(const IniDocKey& section);
	};

	IniData loadINI(const std::filesystem::path& path);
	void saveINI(const std::filesystem::path& path, const IniData& ini);
}