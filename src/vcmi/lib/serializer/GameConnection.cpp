/*
 * GameConnection.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "GameConnection.h"
#ifdef VCMI_VITA
#include "../vita/VitaSendStats.h"
#endif

#include "BinaryDeserializer.h"
#include "BinarySerializer.h"

#include "../gameState/CGameState.h"
#include "../networkPacks/NetPacksBase.h"
#include "../network/NetworkInterface.h"

class GameConnectionPackWriter final : public IBinaryWriter
{
public:
	std::vector<std::byte> buffer;

	int write(const std::byte * data, unsigned size) final;
};

class GameConnectionPackReader final : public IBinaryReader
{
public:
	const std::vector<std::byte> * buffer;
	size_t position;

	int read(std::byte * data, unsigned size) final;
};

int GameConnectionPackWriter::write(const std::byte * data, unsigned size)
{
	buffer.insert(buffer.end(), data, data + size);
	return size;
}

int GameConnectionPackReader::read(std::byte * data, unsigned size)
{
	if (position + size > buffer->size())
		throw std::runtime_error("End of file reached when reading received network pack!");

	std::copy_n(buffer->begin() + position, size, data);
	position += size;
	return size;
}

GameConnection::GameConnection(std::weak_ptr<INetworkConnection> networkConnection)
	: networkConnection(networkConnection)
	, packReader(std::make_unique<GameConnectionPackReader>())
	, packWriter(std::make_unique<GameConnectionPackWriter>())
	, deserializer(std::make_unique<BinaryDeserializer>(packReader.get()))
	, serializer(std::make_unique<BinarySerializer>(packWriter.get()))
{
	assert(networkConnection.lock() != nullptr);

	enterLobbyConnectionMode();
	deserializer->version = ESerializationVersion::CURRENT;
}

GameConnection::~GameConnection() = default;

void GameConnection::sendPack(const CPack & pack)
{
#ifdef VCMI_VITA
	const auto lockStarted = std::chrono::steady_clock::now();
#endif
	std::scoped_lock lock(writeMutex);
#ifdef VCMI_VITA
	VitaSendStats::counters.at(static_cast<size_t>(VitaSendStats::Kind::ConnectionLock)).add(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - lockStarted).count()));
#endif

	auto connectionPtr = networkConnection.lock();

	if (!connectionPtr)
		throw std::runtime_error("Attempt to send packet on a closed connection!");

	{
#ifdef VCMI_VITA
		VitaSendStats::Scope measure(VitaSendStats::Kind::Encode);
#endif
		packWriter->buffer.clear();
		*serializer & &pack;
	}

	logNetwork->trace("Sending a pack of type %s", typeid(pack).name());

	{
#ifdef VCMI_VITA
		VitaSendStats::Scope measure(VitaSendStats::Kind::Transport);
#endif
		connectionPtr->sendPacket(packWriter->buffer);
	}
	{
#ifdef VCMI_VITA
		VitaSendStats::Scope measure(VitaSendStats::Kind::Finalize);
#endif
		packWriter->buffer.clear();
		serializer->clear();
	}
}

std::unique_ptr<CPack> GameConnection::retrievePack(const std::vector<std::byte> & data)
{
	std::unique_ptr<CPack> result;

	packReader->buffer = &data;
	packReader->position = 0;

	*deserializer & result;

	if (result == nullptr)
		throw std::runtime_error("Failed to retrieve pack!");

	if (packReader->position != data.size())
		throw std::runtime_error("Failed to retrieve pack! Not all data has been read!");

	auto packRawPtr = result.get();
	logNetwork->trace("Received CPack of type %s", typeid(*packRawPtr).name());
	deserializer->clear();
	return result;
}

bool GameConnection::isMyConnection(const std::shared_ptr<INetworkConnection> & otherConnection) const
{
	return otherConnection != nullptr && networkConnection.lock() == otherConnection;
}

std::shared_ptr<INetworkConnection> GameConnection::getConnection()
{
	return networkConnection.lock();
}

void GameConnection::enterLobbyConnectionMode()
{
	deserializer->clear();
	serializer->clear();
}

void GameConnection::setCallback(IGameInfoCallback & cb)
{
	deserializer->cb = &cb;
}

void GameConnection::setSerializationVersion(ESerializationVersion version)
{
	deserializer->version = version;
	serializer->version = version;
}
