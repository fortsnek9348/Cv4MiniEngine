#pragma once

#include <array>
#include <fstream>
#include <span>
#include <vector>
#include <cassert>
#include <filesystem>

#include <zlib.h>

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

// NOTE: Civ4 compressed streams spit out a number of length-prefixed compressed blocks.
//       During decompression, just read a block whenever data is needed. Once ths stream ends, that will also be the end of the last block.
//       And to maintain byte-exact output, the block size must also be the same.

inline constexpr size_t kCiv4BlockSize = 64 * 1024;

struct CompressionBinaryStream : IRawBinaryStream
{
	explicit CompressionBinaryStream(IRawBinaryStream* baseStream);
	CompressionBinaryStream(const CompressionBinaryStream&) = delete;
	CompressionBinaryStream& operator=(const CompressionBinaryStream&) = delete;
	CompressionBinaryStream(CompressionBinaryStream&& b) noexcept = delete;
	CompressionBinaryStream& operator=(CompressionBinaryStream&& b) = delete;
	virtual ~CompressionBinaryStream() override;


	virtual void read(std::span<std::byte>) override;

	virtual void write(std::span<const std::byte> blob) override;

	void flushCompression(bool finish);

	void flushOutputBlock();

	void compress(std::span<const std::byte> blob, bool finish);

	IRawBinaryStream* baseStream = nullptr;
	std::unique_ptr<std::byte[]> inBuffer = std::make_unique_for_overwrite<std::byte[]>(kCiv4BlockSize);
	std::unique_ptr<std::byte[]> outBuffer = std::make_unique_for_overwrite<std::byte[]>(kCiv4BlockSize);
	size_t bufferInPosition = 0;
	size_t bufferOutPosition = 0;
	z_stream zlibState{};
};

struct DecompressionBinaryStream : IRawBinaryStream
{
	explicit DecompressionBinaryStream(IRawBinaryStream* baseStream);
	DecompressionBinaryStream(const DecompressionBinaryStream&) = delete;
	DecompressionBinaryStream& operator=(const DecompressionBinaryStream&) = delete;
	DecompressionBinaryStream(DecompressionBinaryStream&& b) = delete;
	DecompressionBinaryStream& operator=(DecompressionBinaryStream&& b) = delete;
	virtual ~DecompressionBinaryStream() override;

	std::vector<std::byte> readUntilEnd();

	virtual void read(std::span<std::byte> blob) override;

	virtual void write(std::span<const std::byte> blob) override;

	// For some reason, I'm not getting Z_STREAM_END.
	void assertStreamEnd();

	void refill();


	IRawBinaryStream* baseStream = nullptr;
	std::unique_ptr<std::byte[]> inBuffer = std::make_unique_for_overwrite<std::byte[]>(kCiv4BlockSize);
	std::unique_ptr<std::byte[]> outBuffer = std::make_unique_for_overwrite<std::byte[]>(kCiv4BlockSize);
	size_t bufferInPosition = 0;
	size_t bufferInEnd = 0;
	size_t bufferOutPosition = 0;
	size_t bufferOutEnd = 0;
	z_stream zlibState{};
	int ret = Z_OK;
};