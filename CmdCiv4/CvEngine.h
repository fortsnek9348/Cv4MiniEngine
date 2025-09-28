#pragma once

#include "MyFFile.h"

#include <CvEnums.h>

#include <CommonStuff/vec.h>

#include <unordered_map>

class CvEngine
{
public:
	static CvEngine& getInstance();

	void reset();

	void AutoSave(bool bInitial);

	void setSignText(PlayerTypes playerI, heck::ivec2 coord, std::wstring text);
	std::wstring_view findSignTextAt(PlayerTypes playerI, heck::ivec2 coord) const;

	void serialise(FFile<StdRawBinaryStream>& file) const;
	void deserialise(FFile<StdRawBinaryStream>& file);
	
private:
	int mAutoSaveCounter = 0;

	struct SignKey
	{
		PlayerTypes playerI{};
		heck::ivec2 coord{};

		friend bool operator==(SignKey, SignKey) = default;
		bool operator<(SignKey) const;
	};

	struct Hasher
	{
		static size_t operator()(SignKey) noexcept;
	};

	std::unordered_map<SignKey, std::wstring, Hasher> mSigns;
};