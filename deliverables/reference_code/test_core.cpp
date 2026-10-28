#include "flight_core.hpp"
#include "nmea_epoch.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace vritra;
static std::string nmea(const std::string& body) {
    unsigned sum=0;for(unsigned char c:body) sum^=c;
    constexpr char hex[]="0123456789ABCDEF";
    std::string s="$"+body+"*";s+=hex[sum>>4];s+=hex[sum&15];return s;
}
static std::string gga(const std::string& time="120000.000",const std::string& quality="1") {
    return nmea("GNGGA,"+time+",2643.052100,N,08418.092580,E,"+quality+",12,0.9,100.0,M,-30.0,M,,");
}
static std::string rmc(const std::string& time="120000.000",const std::string& status="A") {
    return nmea("GNRMC,"+time+","+status+",2643.052100,N,08418.092580,E,5.0,0.0,281026,,,A");
}
static std::string gsa() { return nmea("GNGSA,A,3,01,02,03,04,05,06,,,,,,,1.2,0.9,1.0"); }
static constexpr NavGateConfig gate{500000,2500000,2.5,6,50};
static NmeaEpochParser acquired() {
    NmeaEpochParser p;p.sentence(gsa(),1000000);p.sentence(gga(),1010000);
    assert(p.sentence(rmc(),1020000));assert(liveNavGate(p.latest(),1030000,gate));return p;
}
int main() {
    unsigned checks=0;
    auto p=acquired();auto n=p.latest();++checks;
    assert(std::abs(n.lat_deg-26.717535)<1e-8 && std::abs(n.lon_deg-84.301543)<1e-8);
    assert(std::abs(n.north_mps-2.572222222222)<1e-8 && std::abs(n.east_mps)<1e-8);++checks;
    for (Origin o:{Origin::None,Origin::Replay,Origin::Synthetic,Origin::Default}) {
        auto bad=n;bad.origin=o;assert(!liveNavGate(bad,1030000,gate));++checks;
    }
    assert(!liveNavGate(n,1600000,gate));assert(!liveNavGate(n,1000000,gate));++checks;
    const auto stamp=p.latest().gga_rx_us;
    assert(!p.sentence(gga(),1400000));assert(!p.sentence(rmc(),1410000));
    assert(p.latest().gga_rx_us==stamp);assert(!liveNavGate(p.latest(),1600000,gate));++checks;
    {
        auto q=acquired();q.sentence(rmc("120000.200","V"),1100000);
        assert(!q.latest().coherent);++checks;
    }
    {
        auto q=acquired();q.sentence(gga("120000.200","0"),1100000);
        assert(!q.latest().coherent);++checks;
    }
    {
        NmeaEpochParser q;q.sentence(gsa(),1000000);q.sentence(gga(),1010000);
        assert(!q.sentence(rmc("120001.000"),1020000));assert(!q.latest().coherent);++checks;
    }
    for (const auto& body:{
        "GNGGA,120001.000,2699.000,N,08418.000,E,1,12,0.9,100.0,M,-30.0,M,,",
        "GNGGA,120001.000,2643.000,X,08418.000,E,1,12,0.9,100.0,M,-30.0,M,,",
        "GNGGA,120001.000,2643.000,N,08418.000,E,1,12,nan,100.0,M,-30.0,M,,",
        "GNGGA,240000.000,2643.000,N,08418.000,E,1,12,0.9,100.0,M,-30.0,M,,"
    }) {
        NmeaEpochParser q;q.sentence(gsa(),1000000);assert(!q.sentence(nmea(body),1010000));
        assert(!q.latest().coherent);++checks;
    }
    {
        auto q=acquired();auto s=gga("120001.000");s[s.size()-1]='Z';
        assert(!q.sentence(s,1100000));assert(q.latest().epoch_key==n.epoch_key);++checks;
        for(int i=0;i<1000;++i) q.byte('A',1110000);
        q.byte('$',1110000);for(int i=0;i<1000;++i) q.byte('A',1110000);
        q.byte('\n',1110000);assert(q.latest().epoch_key==n.epoch_key);++checks;
    }
    {
        NmeaEpochParser q;q.sentence(gsa(),1000000);
        q.sentence(nmea("GNGGA,120000.000,0000.000000,N,00000.000000,E,1,12,0.9,0.0,M,0.0,M,,"),1010000);
        assert(q.sentence(nmea("GNRMC,120000.000,A,0000.000000,N,00000.000000,E,5.0,0.0,281026,,,A"),1020000));
        assert(liveNavGate(q.latest(),1030000,gate));++checks; // zero is a legitimate measurement
    }
    {
        auto q=acquired();q.sentence(gga("120000.200","6"),1100000);q.sentence(rmc("120000.200"),1110000);
        assert(!liveNavGate(q.latest(),1120000,gate));++checks;
    }
    {
        auto q=acquired();q.sentence(gga("120000.000","6"),1100000);
        assert(!q.latest().coherent);++checks; // explicit degradation invalidates even a duplicate epoch
    }
    {
        auto q=acquired();q.sentence(nmea("GNGSA,A,2,01,02,03,04,05,06,,,,,,,1.2,0.9,1.0"),1100000);
        assert(!q.latest().coherent);++checks;
    }
    {
        auto q=acquired();q.sentence(gga("120001.000"),4000000);q.sentence(rmc("120001.000"),4010000);
        assert(q.latest().epoch_key==n.epoch_key);assert(!liveNavGate(q.latest(),4020000,gate));++checks;
    }
    FlightPermit permit{true,true,true,true,false};
    ControlConfig config{0.25,0.2,0.25,1.5};CourseController controller;
    Target target{n.lat_deg,n.lon_deg+0.001,1};
    auto out=controller.update(n,target,1030000,gate,permit,config,0.02);
    assert(out.guided && out.brake_diff>0 && out.brake_diff<=0.00500001);++checks;
    permit.release_confirmed=false;
    assert(!controller.update(n,target,1030000,gate,permit,config,0.02).guided);++checks;
    permit.release_confirmed=true;
    auto stale=controller.update(n,target,1600000,gate,permit,config,0.02);
    assert(!stale.guided && stale.brake_diff==0);++checks;
    assert(!controller.update(n,target,1030000,gate,permit,config,NAN).guided);++checks;
    TargetArea area{26,27,84,85};TargetTransaction tx;
    tx.stage(42,true,267175350);assert(!tx.commit(42,true,area));
    tx.stage(42,false,843015430);assert(tx.commit(42,true,area));
    assert(tx.active.revision==1 && tx.commit(42,true,area) && tx.active.revision==1);++checks;
    tx.stage(43,true,267175350);tx.stage(44,false,843015430);
    assert(!tx.commit(44,true,area));assert(tx.active.revision==1);++checks;
    tx.stage(45,true,267175350);tx.stage(45,false,843015430);
    assert(!tx.commit(45,false,area));++checks;
    uint16_t pulse=0;ServoCalibration calibration{1520,1100,1000,2000};
    assert(pulseForBrake(0.5,calibration,pulse) && pulse==1310);
    assert(!pulseForBrake(NAN,calibration,pulse));assert(!pulseForBrake(1.1,calibration,pulse));++checks;
    Events events;
    events.step(960,-20,true,false,true,true,1000000,0);
    assert(!events.step(940,-20,true,false,true,true,2000000,0).petri_open);++checks;
    events.step(960,-20,true,true,true,true,3000000,0);
    assert(events.step(949,-20,true,true,true,true,4000000,0).petri_open);++checks;
    auto saved=events.state;Events restored;restored.restore(saved);
    assert(!restored.step(949,-20,true,true,true,true,5000000,0).petri_open);
    assert(!restored.step(948,-20,true,true,true,true,13999999,0).petri_close);
    assert(restored.step(947,-20,true,true,true,true,14000000,0).petri_close);++checks;
    assert(!restored.step(946,-20,true,true,true,true,15000000,0).petri_close);++checks;
    restored.step(610,-20,true,true,true,true,20000000,0);
    assert(restored.step(599,-20,true,true,true,true,21000000,0).bay_open);
    assert(!restored.step(598,-20,true,true,true,true,22000000,0).bay_open);++checks;
    Events invalid_time;invalid_time.restore(saved);
    assert(invalid_time.step(NAN,NAN,false,false,false,false,0,0).petri_close);++checks;
    TxDeadline radio;assert(!radio.allowed(0,true));radio.request(true);assert(radio.allowed(0,true));
    radio.launch(1000000);assert(radio.allowed(5400999999ULL,true));
    assert(!radio.allowed(5401000000ULL,true));radio.request(true);
    assert(!radio.allowed(5401000001ULL,true));assert(radio.cutoff());++checks;
    TxDeadline restored_radio;restored_radio.restore(true,1000000,true);restored_radio.request(true);
    assert(!restored_radio.allowed(5401000001ULL,true));++checks;
    const char* vector="123456789";
    assert(crc32(reinterpret_cast<const uint8_t*>(vector),9)==0xCBF43926);++checks;
    assert(std::abs(altitudeAgl(100000,100000,293.15))<1e-10);
    assert(std::isnan(altitudeAgl(-1,100000,293.15)));++checks;
    assert(std::abs(wrapPi(-179*pi/180-179*pi/180)-2*pi/180)<1e-10);++checks;
    std::cout<<"PASS: "<<checks<<" grouped assertions; offline synthetic inputs only\n";
}
