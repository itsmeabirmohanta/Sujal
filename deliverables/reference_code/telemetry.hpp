#pragma once
#include "flight_core.hpp"
#include <cstdio>
#include <cstring>
namespace vritra {
constexpr const char* team_id="2026-INSPACe-CAN-7USAT-025";
constexpr const char* csv_header="TEAM_ID,TIME_S,PKT,ALT_M,PRES_PA,TEMP_C,BUS_V,GNSS_TIME,LAT,LON,GPS_ALT_M,SATS,ACC_X,ACC_Y,ACC_Z,SPIN_RATE,STATE";
struct Measurement {double value{NAN};uint64_t sample_us{0};bool valid{false};};
struct TelemetrySnapshot {
    uint64_t mission_us{0};uint32_t packet{0};uint8_t state{0};bool time_valid{false};
    Measurement altitude,pressure,temperature,voltage,ax,ay,az,spin;
    NavEpoch gnss;
};
struct TelemetryTtl {uint64_t baro,temperature,power,imu,hall;};
struct CsvResult {bool ok{false},fits_command_slot{false};size_t bytes{0};};
inline CsvResult formatCsv(const TelemetrySnapshot& s,uint64_t now,const NavGateConfig& gate,
                          const TelemetryTtl& ttl,char* out,size_t capacity,size_t command_slot_limit=130) {
    if(!out||capacity<2||s.state>7)return {};
    size_t used=0;bool ok=true;
    auto append=[&](const char* text){const size_t n=std::strlen(text);if(used+n>=capacity){ok=false;return;}std::memcpy(out+used,text,n);used+=n;out[used]='\0';};
    auto comma=[&](){append(",");};
    auto num=[&](double v,int digits){char b[48];if(!std::isfinite(v))return;const int n=std::snprintf(b,sizeof(b),"%.*f",digits,v);if(n<0||size_t(n)>=sizeof(b)){ok=false;return;}append(b);};
    auto integer=[&](uint64_t v){char b[32];const int n=std::snprintf(b,sizeof(b),"%llu",static_cast<unsigned long long>(v));if(n<0||size_t(n)>=sizeof(b)){ok=false;return;}append(b);};
    auto measured=[&](const Measurement& m,uint64_t age,int digits){if(m.valid&&fresh(now,m.sample_us,age))num(m.value,digits);};
    append(team_id);comma();if(s.time_valid)integer(s.mission_us/1000000);comma();integer(s.packet);comma();
    measured(s.altitude,ttl.baro,1);comma();measured(s.pressure,ttl.baro,0);comma();
    measured(s.temperature,ttl.temperature,1);comma();measured(s.voltage,ttl.power,2);comma();
    const bool gnss_ok=liveNavGate(s.gnss,now,gate);
    if(gnss_ok){integer((s.gnss.epoch_key%86400000ULL)/1000);}comma();
    if(gnss_ok){num(s.gnss.lat_deg,6);}comma();if(gnss_ok){num(s.gnss.lon_deg,6);}comma();
    if(gnss_ok){num(s.gnss.msl_m,1);}comma();if(gnss_ok){integer(s.gnss.satellites);}comma();
    measured(s.ax,ttl.imu,1);comma();measured(s.ay,ttl.imu,1);comma();measured(s.az,ttl.imu,1);comma();
    measured(s.spin,ttl.hall,0);comma();integer(s.state);append("\r");
    if(!ok){out[0]='\0';return {};}
    return {true,used<=command_slot_limit,used};
}
// A measured outlier is NOT clamped to fit the airtime slot. If the valid CSV
// exceeds its reserved size, the RF scheduler must shorten/omit that slot's
// optional uplink window while continuing 1Hz core transmission, and flag the
// overrun. A <=1s waveform is still required; no claim for arbitrary huge values.
} // namespace vritra
