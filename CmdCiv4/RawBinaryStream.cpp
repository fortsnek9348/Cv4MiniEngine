#include "RawBinaryStream.h"

CompressionBinaryStream::CompressionBinaryStream(IRawBinaryStream* baseStream) : baseStream(baseStream)
{
	if (deflateInit(&zlibState, Z_DEFAULT_COMPRESSION) != Z_OK)
		throw std::runtime_error("deflateInit failed.");
}
CompressionBinaryStream::~CompressionBinaryStream()
{
	//flushCompression(true);
	(void)deflateEnd(&zlibState);
}


void CompressionBinaryStream::read(std::span<std::byte>)
{
	std::abort();
}

void CompressionBinaryStream::write(std::span<const std::byte> blob)
{
	if (blob.size() > kCiv4BlockSize - bufferInPosition) [[unlikely]]
	{
		flushCompression(false);
		if (blob.size() >= kCiv4BlockSize)
		{
			compress(blob, false);
			return;
		}
	}

	std::ranges::copy(blob, std::span(inBuffer.get(), kCiv4BlockSize).subspan(bufferInPosition).begin());
	bufferInPosition += blob.size();
}

void CompressionBinaryStream::flushCompression(bool finish)
{
	compress(std::span(inBuffer.get(), bufferInPosition), finish);
	bufferInPosition = 0;
}

void CompressionBinaryStream::flushOutputBlock()
{
	baseStream->writeTrivial((uint32_t)bufferOutPosition);
	baseStream->write(std::span(outBuffer.get(), kCiv4BlockSize).subspan(0, bufferOutPosition));
	bufferOutPosition = 0;
}

void CompressionBinaryStream::compress(std::span<const std::byte> blob, bool finish)
{
	zlibState.avail_in = (uint32_t)blob.size();
	// Needs to be non-const for old zlib.
	zlibState.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(blob.data()));

	do
	{
		zlibState.avail_out = (uint32_t)(kCiv4BlockSize - bufferOutPosition);
		zlibState.next_out = reinterpret_cast<Bytef*>(outBuffer.get()) + bufferOutPosition;
		[[maybe_unused]] const int ret = deflate(&zlibState, finish ? Z_FINISH : Z_NO_FLUSH);    // no bad return value
		assert(ret != Z_STREAM_ERROR); // state not clobbered

		bufferOutPosition = kCiv4BlockSize - zlibState.avail_out;
		if (bufferOutPosition >= kCiv4BlockSize)
			flushOutputBlock();

	} while (zlibState.avail_out == 0);
	assert(zlibState.avail_in == 0);     // all input will be used

	if (finish && bufferOutPosition > 0)
		flushOutputBlock();
}

///

DecompressionBinaryStream::DecompressionBinaryStream(IRawBinaryStream* baseStream) : baseStream(baseStream)
{
	ret = inflateInit(&zlibState);
	if (ret != Z_OK)
		throw std::runtime_error("inflateInit failed.");
}
DecompressionBinaryStream::~DecompressionBinaryStream()
{
	(void)inflateEnd(&zlibState);
}

std::vector<std::byte> DecompressionBinaryStream::readUntilEnd()
{
	std::vector<std::byte> blob;

	for (;;)
	{
		if (const size_t n = bufferOutEnd - bufferOutPosition)
		{
			blob.append_range(std::span(outBuffer.get(), bufferOutEnd).subspan(bufferOutPosition, n));
			bufferOutPosition += n;
		}
		else
		{
			refill();
			if (bufferOutEnd == bufferOutPosition)
				break;
		}
	}

	return blob;
}

void DecompressionBinaryStream::read(std::span<std::byte> blob)
{
	while (!blob.empty())
	{
		if (const size_t n = std::min(blob.size(), bufferOutEnd - bufferOutPosition)) [[likely]]
		{
			std::ranges::copy(std::span(outBuffer.get(), bufferOutEnd).subspan(bufferOutPosition, n), blob.begin());
			bufferOutPosition += n;
			blob = blob.subspan(n);
		}
		else
		{
			refill();
			if (bufferOutEnd == bufferOutPosition)
				throw std::runtime_error("EOF during decompression.");
		}
	}
}

void DecompressionBinaryStream::write([[maybe_unused]] std::span<const std::byte> blob)
{
	std::abort();
}

void DecompressionBinaryStream::assertStreamEnd()
{
	if (ret != Z_STREAM_END)
		throw std::runtime_error("zlib stream unexpectedly did not end at the current read position.");
}

void DecompressionBinaryStream::refill()
{
	while (ret == Z_OK)
	{
		if (bufferInPosition >= bufferInEnd)
		{
			uint32_t blockSize = 0;
			baseStream->readTrivial(blockSize);
			baseStream->read(std::span(inBuffer.get(), blockSize));
			bufferInPosition = 0;
			bufferInEnd = blockSize;
		}

		zlibState.avail_in = (uint32_t)(bufferInEnd - bufferInPosition);
		if (zlibState.avail_in == 0)
			throw std::runtime_error("Unexpected EOF during decompression.");
		// Needs to be non-const for old zlib.
		zlibState.next_in = reinterpret_cast<Bytef*>(inBuffer.get()) + bufferInPosition;

		/* run inflate() on input until output buffer not full */
		//do {
		zlibState.avail_out = kCiv4BlockSize;
		zlibState.next_out = reinterpret_cast<Bytef*>(outBuffer.get());
		ret = inflate(&zlibState, Z_NO_FLUSH);
		assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
		switch (ret)
		{
		case Z_NEED_DICT:
			throw std::runtime_error("Error during decompression. Missing dictionary.");
		case Z_STREAM_END:
			break;
		default:
			// #define Z_DATA_ERROR   (-3)
			// #define Z_MEM_ERROR    (-4)
			if (ret < 0)
				throw std::runtime_error("Error during decompression. Possibly corrupt data.");
			break;
		}

		bufferOutPosition = 0;
		bufferOutEnd = kCiv4BlockSize - zlibState.avail_out;
		if (zlibState.avail_out < kCiv4BlockSize)
			break;
		//} while (zlibState.avail_out == 0);
	}

	bufferInPosition = bufferInEnd - zlibState.avail_in;
}