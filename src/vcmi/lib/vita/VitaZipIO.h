// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
/*
 * Part of VCMI engine, licensed under GNU GPL v2.0 or later.
 * See license.txt in the project root.
 */
#pragma once

#include "../filesystem/MinizipExtensions.h"
#include "../filesystem/CFileInputStream.h"
#include <atomic>

// Each ZIP handle has its own cursor over the same immutable archive bytes.
class VitaZipIO final : public CIOApi
{
	struct Cursor
	{
		std::shared_ptr<const std::vector<ui8>> bytes;
		size_t position = 0;
	};
	std::shared_ptr<const std::vector<ui8>> bytes;

	static voidpf ZCALLBACK open(voidpf opaque, const void *, int mode)
	{
		if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) != ZLIB_FILEFUNC_MODE_READ)
			return nullptr;
		openCalls.fetch_add(1, std::memory_order_relaxed);
		return new Cursor{static_cast<VitaZipIO *>(opaque)->bytes};
	}
	static uLong ZCALLBACK read(voidpf, voidpf stream, void * output, uLong requested)
	{
		auto & cursor = *static_cast<Cursor *>(stream);
		const auto count = std::min<size_t>(requested, cursor.bytes->size() - cursor.position);
		std::memcpy(output, cursor.bytes->data() + cursor.position, count);
		cursor.position += count;
		return count;
	}
	static uLong ZCALLBACK write(voidpf, voidpf, const void *, uLong) { return 0; }
	static ZPOS64_T ZCALLBACK tell(voidpf, voidpf stream) { return static_cast<Cursor *>(stream)->position; }
	static long ZCALLBACK seek(voidpf, voidpf stream, ZPOS64_T offset, int origin)
	{
		auto & cursor = *static_cast<Cursor *>(stream);
		const int64_t displacement = static_cast<int64_t>(offset);
		int64_t base;
		switch (origin)
		{
		case ZLIB_FILEFUNC_SEEK_SET: base = 0; break;
		case ZLIB_FILEFUNC_SEEK_CUR: base = cursor.position; break;
		case ZLIB_FILEFUNC_SEEK_END: base = cursor.bytes->size(); break;
		default: return -1;
		}
		if (displacement < -base || displacement > static_cast<int64_t>(cursor.bytes->size()) - base)
			return -1;
		cursor.position = base + displacement;
		return 0;
	}
	static int ZCALLBACK close(voidpf, voidpf stream) { delete static_cast<Cursor *>(stream); return 0; }
	static int ZCALLBACK error(voidpf, voidpf) { return 0; }

public:
	inline static std::atomic<uint32_t> openCalls{0}, cacheHits{0};
	explicit VitaZipIO(const boost::filesystem::path & path)
	{
		CFileInputStream input(path);
		auto storage = std::make_shared<std::vector<ui8>>(input.getSize());
		if (input.read(storage->data(), storage->size()) != static_cast<si64>(storage->size()))
			throw std::runtime_error("Incomplete built-in ZIP read: " + path.string());
		bytes = std::move(storage);
		std::fprintf(stderr, "Vita ZIP memory: path=%s bytes=%u\n", path.string().c_str(), static_cast<unsigned>(bytes->size()));
	}
	zlib_filefunc64_def getApiStructure() override
	{
		return {open, read, write, tell, seek, close, error, this};
	}
};
