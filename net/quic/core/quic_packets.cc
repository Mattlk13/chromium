// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_packets.h"

#include "base/memory/ptr_util.h"
#include "net/quic/core/quic_flags.h"
#include "net/quic/core/quic_utils.h"
#include "net/quic/core/quic_versions.h"
#include "net/quic/platform/api/quic_str_cat.h"
#include "net/quic/platform/api/quic_text_utils.h"

using base::StringPiece;
using std::string;

namespace net {

size_t GetPacketHeaderSize(QuicVersion version,
                           const QuicPacketHeader& header) {
  return GetPacketHeaderSize(version, header.public_header.connection_id_length,
                             header.public_header.version_flag,
                             header.public_header.multipath_flag,
                             header.public_header.nonce != nullptr,
                             header.public_header.packet_number_length);
}

size_t GetPacketHeaderSize(QuicVersion version,
                           QuicConnectionIdLength connection_id_length,
                           bool include_version,
                           bool include_path_id,
                           bool include_diversification_nonce,
                           QuicPacketNumberLength packet_number_length) {
  return kPublicFlagsSize + connection_id_length +
         (include_version ? kQuicVersionSize : 0) +
         (include_path_id ? kQuicPathIdSize : 0) + packet_number_length +
         (include_diversification_nonce ? kDiversificationNonceSize : 0);
}

size_t GetStartOfEncryptedData(QuicVersion version,
                               const QuicPacketHeader& header) {
  return GetPacketHeaderSize(version, header);
}

size_t GetStartOfEncryptedData(QuicVersion version,
                               QuicConnectionIdLength connection_id_length,
                               bool include_version,
                               bool include_path_id,
                               bool include_diversification_nonce,
                               QuicPacketNumberLength packet_number_length) {
  // Encryption starts before private flags.
  return GetPacketHeaderSize(version, connection_id_length, include_version,
                             include_path_id, include_diversification_nonce,
                             packet_number_length);
}

QuicPacketPublicHeader::QuicPacketPublicHeader()
    : connection_id(0),
      connection_id_length(PACKET_8BYTE_CONNECTION_ID),
      multipath_flag(false),
      reset_flag(false),
      version_flag(false),
      packet_number_length(PACKET_6BYTE_PACKET_NUMBER),
      nonce(nullptr) {}

QuicPacketPublicHeader::QuicPacketPublicHeader(
    const QuicPacketPublicHeader& other) = default;

QuicPacketPublicHeader::~QuicPacketPublicHeader() {}

QuicPacketHeader::QuicPacketHeader()
    : packet_number(0), path_id(kDefaultPathId) {}

QuicPacketHeader::QuicPacketHeader(const QuicPacketPublicHeader& header)
    : public_header(header), packet_number(0), path_id(kDefaultPathId) {}

QuicPacketHeader::QuicPacketHeader(const QuicPacketHeader& other) = default;

QuicPublicResetPacket::QuicPublicResetPacket()
    : nonce_proof(0), rejected_packet_number(0) {}

QuicPublicResetPacket::QuicPublicResetPacket(
    const QuicPacketPublicHeader& header)
    : public_header(header), nonce_proof(0), rejected_packet_number(0) {}

std::ostream& operator<<(std::ostream& os, const QuicPacketHeader& header) {
  os << "{ connection_id: " << header.public_header.connection_id
     << ", connection_id_length: " << header.public_header.connection_id_length
     << ", packet_number_length: " << header.public_header.packet_number_length
     << ", multipath_flag: " << header.public_header.multipath_flag
     << ", reset_flag: " << header.public_header.reset_flag
     << ", version_flag: " << header.public_header.version_flag;
  if (header.public_header.version_flag) {
    os << ", version:";
    for (size_t i = 0; i < header.public_header.versions.size(); ++i) {
      os << " ";
      os << QuicVersionToString(header.public_header.versions[i]);
    }
  }
  if (header.public_header.nonce != nullptr) {
    os << ", diversification_nonce: "
       << QuicTextUtils::HexEncode(
              StringPiece(header.public_header.nonce->data(),
                          header.public_header.nonce->size()));
  }
  os << ", path_id: " << static_cast<int>(header.path_id)
     << ", packet_number: " << header.packet_number << " }\n";
  return os;
}

QuicData::QuicData(const char* buffer, size_t length)
    : buffer_(buffer), length_(length), owns_buffer_(false) {}

QuicData::QuicData(const char* buffer, size_t length, bool owns_buffer)
    : buffer_(buffer), length_(length), owns_buffer_(owns_buffer) {}

QuicData::~QuicData() {
  if (owns_buffer_) {
    delete[] const_cast<char*>(buffer_);
  }
}

QuicPacket::QuicPacket(char* buffer,
                       size_t length,
                       bool owns_buffer,
                       QuicConnectionIdLength connection_id_length,
                       bool includes_version,
                       bool includes_path_id,
                       bool includes_diversification_nonce,
                       QuicPacketNumberLength packet_number_length)
    : QuicData(buffer, length, owns_buffer),
      buffer_(buffer),
      connection_id_length_(connection_id_length),
      includes_version_(includes_version),
      includes_path_id_(includes_path_id),
      includes_diversification_nonce_(includes_diversification_nonce),
      packet_number_length_(packet_number_length) {}

QuicEncryptedPacket::QuicEncryptedPacket(const char* buffer, size_t length)
    : QuicData(buffer, length) {}

QuicEncryptedPacket::QuicEncryptedPacket(const char* buffer,
                                         size_t length,
                                         bool owns_buffer)
    : QuicData(buffer, length, owns_buffer) {}

std::unique_ptr<QuicEncryptedPacket> QuicEncryptedPacket::Clone() const {
  char* buffer = new char[this->length()];
  memcpy(buffer, this->data(), this->length());
  return base::MakeUnique<QuicEncryptedPacket>(buffer, this->length(), true);
}

std::ostream& operator<<(std::ostream& os, const QuicEncryptedPacket& s) {
  os << s.length() << "-byte data";
  return os;
}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time)
    : QuicEncryptedPacket(buffer, length),
      receipt_time_(receipt_time),
      ttl_(0) {}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time,
                                       bool owns_buffer)
    : QuicEncryptedPacket(buffer, length, owns_buffer),
      receipt_time_(receipt_time),
      ttl_(0) {}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time,
                                       bool owns_buffer,
                                       int ttl,
                                       bool ttl_valid)
    : QuicEncryptedPacket(buffer, length, owns_buffer),
      receipt_time_(receipt_time),
      ttl_(ttl_valid ? ttl : -1) {}

