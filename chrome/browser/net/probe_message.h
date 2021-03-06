// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROBE_MESSAGE_H_
#define CHROME_BROWSER_NET_PROBE_MESSAGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/net/probe_message.pb.h"

namespace chrome_browser_net {

// Packet format between client and server is defined in probe_message.proto.
class ProbeMessage {
 public:
  ProbeMessage();

  // Generate a ProbeRequest packet.
  void GenerateProbeRequest(const ProbePacket_Token& received_token,
                            uint32 group_id,
                            uint32 probe_size,
                            uint32 pacing_interval_micros,
                            uint32 number_probe_packets,
                            ProbePacket* output);
  // Make an encoded packet (string) from a ProbePacket object.
  std::string MakeEncodedPacket(const ProbePacket& packet) const;
  // Fill some common fields in the packet header.
  void SetPacketHeader(ProbePacket_Type packet_type,
                       ProbePacket* probe_packet) const;
  // Parse the input string (which is an encoded packet) and save the result to
  // packet. Return true if there is no error and false otherwise.
  bool ParseInput(const std::string& input, ProbePacket* packet) const;

  static const uint32 kMaxProbePacketBytes;

 private:
  // For unittest.
  friend class ProbeMessageTest;
  FRIEND_TEST_ALL_PREFIXES(ProbeMessageTest, TestChecksum);
  FRIEND_TEST_ALL_PREFIXES(ProbeMessageTest, TestEncode);
  FRIEND_TEST_ALL_PREFIXES(ProbeMessageTest, TestGenerateProbeRequest);
  FRIEND_TEST_ALL_PREFIXES(ProbeMessageTest, TestSetPacketHeader);

  // Compute the checksum of the padding string.
  uint32 Checksum(const std::string& str) const;

  // Encode the packet with kEncodingString. This is also used for decoding.
  std::string Encode(const std::string& input) const;

  static const uint32 kVersion;
  static const uint32 kMaxNumberProbePackets;
  static const uint32 kMaxPacingIntervalMicros;
  static const char kEncodingString[];

  DISALLOW_COPY_AND_ASSIGN(ProbeMessage);
};
}       // namespace chrome_browser_net
#endif  // CHROME_BROWSER_NET_PROBE_MESSAGE_H_
