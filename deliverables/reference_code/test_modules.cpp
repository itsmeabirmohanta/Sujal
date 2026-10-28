// Offline-only test data. Never compile this translation unit into firmware.
#include "pins.hpp"
#include "protocol.hpp"
#include "journal.hpp"
#include "logging.hpp"
#include "drivers.hpp"
#include "scheduler.hpp"
#include "telemetry.hpp"
#include "fsm.hpp"
#include "planner.hpp"
#include "control_pi.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace vritra;
int main() {
    protocol::Frame f;f.source=1;f.type=1;f.session=7;f.sequence=9;f.length=64;
    for(size_t i=0;i<f.length;++i)f.payload[i]=uint8_t(i);
    std::array<uint8_t,protocol::wire_cap> wire{};
    const auto n=protocol::encode(f,wire.data(),wire.size());
    assert(n<=92&&n>0);protocol::Frame decoded;
    assert(protocol::decode(wire.data(),n,decoded)&&decoded.payload==f.payload&&decoded.sequence==9);
    for(size_t i=0;i<n;++i){auto damaged=wire;damaged[i]^=0x80;assert(!protocol::decode(damaged.data(),n,decoded));}
    protocol::Receiver receiver;for(size_t i=0;i<200;++i)assert(!receiver.byte(1,decoded));
    assert(!receiver.byte(0,decoded));bool accepted=false;
    for(size_t i=0;i<n;++i)accepted=receiver.byte(wire[i],decoded);
    assert(accepted);f.length=65;assert(!protocol::encode(f,wire.data(),wire.size()));
    f.length=0;f.source=3;assert(!protocol::encode(f,wire.data(),wire.size()));

    MissionJournal j;j.mission=1;j.pad_pa_x64=6400000;j.pad_millikelvin=293150;
    auto a=encodeJournal(j,true);assert(decodeJournal(a,j));
    assert(!decodeJournal(encodeJournal(j,false),j));
    for(size_t i=0;i<journal_bytes;++i){auto bad=a;bad[i]^=1;assert(!decodeJournal(bad,j));}
    j.generation=1;auto b=encodeJournal(j,true);MissionJournal chosen;
    assert(selectJournal(a,b,chosen)&&chosen.generation==1);
    b[20]^=1;assert(selectJournal(a,b,chosen)&&chosen.generation==0);
    j.generation=0;j.elapsed_us=1;assert(!selectJournal(a,encodeJournal(j,true),chosen));

    SpscQueue<int,3> queue;int v=0;assert(queue.push(1)&&queue.push(2)&&!queue.push(3));
    assert(queue.pop(v)&&v==1);assert(queue.push(3));assert(queue.pop(v)&&v==2);assert(queue.pop(v)&&v==3);assert(!queue.pop(v));
    LogRecord log;log.source=4;log.type=2;log.length=64;log.sample_us=123;
    std::array<uint8_t,log_record_max> bytes{};auto ln=encodeLog(log,bytes.data(),bytes.size());
    assert(ln==104);LogRecord recovered;assert(decodeLog(bytes.data(),ln,recovered)&&recovered.sample_us==123);
    bytes[50]^=1;assert(!decodeLog(bytes.data(),ln,recovered));

    AxisKalman axis;assert(!axis.predict(.02,1));assert(axis.initialize(0,0,25,.25));
    assert(axis.predict(.02,1)&&axis.measure(1,25,1));
    assert(axis.measure(.1,.25,1,true)); // Same GNSS epoch must update BOTH observables.
    assert(!axis.measure(1,25,1)&&!axis.measure(.1,.25,1,true));
    assert(!axis.measure(100000,1,2));assert(axis.finite());
    for(uint64_t i=2;i<1000;++i){assert(axis.predict(.02,1));assert(axis.measure(1,25,i));assert(axis.finite());}

    const NavGateConfig gate{500000,2500000,2.5,6,50};
    NavEpoch nav;nav.origin=Origin::LiveUart;nav.coherent=true;nav.fix_quality=1;nav.fix_dimension=3;
    nav.satellites=8;nav.hdop=1;nav.lat_deg=26;nav.lon_deg=84;nav.msl_m=100;
    nav.north_mps=5;nav.east_mps=0;nav.gga_rx_us=nav.rmc_rx_us=nav.quality_rx_us=1000000;
    WindCircle<> fit;
    for(uint64_t i=0;i<24;++i){nav.epoch_key=i+1;nav.north_mps=2+5*std::cos(2*pi*i/24);nav.east_mps=-1+5*std::sin(2*pi*i/24);assert(fit.add(nav,1000000,gate,true));assert(!fit.add(nav,1000000,gate,true));}
    auto wind=fit.fit(1000000,12,2*pi/3,.1,1,10,500000);
    assert(wind.valid&&std::abs(wind.north_mps-2)<1e-8&&std::abs(wind.east_mps+1)<1e-8&&std::abs(wind.airspeed_mps-5)<1e-8);
    assert(!fit.fit(2000000,12,2*pi/3,.1,1,10,500000).valid);
    nav.epoch_key=25;nav.gga_rx_us=nav.rmc_rx_us=nav.quality_rx_us=2000000;
    assert(fit.add(nav,2000000,gate,true));
    assert(!fit.fit(2000000,12,2*pi/3,.1,1,10,500000).valid); // A fresh tail cannot rejuvenate an old fitting window.
    nav.gga_rx_us=nav.rmc_rx_us=nav.quality_rx_us=1000000;
    Planner planner;assert(planner.setMeasuredOrigin(nav,1000000,gate));
    PlannerConfig pc{30,20,50,200,50,20,5,1,8,500000};
    FlightPermit permit{true,true,true,true,false};Target target{26.001,84.001,1};
    auto demand=planner.update(nav,target,1000000,gate,permit,100,2,true,{},pc);
    assert(demand.valid&&demand.mode==GuidanceMode::Home&&!demand.wind_valid&&!demand.flare);
    assert(planner.update(nav,target,2000000,gate,permit,100,2,true,wind,pc).mode==GuidanceMode::NavDegraded);
    permit.release_confirmed=false;assert(!planner.update(nav,target,1000000,gate,permit,100,2,true,wind,pc).valid);

    RatePi pi_control;PiConfig pi_cfg{.5,.2,1,.25,.5};
    for(int i=0;i<1000;++i)assert(std::abs(pi_control.update(100,0,0,.02,true,pi_cfg))<=.25);
    assert(std::abs(pi_control.integral())<=.25);assert(pi_control.update(0,0,0,.02,false,pi_cfg)==0);

    FlightMachine state;FsmConfig fc{10,.5,5,.3,.5,2000000,10000000,500000};
    assert(!state.releaseConfirmed(true));assert(state.enterPad(true,true)&&state.arm(true));
    FlightInputs in{1,20,1,0,true,true,true};state.step(in,fc,false);
    in.now_us=2000001;assert(state.step(in,fc,false)&&state.phase()==Phase::Ascent);
    assert(state.releaseConfirmed(true));in.v_up_mps=-1;in.now_us+=1;state.step(in,fc,false);
    in.now_us+=499999;assert(!state.step(in,fc,false)&&state.phase()==Phase::Release);
    in.now_us+=1;assert(state.step(in,fc,false)&&state.phase()==Phase::Descent);
    in.now_us+=1;assert(state.step(in,fc,true)&&state.phase()==Phase::Parafoil);
    assert(!state.restore(6,false,true)&&!state.restore(2,true,true));
    in.agl_m=0;in.v_up_mps=0;in.now_us+=1;state.step(in,fc,true);
    in.now_us+=10000000;assert(state.step(in,fc,true)&&state.phase()==Phase::Impact);

    TelemetrySnapshot snapshot;snapshot.gnss=nav;snapshot.time_valid=true;snapshot.state=6;
    snapshot.pressure={100000,1000000,true};TelemetryTtl ttl{100000,100000,100000,100000,100000};
    char csv[256];auto c=formatCsv(snapshot,1000000,gate,ttl,csv,sizeof(csv));
    assert(c.ok&&std::count(csv,csv+c.bytes,',')==16&&csv[c.bytes-1]=='\r');
    c=formatCsv(snapshot,2000000,gate,ttl,csv,sizeof(csv));assert(c.ok);
    assert(std::string(csv).find("26.000000")==std::string::npos&&std::string(csv).find("100000")==std::string::npos);
    assert(!formatCsv(snapshot,1000000,gate,ttl,csv,8).ok&&csv[0]=='\0');

    uint8_t hdc[6]={0x80,0,0,0x80,0,0};hdc[2]=drivers::crc8(hdc,2);hdc[5]=drivers::crc8(hdc+3,2);
    struct Bus:drivers::I2cBootIo {drivers::IoResult write(uint8_t,uint8_t,uint8_t,uint8_t)override{return {drivers::IoResult::Ok};}} bus;
    const std::array<drivers::RegWrite,1> boot{{{0x47,0,0x7E,0xB6,2000}}};drivers::BootWriter<1> bootWriter(boot);
    assert(bootWriter.step(0,bus).code==drivers::IoResult::Busy);
    assert(bootWriter.step(1999,bus).code==drivers::IoResult::Busy);
    assert(bootWriter.step(2000,bus).code==drivers::IoResult::Ok);
    double temp,rh;assert(drivers::hdc3020(hdc,temp,rh)&&rh>50&&rh<50.01);
    hdc[0]^=1;assert(!drivers::hdc3020(hdc,temp,rh));
    uint8_t pressure[6]={0,0,25,0,0,0};assert(!drivers::bmp585(pressure,rh,temp));
    assert(drivers::s24(std::array<uint8_t,3>{0,0,0xFF}.data())==-65536);
    int calls=0;ScheduledJob job{100,0,[](void* p,uint64_t){++*static_cast<int*>(p);},&calls};
    assert(job.poll(0)&&!job.poll(99)&&job.poll(500)&&calls==2&&job.deadline_misses==1);
    ProgressWatchdog wd(3,100,0);wd.advanced(1);assert(!wd.mayFeed(1));wd.advanced(2);assert(wd.mayFeed(2));wd.advanced(3);assert(!wd.mayFeed(102));
    std::cout<<"PASS: module integration, corruption, expiry, estimator, control, FSM and storage checks\n";
}
