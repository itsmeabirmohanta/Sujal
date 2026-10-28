#pragma once
#include "protocol.hpp"
namespace vritra {
struct MissionJournal {
    uint32_t generation{0},mission{0};uint64_t elapsed_us{0},launch_us{0},petri_close_us{0};
    uint64_t start_rtc_s{0},launch_rtc_s{0};uint32_t pad_pa_x64{0},pad_millikelvin{0};
    int32_t target_lat_e7{0},target_lon_e7{0};uint32_t target_revision{0};uint8_t state{0},flags{0};
};
enum JournalFlag:uint8_t {Armed=1,Released=2,BayIntent=4,PetriStarted=8,PetriDone=16,TxCutoff=32,TimeTrusted=64,GuidanceEnabled=128};
constexpr size_t journal_bytes=88;
using JournalBytes=std::array<uint8_t,journal_bytes>;
inline JournalBytes encodeJournal(const MissionJournal& s,bool committed) {
    JournalBytes b{};b[0]='V';b[1]='J';b[2]='R';b[3]='2';
    auto put=[&](size_t offset,uint64_t value,size_t n){protocol::put(b.data()+offset,value,n);};
    put(4,2,2);put(6,journal_bytes,2);put(8,s.generation,4);put(12,s.mission,4);
    put(16,s.elapsed_us,8);put(24,s.launch_us,8);put(32,s.petri_close_us,8);
    put(40,s.start_rtc_s,8);put(48,s.launch_rtc_s,8);put(56,s.pad_pa_x64,4);put(60,s.pad_millikelvin,4);
    put(64,uint32_t(s.target_lat_e7),4);put(68,uint32_t(s.target_lon_e7),4);put(72,s.target_revision,4);
    b[76]=s.state;b[77]=s.flags;put(80,crc32(b.data(),80),4);
    put(84,committed?0xA55A00FFu:0xFFFFFFFFu,4);return b;
}
inline bool decodeJournal(const JournalBytes& b,MissionJournal& out) {
    auto get=[&](size_t offset,size_t n){return protocol::get(b.data()+offset,n);};
    if(b[0]!='V'||b[1]!='J'||b[2]!='R'||b[3]!='2'||get(4,2)!=2||get(6,2)!=journal_bytes||
       get(84,4)!=0xA55A00FFu||get(80,4)!=crc32(b.data(),80)||b[76]>7)return false;
    MissionJournal s;s.generation=uint32_t(get(8,4));s.mission=uint32_t(get(12,4));
    s.elapsed_us=get(16,8);s.launch_us=get(24,8);s.petri_close_us=get(32,8);
    s.start_rtc_s=get(40,8);s.launch_rtc_s=get(48,8);s.pad_pa_x64=uint32_t(get(56,4));s.pad_millikelvin=uint32_t(get(60,4));
    s.target_lat_e7=int32_t(uint32_t(get(64,4)));s.target_lon_e7=int32_t(uint32_t(get(68,4)));s.target_revision=uint32_t(get(72,4));
    s.state=b[76];s.flags=b[77];
    if((s.flags&PetriDone)&&!(s.flags&PetriStarted))return false;
    if(s.mission==0||s.pad_pa_x64==0||s.pad_millikelvin<230000||s.pad_millikelvin>330000||
       std::abs(int64_t(s.target_lat_e7))>900000000LL||std::abs(int64_t(s.target_lon_e7))>1800000000LL)return false;
    out=s;return true;
}
inline bool selectJournal(const JournalBytes& a,const JournalBytes& b,MissionJournal& result) {
    MissionJournal x,y;const bool vx=decodeJournal(a,x),vy=decodeJournal(b,y);
    if(!vx&&!vy)return false;
    if(vx&&!vy){result=x;return true;}if(vy&&!vx){result=y;return true;}
    const uint32_t diff=x.generation-y.generation;
    if(diff==0x80000000u)return false; // ambiguous wrap: do not guess
    if(diff==0) {
        if(a!=b)return false; // same generation with conflicting data
        result=x;return true;
    }
    result=diff<0x80000000u?x:y;return true;
}
// Board adapter protocol: choose inactive slot/sector -> write uncommitted bytes
// 0..83 in page-safe chunks -> read/verify -> program commit word84..87 -> verify.
// Keep the previous slot until the new one is verified. NOR sectors are erased
// before flight; erase/program and NVS commits are NOT claimed nonblocking here.
} // namespace vritra
