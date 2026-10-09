#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MessagingProtocol {
constexpr uint16_t kMagic=0x504D; constexpr uint8_t kVersion=1; constexpr size_t kHeaderSize=30; constexpr size_t kTagSize=16; constexpr size_t kMaxPacket=255; constexpr size_t kMaxText=160;
enum class Type:uint8_t{Message=1,Ack=2,PairHello=3,PairConfirm=4};
struct Header{Type type=Type::Message;uint64_t sender=0,recipient=0;uint32_t messageId=0,counter=0;uint16_t payloadLength=0;};
bool encodeEncrypted(const Header&header,const uint8_t key[32],const uint8_t*plain,size_t plainLength,uint8_t*out,size_t capacity,size_t&outLength);
bool decodeEncrypted(const uint8_t*packet,size_t packetLength,const uint8_t key[32],Header&header,uint8_t*plain,size_t capacity,size_t&plainLength);
bool encodePairHello(uint64_t sender,const char*name,const char*code,uint8_t*out,size_t capacity,size_t&outLength);
bool decodePairHello(const uint8_t*packet,size_t packetLength,const char*code,Header&header,char*name,size_t nameCapacity,uint8_t derivedKey[32]);
bool inspectHeader(const uint8_t*packet,size_t packetLength,Header&header);
void derivePairKey(const char*code,uint64_t first,uint64_t second,uint8_t key[32]);
}