std::unique_ptr<QuicReceivedPacket> QuicReceivedPacket::Clone() const {
  char* buffer = new char[this->length()];
  memcpy(buffer, this->data(), this->length());
  return base::MakeUnique<QuicReceivedPacket>(
      buffer, this->length(), receipt_time(), true, ttl(), ttl() >= 0);
}

std::ostream& operator<<(std::ostream& os, const QuicReceivedPacket& s) {
  os << s.length() << "-byte data";
  return os;
}

StringPiece QuicPacket::AssociatedData(QuicVersion version) const {
  return StringPiece(
      data(), GetStartOfEncryptedData(version, connection_id_length_,
                                      includes_version_, includes_path_id_,
                                      includes_diversification_nonce_,
                                      packet_number_length_));
}

StringPiece QuicPacket::Plaintext(QuicVersion version) const {
  const size_t start_of_encrypted_data = GetStartOfEncryptedData(
      version, connection_id_length_, includes_version_, includes_path_id_,
      includes_diversification_nonce_, packet_number_length_);
  return StringPiece(data() + start_of_encrypted_data,
                     length() - start_of_encrypted_data);
}

SerializedPacket::SerializedPacket(QuicPathId path_id,
                                   QuicPacketNumber packet_number,
                                   QuicPacketNumberLength packet_number_length,
                                   const char* encrypted_buffer,
                                   QuicPacketLength encrypted_length,
                                   bool has_ack,
                                   bool has_stop_waiting)
    : encrypted_buffer(encrypted_buffer),
      encrypted_length(encrypted_length),
      has_crypto_handshake(NOT_HANDSHAKE),
      num_padding_bytes(0),
      path_id(path_id),
      packet_number(packet_number),
      packet_number_length(packet_number_length),
      encryption_level(ENCRYPTION_NONE),
      has_ack(has_ack),
      has_stop_waiting(has_stop_waiting),
      transmission_type(NOT_RETRANSMISSION),
      original_path_id(kInvalidPathId),
      original_packet_number(0) {}

SerializedPacket::SerializedPacket(const SerializedPacket& other) = default;

SerializedPacket::~SerializedPacket() {}

void ClearSerializedPacket(SerializedPacket* serialized_packet) {
  if (!serialized_packet->retransmittable_frames.empty()) {
    DeleteFrames(&serialized_packet->retransmittable_frames);
  }
  serialized_packet->encrypted_buffer = nullptr;
  serialized_packet->encrypted_length = 0;
}

char* CopyBuffer(const SerializedPacket& packet) {
  char* dst_buffer = new char[packet.encrypted_length];
  memcpy(dst_buffer, packet.encrypted_buffer, packet.encrypted_length);
  return dst_buffer;
}

}  // namespace net
