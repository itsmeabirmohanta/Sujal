#pragma once
#include "flight_core.hpp"
namespace vritra {
enum class Phase:uint8_t {Boot=0,Test=1,Pad=2,Ascent=3,Release=4,Descent=5,Parafoil=6,Impact=7};
struct FlightInputs {
    uint64_t now_us;double agl_m,v_up_mps,ground_speed_mps;
    bool baro_valid,gnss_speed_valid,imu_stable;
};
struct FsmConfig {double lift_height_m,lift_vup_mps,land_height_m,land_vup_mps,land_speed_mps;uint64_t lift_dwell_us,land_dwell_us;uint64_t descent_dwell_us{500000};};
class FlightMachine {
    Phase phase_{Phase::Boot};bool armed_{false},released_{false},lift_seen_{false},land_seen_{false};
    uint64_t lift_start_{0},land_start_{0},last_now_{0},descent_start_{0};bool ticked_{false},descent_seen_{false};
public:
    Phase phase()const{return phase_;}bool released()const{return released_;}
    bool enterPad(bool calibration_valid,bool physical_ground_setup) {
        if(phase_!=Phase::Boot||!calibration_valid||!physical_ground_setup)return false;
        phase_=Phase::Pad;return true;
    }
    bool arm(bool all_readiness_checks) {
        if(phase_!=Phase::Pad||!all_readiness_checks)return false;
        armed_=true;return true;
    }
    bool releaseConfirmed(bool matching_session_and_transaction) {
        if(!matching_session_and_transaction||!armed_||phase_!=Phase::Ascent)return false;
        released_=true;phase_=Phase::Release;return true;
    }
    bool restore(uint8_t phase,bool armed,bool released) {
        if(phase>7||(phase>=4&&(!released||!armed))||(released&&phase<4))return false;
        phase_=Phase(phase);armed_=armed;released_=released;lift_seen_=land_seen_=ticked_=descent_seen_=false;return true;
    }
    bool step(const FlightInputs& i,const FsmConfig& c,bool bay_intent_committed) {
        if((ticked_&&i.now_us<=last_now_)||!std::isfinite(c.lift_height_m)||c.lift_height_m<0||
          !std::isfinite(c.lift_vup_mps)||c.lift_vup_mps<=0||!std::isfinite(c.land_height_m)||c.land_height_m<0||
          !std::isfinite(c.land_vup_mps)||c.land_vup_mps<=0||!std::isfinite(c.land_speed_mps)||c.land_speed_mps<=0||
          !c.lift_dwell_us||!c.land_dwell_us||!c.descent_dwell_us)return false;
        ticked_=true;last_now_=i.now_us;const Phase old=phase_;
        const bool baro=i.baro_valid&&std::isfinite(i.agl_m)&&std::isfinite(i.v_up_mps);
        if(phase_==Phase::Pad&&armed_) {
            const bool lifting=baro&&i.agl_m>c.lift_height_m&&i.v_up_mps>c.lift_vup_mps;
            if(!lifting)lift_seen_=false;
            else if(!lift_seen_){lift_seen_=true;lift_start_=i.now_us;}
            else if(i.now_us-lift_start_>=c.lift_dwell_us)phase_=Phase::Ascent;
        }
        if(phase_==Phase::Release) {
            if(!baro||i.v_up_mps>=-0.5)descent_seen_=false;
            else if(!descent_seen_){descent_seen_=true;descent_start_=i.now_us;}
            else if(i.now_us-descent_start_>=c.descent_dwell_us)phase_=Phase::Descent;
        }
        if(phase_==Phase::Descent&&bay_intent_committed)phase_=Phase::Parafoil;
        if(uint8_t(phase_)>=4&&phase_!=Phase::Impact) {
            const bool slow_gnss=!i.gnss_speed_valid||(std::isfinite(i.ground_speed_mps)&&i.ground_speed_mps<c.land_speed_mps);
            const bool landing=baro&&i.agl_m>=-c.land_height_m&&i.agl_m<c.land_height_m&&
              std::abs(i.v_up_mps)<c.land_vup_mps&&slow_gnss&&i.imu_stable;
            if(!landing)land_seen_=false;
            else if(!land_seen_){land_seen_=true;land_start_=i.now_us;}
            else if(i.now_us-land_start_>=c.land_dwell_us)phase_=Phase::Impact;
        }
        return old!=phase_;
    }
};
} // namespace vritra
