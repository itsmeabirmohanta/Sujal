#pragma once
#include "flight_core.hpp"
#include <array>
#include <cstring>
namespace vritra::protocol {
// V2 payload cap is 64 bytes; total decoded frame <=90, COBS wire <=92.
constexpr size_t payload_cap=64,header_bytes=22,decoded_cap=header_bytes+payload_cap+4,wire_cap=decoded_cap+2;
struct Frame {
    uint8_t type{0},source{0},flags{0};uint32_t session{0},sequence{0};
    uint64_t send_us{0};uint16_t length{0};std::array<uint8_t,payload_cap> payload{};
};
inline void put(uint8_t* out,uint64_t value,size_t bytes) {
    for(size_t i=0;i<bytes;++i)out[i]=static_cast<uint8_t>(value>>(8*i));
}
inline uint64_t get(const uint8_t* p,size_t bytes) {
    uint64_t v=0;for(size_t i=0;i<bytes;++i)v|=uint64_t(p[i])<<(8*i);return v;
}
inline size_t cobsEncode(const uint8_t* in,size_t n,uint8_t* out,size_t cap) {
    if(cap<2)return 0;
    size_t code_index=0,w=1;uint8_t code=1;
    for(size_t i=0;i<n;++i) {
        if(in[i]==0) {
            out[code_index]=code;code_index=w;if(w>=cap)return 0;++w;code=1;
        } else {
            if(w>=cap)return 0;
            out[w++]=in[i];
            if(++code==0xFF) {out[code_index]=code;code_index=w;if(w>=cap)return 0;++w;code=1;}
        }
    }
    out[code_index]=code;if(w>=cap)return 0;out[w++]=0;return w;
}
inline size_t cobsDecode(const uint8_t* in,size_t n,uint8_t* out,size_t cap) {
    if(!n || in[n-1]!=0)return 0;
    --n;size_t r=0,w=0;
    while(r<n) {
        const uint8_t code=in[r++];if(!code || r+code-1>n)return 0;
        for(unsigned i=1;i<code;++i) {if(!in[r] || w>=cap)return 0;out[w++]=in[r++];}
        if(code!=0xFF && r<n) {if(w>=cap)return 0;out[w++]=0;}
    }
    return w;
}
inline size_t encode(const Frame& f,uint8_t* out,size_t cap) {
    if(f.length>payload_cap || (f.source!=1 && f.source!=2 && f.source!=4) || !f.type)return 0;
    std::array<uint8_t,decoded_cap> b{};b[0]=2;b[1]=f.type;b[2]=f.source;b[3]=f.flags;
    put(b.data()+4,f.session,4);put(b.data()+8,f.sequence,4);put(b.data()+12,f.send_us,8);
    put(b.data()+20,f.length,2);std::memcpy(b.data()+22,f.payload.data(),f.length);
    put(b.data()+22+f.length,crc32(b.data(),22+f.length),4);
    return cobsEncode(b.data(),26+f.length,out,cap);
}
inline bool decode(const uint8_t* in,size_t n,Frame& out) {
    std::array<uint8_t,decoded_cap>b{};const size_t size=cobsDecode(in,n,b.data(),b.size());
    if(size<26 || b[0]!=2 || (b[2]!=1 && b[2]!=2 && b[2]!=4) || !b[1])return false;
    const size_t length=get(b.data()+20,2);
    if(length>payload_cap || size!=26+length || get(b.data()+22+length,4)!=crc32(b.data(),22+length))return false;
    Frame f;f.type=b[1];f.source=b[2];f.flags=b[3];f.session=uint32_t(get(b.data()+4,4));
    f.sequence=uint32_t(get(b.data()+8,4));f.send_us=get(b.data()+12,8);f.length=uint16_t(length);
    std::memcpy(f.payload.data(),b.data()+22,length);out=f;return true;
}
// Input assembler has a bounded cap and discards an overlong frame until delimiter.
class Receiver {
    std::array<uint8_t,wire_cap> bytes_{};size_t used_{0};bool overflow_{false};
public:
    bool byte(uint8_t b,Frame& frame) {
        if(overflow_) {if(b==0){overflow_=false;used_=0;}return false;}
        if(used_==bytes_.size()) {overflow_=b!=0;used_=0;return false;}
        bytes_[used_++]=b;if(b!=0)return false;
        const bool ok=decode(bytes_.data(),used_,frame);used_=0;return ok;
    }
};
} // namespace vritra::protocol
