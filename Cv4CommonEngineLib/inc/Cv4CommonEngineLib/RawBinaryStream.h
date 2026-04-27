#pragma once

#include <array>
#include <fstream>
#include <span>
#include <vector>
#include <cassert>
#include <filesystem>

struct IRawBinaryStream
{
	virtual void read(std::span<std::byte>) = 0;
	virtual void write(std::span<const std::byte>) = 0;
	virtual ~IRawBinaryStream() noexcept(false) = default;

	template<class T>
		requires std::is_trivially_copyable_v<T>
	void writeTrivial(const T& x)
	{
		write(std::as_bytes(std::span(std::addressof(x), 1)));
	}

	template<class T>
		requires std::is_trivially_copyable_v<T>
	void writeTrivialArray(std::span<T> x)
	{
		write(std::as_bytes(x));
	}

	template<class T>
		requires std::is_trivially_copyable_v<T>
	void readTrivial(T& x)
	{
		read(std::as_writable_bytes(std::span(std::addressof(x), 1)));
	}

	template<class T>
		requires std::is_trivially_copyable_v<T>
	void readTrivialArray(std::span<T> x)
	{
		read(std::as_writable_bytes(x));
	}
};

struct StdRawBinaryStream : IRawBinaryStream
{
	explicit StdRawBinaryStream(const std::filesystem::path& path, std::ios_base::openmode mode) : stream(path, mode | std::ios::binary)
	{
		//if (!stream && (mode & std::ios::out) != 0)
		//{
		//	// Create directories, try again.
		//	std::error_code errc{};
		//	(void)std::filesystem::create_directories(path.parent_path(), errc);
		//	stream.open(path, mode | std::ios::binary);
		//}
	}

	virtual void read(std::span<std::byte> blob) override
	{
		if (!stream.read(reinterpret_cast<char*>(blob.data()), blob.size()))
			throw std::runtime_error("Failed to read file (eof?).");
	}
	virtual void write(std::span<const std::byte> blob) override
	{
		if (!stream.write(reinterpret_cast<const char*>(blob.data()), blob.size()))
			throw std::runtime_error("Failed to write file.");
	}

	std::fstream stream;
};

struct MemoryWriteRawBinaryStream : IRawBinaryStream
{
	virtual void read(std::span<std::byte>) override
	{
		std::abort();
	}
	virtual void write(std::span<const std::byte> src) override
	{
		blob.append_range(src);
	}

	std::vector<std::byte> blob;
};

struct MemoryReadRawBinaryStream : IRawBinaryStream
{
	explicit MemoryReadRawBinaryStream(std::span<const std::byte> blob) noexcept : blob(blob)
	{
	}

	virtual void read(std::span<std::byte> dst) override
	{
		if (dst.size() > blob.size())
			std::abort();
		std::copy_n(blob.begin(), dst.size(), dst.begin());
		blob = blob.subspan(dst.size());
	}
	virtual void write([[maybe_unused]] std::span<const std::byte> src) override
	{
		std::abort();
	}

	std::span<const std::byte> blob;
};

