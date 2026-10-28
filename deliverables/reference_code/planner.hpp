#pragma once
#include "estimators.hpp"
namespace vritra {
enum class GuidanceMode:uint8_t {Inhibited,NavDegraded,Home,Energy,Approach,Final,Landed};
struct PlannerConfig {
    double hold_radius_m,min_turn_radius_m,hold_upwind_offset_m;
    double energy_floor_m,final_height_m,reserve_height_m,capture_radius_m;
    double min_sink_mps,max_sink_mps;uint64_t wind_max_age_us;
};
struct GuidanceDemand {bool valid{false},reachable{false},wind_valid{false},flare{false};GuidanceMode mode{GuidanceMode::Inhibited};double course_rad{NAN};};
class Planner {
    double origin_lat_{NAN},origin_lon_{NAN};bool final_latched_{false};
    static bool targetValid(const Target& t){return std::isfinite(t.lat_deg)&&std::abs(t.lat_deg)<=90&&std::isfinite(t.lon_deg)&&std::abs(t.lon_deg)<=180;}
    static bool configValid(const PlannerConfig& c) {
        const double values[]={c.hold_radius_m,c.min_turn_radius_m,c.hold_upwind_offset_m,c.energy_floor_m,
          c.final_height_m,c.reserve_height_m,c.capture_radius_m,c.min_sink_mps,c.max_sink_mps};
        for(double v:values)if(!std::isfinite(v)||v<0)return false;
        return c.min_turn_radius_m>0&&c.hold_radius_m>=c.min_turn_radius_m&&c.final_height_m>0&&
          c.energy_floor_m>c.final_height_m&&c.min_sink_mps>0&&c.max_sink_mps>c.min_sink_mps&&c.capture_radius_m>0;
    }
    void local(double lat,double lon,double& n,double& e)const {
        n=6378137*(lat-origin_lat_)*pi/180;
        e=6378137*std::cos(origin_lat_*pi/180)*wrapPi((lon-origin_lon_)*pi/180);
    }
public:
    bool setMeasuredOrigin(const NavEpoch& nav,uint64_t now,const NavGateConfig& gate) {
        if(!liveNavGate(nav,now,gate))return false;
        origin_lat_=nav.lat_deg;origin_lon_=nav.lon_deg;final_latched_=false;return true;
    }
    GuidanceDemand update(const NavEpoch& nav,const Target& target,uint64_t now,const NavGateConfig& gate,
                          const FlightPermit& permit,double h_agl,double sink,bool baro_valid,
                          const WindEstimate& wind,const PlannerConfig& c) {
        GuidanceDemand out;
        if(permit.landed){out.mode=GuidanceMode::Landed;return out;}
        if(!permit.armed||!permit.release_confirmed||!permit.secondary_commanded||!permit.canopy_stabilized)return out;
        if(!liveNavGate(nav,now,gate)){out.mode=GuidanceMode::NavDegraded;return out;}
        if(!targetValid(target)||!configValid(c)||!std::isfinite(origin_lat_)||!std::isfinite(origin_lon_)||
          !baro_valid||!std::isfinite(h_agl)||h_agl<0||!std::isfinite(sink)||sink<c.min_sink_mps||sink>c.max_sink_mps)return out;
        double pn,pe,tn,te;local(nav.lat_deg,nav.lon_deg,pn,pe);local(target.lat_deg,target.lon_deg,tn,te);
        const double dn=tn-pn,de=te-pe,distance=std::hypot(dn,de);
        const double tgo=h_agl/sink;
        out.wind_valid=wind.valid&&fresh(now,wind.fitted_us,c.wind_max_age_us)&&std::isfinite(wind.north_mps)&&
          std::isfinite(wind.east_mps)&&std::isfinite(wind.airspeed_mps)&&wind.airspeed_mps>0;
        out.mode=GuidanceMode::Home;double aim_n=tn,aim_e=te;
        if(out.wind_valid) {
            const double speed=std::hypot(wind.north_mps,wind.east_mps);
            out.reachable=std::hypot(dn-wind.north_mps*tgo,de-wind.east_mps*tgo)<=wind.airspeed_mps*tgo;
            if(speed>0.2&&wind.airspeed_mps>speed) {
                const double wn=wind.north_mps/speed,we=wind.east_mps/speed;
                const double final_distance=(wind.airspeed_mps-speed)*c.final_height_m/sink;
                const double entry_n=tn+wn*final_distance,entry_e=te+we*final_distance;
                if(h_agl<=c.final_height_m)final_latched_=true;
                if(final_latched_) {out.mode=GuidanceMode::Final;aim_n=tn;aim_e=te;}
                else {
                    const double distance_to_entry=std::hypot(entry_n-pn,entry_e-pe);
                    const double required_height=sink*distance_to_entry/(wind.airspeed_mps-speed)+c.final_height_m+c.reserve_height_m;
                    if(out.reachable&&h_agl>std::max(c.energy_floor_m,required_height)) {
                        const double hn=tn-wn*c.hold_upwind_offset_m,he=te-we*c.hold_upwind_offset_m;
                        const double rn=pn-hn,re=pe-he,radius=std::hypot(rn,re);
                        if(radius>c.capture_radius_m) {
                            const double radial_n=rn/radius,radial_e=re/radius;
                            const double correction=(radius-c.hold_radius_m)/c.hold_radius_m;
                            // Clockwise tangent plus radial correction; requires a qualified turn envelope.
                            const double direction_n=-radial_e-correction*radial_n,direction_e=radial_n-correction*radial_e;
                            out.course_rad=std::atan2(direction_e,direction_n);out.mode=GuidanceMode::Energy;
                        }
                    } else {aim_n=entry_n;aim_e=entry_e;out.mode=GuidanceMode::Approach;}
                }
            }
        }
        // Unknown wind remains unknown; direct homing is still grounded in a live fix.
        if(out.mode!=GuidanceMode::Energy) {
            if(distance<c.capture_radius_m){out.valid=false;return out;}
            out.course_rad=std::atan2(aim_e-pe,aim_n-pn);
        }
        out.valid=std::isfinite(out.course_rad);out.flare=false;return out;
    }
};
} // namespace vritra
