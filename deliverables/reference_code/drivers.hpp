#pragma once
#include "flight_core.hpp"
#include <array>
namespace vritra::drivers {
struct RegWrite {uint8_t address,bank,reg,value;uint32_t wait_after_us;};
// Datasheet/driver-derived constants; see numeric_traceability.csv for sources.
constexpr std::array<RegWrite,8> bmp585_boot{{
 {0x47,0,0x7E,0xB6,2000},{0x47,0,0x37,0xBC,3000},{0x47,0,0x16,0,0},{0x47,0,0x18,0,0},
 {0x47,0,0x15,0,0},{0x47,0,0x30,0x03,0},{0x47,0,0x31,0,0},{0x47,0,0x36,0x61,0}
}}; // verify status, then write0x37=0xBD and verify0x38, before publishing
constexpr std::array<RegWrite,11> lsm_boot{{
 {0x6A,0,0x12,1,20000},{0x6A,0,0x12,0x44,0},{0x6A,0,0x10,0x5C,0},{0x6A,0,0x11,0x5C,0},
 {0x6A,0,0x13,2,0},{0x6A,0,0x15,3,0},{0x6A,0,0x16,0,0},{0x6A,0,0x17,0,0},
 {0x6A,0,0x19,0x20,0},{0x6A,0,0x0D,0,0},{0x6A,0,0x0E,0,0}
}}; // FIFO registers stay at verified reset/bypass; reset bit must clear
inline int16_t le16(const uint8_t* p){return int16_t(uint16_t(p[0])|(uint16_t(p[1])<<8));}
inline int16_t be16(const uint8_t* p){return int16_t((uint16_t(p[0])<<8)|p[1]);}
inline int32_t s24(const uint8_t* p) {
    const uint32_t x=uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16);
    return x&0x800000u?int32_t(x|0xFF000000u):int32_t(x);
}
inline uint8_t crc8(const uint8_t* p,size_t n) {
    uint8_t c=0xFF;for(size_t i=0;i<n;++i){c^=p[i];for(int j=0;j<8;++j)c=uint8_t((c<<1)^((c&0x80)?0x31:0));}return c;
}
inline bool bmp585(const uint8_t* six,double& pa,double& celsius) {
    const uint32_t raw=uint32_t(six[3])|(uint32_t(six[4])<<8)|(uint32_t(six[5])<<16);
    if(raw==0x7F7F7Fu||raw==0xFFFFFFu)return false;
    pa=raw/64.0;celsius=s24(six)/65536.0;return pa>=30000&&pa<=125000&&celsius>=-40&&celsius<=85;
}
inline bool hdc3020(const uint8_t* six,double& celsius,double& humidity) {
    if(crc8(six,2)!=six[2]||crc8(six+3,2)!=six[5])return false;
    const uint16_t t=(uint16_t(six[0])<<8)|six[1],h=(uint16_t(six[3])<<8)|six[4];
    celsius=-45+175.0*t/65535;humidity=100.0*h/65535;return true;
}
struct IoResult {enum Code {Busy,Ok,Fault}code;};
// All hardware primitives return within a declared bounded transaction deadline.
// Implement with controller timeouts; the interface alone is not a timing proof.
class I2cBootIo {public:virtual ~I2cBootIo()=default;virtual IoResult write(uint8_t addr,uint8_t bank,uint8_t reg,uint8_t value)=0;};
template<size_t N> class BootWriter {
    const std::array<RegWrite,N>& writes_;size_t index_{0};uint64_t resume_us_{0};bool fault_{false};
public:
    explicit BootWriter(const std::array<RegWrite,N>& writes):writes_(writes){}
    IoResult step(uint64_t now,I2cBootIo& io) {
        if(fault_)return {IoResult::Fault};
        if(now<resume_us_)return {IoResult::Busy};
        if(index_==N)return {IoResult::Ok};
        const auto& w=writes_[index_];const IoResult r=io.write(w.address,w.bank,w.reg,w.value);
        if(r.code==IoResult::Fault){fault_=true;return r;}
        if(r.code==IoResult::Ok){++index_;resume_us_=now+w.wait_after_us;}
        return {IoResult::Busy};
    }
};
} // namespace vritra::drivers
