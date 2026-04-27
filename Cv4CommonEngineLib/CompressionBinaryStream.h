#pragma once

#include "inc/Cv4CommonEngineLib/RawBinaryStream.h"

#include <zlib.h>

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