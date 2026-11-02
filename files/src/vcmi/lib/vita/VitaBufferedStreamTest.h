// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../filesystem/CCompressedStream.h"
#include <array>
#include <cstdio>
#include <stdexcept>

inline void runVitaBufferedStreamTest()
{
	class TestStream : public CBufferedStream
	{
		si64 length, sourcePosition = 0;
		si64 readMore(ui8 * output, si64 requested) override
		{
			const si64 count = std::min(requested, length - sourcePosition);
			for(si64 i = 0; i < count; ++i)
				output[i] = static_cast<ui8>((sourcePosition + i) * 71 + 19);
			sourcePosition += count;
			return count;
		}
	public:
		TestStream(si64 length, size_t hint) : CBufferedStream(hint), length(length) {}
	};
	unsigned cases = 0;
	for(size_t length : {0u, 1u, 511u, 512u, 513u, 1024u, 1025u, 4096u, 4097u, 65536u})
		for(size_t hint : {size_t{0}, length, length / 2, length + 17})
		{
			TestStream stream(length, hint);
			std::array<ui8, 17> prefix{};
			const si64 prefixCount = std::min(length, prefix.size());
			if(stream.read(prefix.data(), prefix.size()) != prefixCount || stream.getSize() != static_cast<si64>(length)
				|| stream.tell() != prefixCount)
				throw std::runtime_error("Vita buffered stream size/position test failed");
			stream.seek(0);
			auto all = stream.readAll();
			if(all.second != static_cast<si64>(length))
				throw std::runtime_error("Vita buffered stream readAll size test failed");
			for(size_t i = 0; i < length; ++i)
				if(all.first[i] != static_cast<ui8>(i * 71 + 19))
					throw std::runtime_error("Vita buffered stream content test failed");
			if(stream.read(prefix.data(), prefix.size()) != 0)
				throw std::runtime_error("Vita buffered stream EOF test failed");
			++cases;
		}
	std::fprintf(stderr, "Vita buffered stream tests: %u cases passed\n", cases);
}
