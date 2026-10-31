/*
 * CFileInputStream.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CFileInputStream.h"

#include "../ExceptionsCommon.h"
#include "../texts/TextOperations.h"

CFileInputStream::CFileInputStream(const boost::filesystem::path & file, si64 start, si64 size)
  : dataStart{start},
	dataSize{size},
	fileStream{file.c_str(), std::ios::in | std::ios::binary}
{
	if (fileStream.fail())
		throw DataLoadingException("Failed to open file '" + TextOperations::filesystemPathToUtf8(file) + "'. Reason: " + strerror(errno) );

	if (dataSize == 0)
	{
		fileStream.seekg(0, std::ios::end);
		dataSize = static_cast<si64>(fileStream.tellg()) - dataStart;
	}

	fileStream.seekg(dataStart, std::ios::beg);
}


si64 CFileInputStream::read(ui8 * data, si64 size)
{
	si64 toRead = std::min(dataSize - position, size);
	fileStream.read(reinterpret_cast<char *>(data), toRead);
	const si64 count = fileStream.gcount();
	position += count;
	return count;
}

si64 CFileInputStream::seek(si64 position)
{
	const si64 target = std::clamp(position, si64{0}, dataSize);
	fileStream.clear();
	fileStream.seekg(dataStart + target);
	if (fileStream.fail())
		return -1;
	this->position = target;
	return target;
}

si64 CFileInputStream::tell()
{
	return position;
}

si64 CFileInputStream::skip(si64 delta)
{
	const si64 origin = position;
	if (seek(position + delta) < 0)
		return -1;
	return position - origin;
}

si64 CFileInputStream::getSize()
{
	return dataSize;
}
