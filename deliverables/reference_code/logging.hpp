#pragma once
#include "protocol.hpp"
#include <atomic>
namespace vritra {
template<class T,size_t Capacity> class SpscQueue {
    static_assert(Capacity>=2);std::array<T,Capacity>data_{};
    std::atomic<size_t>head_{0},tail_{0};
public:
    bool push(const T& v) {
        const size_t h=head_.load(std::memory_order_relaxed),next=(h+1)%Capacity;
        if(next==tail_.load(std::memory_order_acquire))return false;
        data_[h]=v;head_.store(next,std::memory_order_release);return true;
    }
    bool pop(T& v) {
        const size_t t=tail_.load(std::memory_order_relaxed);
        if(t==head_.load(std::memory_order_acquire))return false;
        v=data_[t];tail_.store((t+1)%Capacity,std::memory_order_release);return true;
    }
};
struct LogRecord {
    uint8_t type{0},source{0};uint32_t session{0},sequence{0};uint64_t sample_us{0},mission_us{0};
    uint16_t length{0},flags{0};std::array<uint8_t,64>payload{};
};
constexpr size_t log_record_max=104;
inline size_t encodeLog(const LogRecord& r,uint8_t* out,size_t cap) {
    if(!out||r.length>64||r.source<1||r.source>4||!r.type||cap<size_t(40)+r.length)return 0;
    out[0]='V';out[1]='L';out[2]='R';out[3]='2';out[4]=2;out[5]=r.type;out[6]=r.source;out[7]=0;
    protocol::put(out+8,r.session,4);protocol::put(out+12,r.sequence,4);
    protocol::put(out+16,r.sample_us,8);protocol::put(out+24,r.mission_us,8);
    protocol::put(out+32,r.length,2);protocol::put(out+34,r.flags,2);
    std::memcpy(out+36,r.payload.data(),r.length);protocol::put(out+36+r.length,crc32(out,36+r.length),4);return 40+r.length;
}
inline bool decodeLog(const uint8_t* in,size_t n,LogRecord& out) {
    if(!in||n<40||in[0]!='V'||in[1]!='L'||in[2]!='R'||in[3]!='2'||in[4]!=2||!in[5]||in[6]<1||in[6]>4)return false;
    const size_t len=protocol::get(in+32,2);
    if(len>64||n!=40+len||protocol::get(in+36+len,4)!=crc32(in,36+len))return false;
    LogRecord r;r.type=in[5];r.source=in[6];r.session=uint32_t(protocol::get(in+8,4));r.sequence=uint32_t(protocol::get(in+12,4));
    r.sample_us=protocol::get(in+16,8);r.mission_us=protocol::get(in+24,8);r.length=uint16_t(len);r.flags=uint16_t(protocol::get(in+34,2));
    std::memcpy(r.payload.data(),in+36,len);out=r;return true;
}
} // namespace vritra
