/*
*	IPK Project 1: client-server for simple file transfer
*	Author: Tom� Pazdiora (xpazdi02)
*	File: IPKPacket.cpp
*/

#include "IPKPacket.h"
#include "CRC32.h"
#include <algorithm>
#include <stdexcept>

const std::string IPKPacket::signature{ "IPKFTP" };
const uint8_t IPKPacket::version{ 1 };

// Create Packet
IPKPacket::IPKPacket(IPKTransmissionType type, std::string filename, std::vector<unsigned char> data)
	: type(type), filename(filename), data(data)
{
	if (type == RequestFile && (filename.size() == 0 || data.size() > 0)) {
		throw IPKPacketException(PacketCreationError, "IPKTransmissionType::RequestFile requires filename");
	}
	else if (type == OfferFile && (filename.size() == 0 || data.size() == 0)) {
		throw IPKPacketException(PacketCreationError, "IPKTransmissionType::OfferFile requires filename");
	}
}

// Deserialize
IPKPacket::IPKPacket(const std::vector<unsigned char> message)
	: type(Unknown), filename(), data()
{
	// bypass const for initialization within this constructor
	auto &type_notconst = *(const_cast<IPKTransmissionType*>(&this->type));
	auto &filename_notconst = *(const_cast<std::string*>(&this->filename));
	auto &data_notconst = *(const_cast<std::vector<unsigned char>*>(&this->data));

	// check signature
	if (this->signature != std::string(message.begin(), message.begin() + 0x6)) {
		throw(IPKPacketException(SignatureError));
	}
	// check version
	if (this->version != static_cast<uint8_t>(*(message.begin() + 0x6))) {
		throw(IPKPacketException(VersionError));
	}
	// check crc
	auto crc = *(reinterpret_cast<const uint32_t*>(&(*(message.end() - 0x4))));
	if (crc != CRC32(message.begin(), message.end() - 0x4)) {
		throw(IPKPacketException(CRC32Error));
	}
	// check transmission type
	auto t = static_cast<IPKTransmissionType>(*(message.begin() + 0x7));
	if (t >= Unknown || t < 0) {
		throw(IPKPacketException(TransmissionTypeError));
	}
	type_notconst = t;
	// check overall message size
	auto overall_size = *(reinterpret_cast<const uint64_t*>(&(*(message.begin() + 0x8))));
	if (overall_size != message.size() ) {
		throw(IPKPacketException(SizeError));
	}
	// load filename
	auto message_data_it = message.end();
	if (type == RequestFile || type == OfferFile) {
		for (auto it = message.begin() + 0x10; it != message.end(); it++) {
			if (*it == 0) {
				message_data_it = it + 1;
				break;
			}
			filename_notconst += *it;
		}
	}
	// load data
	if (type == OfferFile) {
		std::copy(message_data_it, message.end() - 0x4, std::back_inserter(data_notconst));
	}
}

// Serialize
IPKPacket::operator const std::vector<unsigned char>() const
{
	std::vector<unsigned char> message;

	uint64_t overall_size = 20;
	if (this->type == RequestFile || this->type == OfferFile) {
		overall_size += this->filename.size() + 1;
	}
	if (this->type == OfferFile) {
		overall_size += this->data.size();
	}
	message.resize(overall_size);

	auto it = std::begin(message);

	it = std::copy(std::begin(this->signature), std::end(this->signature), it); // signature
	*(it++) = static_cast<unsigned char>(this->version); // version
	*(it++) = static_cast<unsigned char>(this->type); // transmission type
	unsigned char *overall_size_ptr = reinterpret_cast<unsigned char*>(&overall_size);
	it = std::copy(overall_size_ptr, overall_size_ptr + sizeof(overall_size), it); // overall size
	if (this->type == RequestFile || this->type == OfferFile) {
		it = std::copy(std::begin(this->filename), std::end(this->filename), it); // filename
		*(it++) = static_cast<unsigned char>(0); // null terminator
	}
	if (this->type == OfferFile) {
		it = std::copy(std::begin(this->data), std::end(this->data), it); // file data
	}

	uint32_t crc = CRC32(std::begin(message), std::end(message) - 4);
	unsigned char *crc_ptr = reinterpret_cast<unsigned char*>(&crc);
	it = std::copy(crc_ptr, crc_ptr + sizeof(crc), it); // crc

	return message;
}

const std::string IPKPacket::GetFilename() const
{
	return this->filename;
}

const std::vector<unsigned char> IPKPacket::GetData() const
{
	return this->data;
}

IPKPacketException::IPKPacketException(const IPKPacketError error, const std::string message)
	: std::runtime_error(message), error(error)
{
}
