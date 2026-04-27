#pragma once

#include "RawBinaryStream.h"

#include <CvGameCoreDLL/FDataStreamBase.h>

#include <CommonStuff/StringConversion.h>

#include <filesystem>
#include <fstream>
#include <span>
#include <string.h>


template<class RawStream>
class FFile : public FDataStreamBase
{
public:

	template<class... Args>
	explicit FFile(Args&&... args) : stream(std::forward<Args>(args)...)
	{
	}

	void WriteString(const wchar_t* szName) override
	{
#ifdef _WIN32
		const size_t length = szName ? wcslen(szName) : 0;
		Write(uint32_t(length));
		stream.writeTrivialArray(std::span(szName, length));
#else
		const std::u16string u16str = szName ? heck::toUtf16(szName) : std::u16string();
		Write(uint32_t(u16str.size()));
		stream.writeTrivialArray(std::span(u16str));
#endif
	}

	void WriteString(const char* szName) override
	{
		const size_t length = szName ? strlen(szName) : 0;
		Write(uint32_t(length));
		stream.writeTrivialArray(std::span(szName, length));
	}

	void WriteString(const std::string& szName) override
	{
		Write(uint32_t(szName.size()));
		stream.writeTrivialArray(std::span(szName));
	}

	void WriteString(const std::wstring& szName) override
	{
#ifdef _WIN32
		Write(uint32_t(szName.size()));
		stream.writeTrivialArray(std::span(szName));
#else
		const std::u16string u16str = heck::toUtf16(szName);
		Write(uint32_t(u16str.size()));
		stream.writeTrivialArray(std::span(u16str));
#endif
	}

	void WriteString(int count, std::string values[]) override
	{
		for (const auto& s : std::span(values, count))
			WriteString(s);
	}

	void WriteString(int count, std::wstring values[]) override
	{
		for (const auto& s : std::span(values, count))
			WriteString(s);
	}

	void ReadString(char* szName) override
	{
		unsigned int length = 0;
		Read(&length);
		stream.readTrivialArray(std::span(szName, length));
		szName[length] = {};
	}

	void ReadString(wchar_t* szName) override
	{
		unsigned int length = 0;
		Read(&length);
#ifdef _WIN32
		stream.readTrivialArray(std::span(szName, length));
#else
		std::unique_ptr<char16_t[]> raw = std::make_unique_for_overwrite<char16_t[]>(length);
		stream.readTrivialArray(std::span(raw.get(), length));
		const std::wstring wstr = heck::toWide(std::u16string_view(raw.get(), length));
		std::ranges::copy(wstr, std::span(raw.get(), length).begin());
#endif
		szName[length] = {};
	}

	void ReadString(std::string& szName) override
	{
		unsigned int length = 0;
		Read(&length);
		szName.resize_and_overwrite(length, [&](char* ptr, size_t n) {
			stream.readTrivialArray(std::span(ptr, n));
			return n;
		});
	}

	void ReadString(std::wstring& szName) override
	{
		unsigned int length = 0;
		Read(&length);
#ifdef _WIN32
		szName.resize_and_overwrite(length, [&](wchar_t* ptr, size_t n) {
			stream.readTrivialArray(std::span(ptr, n));
			return n;
			});
#else
		std::unique_ptr<char16_t[]> raw = std::make_unique_for_overwrite<char16_t[]>(length);
		stream.readTrivialArray(std::span(raw.get(), length));
		szName = heck::toWide(std::u16string_view(raw.get(), length));
#endif
	}

	void ReadString(int count, std::string values[]) override
	{
		for (auto& s : std::span(values, count))
			ReadString(s);
	}

	void ReadString(int count, std::wstring values[]) override
	{
		for (auto& s : std::span(values, count))
			ReadString(s);
	}

	char* ReadString() override
	{
		unsigned int length = 0;
		Read(&length);
		auto ptr = std::make_unique<char[]>(length + 1);
		stream.readTrivialArray(std::span(ptr.get(), length));
		return ptr.release();
	}

	wchar_t* ReadWideString() override
	{
		unsigned int length = 0;
		Read(&length);
#ifdef _WIN32
		auto ptr = std::make_unique<wchar_t[]>(length + 1);
		stream.readTrivialArray(std::span(ptr.get(), length));
#else
		std::unique_ptr<char16_t[]> raw = std::make_unique_for_overwrite<char16_t[]>(length);
		stream.readTrivialArray(std::span(raw.get(), length));
		const std::wstring wstr = heck::toWide(std::u16string_view(raw.get(), length));
		auto ptr = std::make_unique<wchar_t[]>(wstr.size() + 1);
		std::ranges::copy(wstr, std::span(ptr.get(), wstr.size()).begin());
		ptr[wstr.size()] = L'\0';
#endif
		return ptr.release();
	}

	void Read(char* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(uint8_t* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(int count, char values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Read(int count, uint8_t values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Read(bool* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(int count, bool values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Read(short* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(unsigned short* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(int count, short values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Read(int count, unsigned short values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Read(int* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(unsigned int* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(int count, int values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Read(int count, unsigned int values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	/*
	void Read(long* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(unsigned long* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(int count, long values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Read(int count, unsigned long values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}*/

	void Read(float* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(int count, float values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Read(double* value) override
	{
		stream.readTrivial(*value);
	}

	void Read(int count, double values[]) override
	{
		stream.readTrivialArray(std::span(values, count));
	}

	void Write(char value) override
	{
		stream.writeTrivial(value);
	}

	void Write(uint8_t value) override
	{
		stream.writeTrivial(value);
	}

	void Write(int count, const char values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	void Write(int count, const uint8_t values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	void Write(bool value) override
	{
		stream.writeTrivial(value);
	}

	void Write(int count, const bool values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	void Write(short value) override
	{
		stream.writeTrivial(value);
	}

	void Write(unsigned short value) override
	{
		stream.writeTrivial(value);
	}

	void Write(int count, const short values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	void Write(int count, const unsigned short values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	void Write(int value) override
	{
		stream.writeTrivial(value);
	}

	void Write(unsigned int value) override
	{
		stream.writeTrivial(value);
	}

	void Write(int count, const int values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	void Write(int count, const unsigned int values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	/*
	void Write(long value) override
	{
		stream.writeTrivial(value);
	}

	void Write(unsigned long value) override
	{
		stream.writeTrivial(value);
	}

	void Write(int count, const long values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	void Write(int count, const unsigned long values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}*/

	void Write(float value) override
	{
		stream.writeTrivial(value);
	}

	void Write(int count, const float values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}

	void Write(double value) override
	{
		stream.writeTrivial(value);
	}

	void Write(int count, const double values[]) override
	{
		stream.writeTrivialArray(std::span(values, count));
	}


	RawStream stream;
};