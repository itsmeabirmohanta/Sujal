#pragma once
// Engineering reference, not an integrated or flight-qualified firmware.
// D tuning values are deliberately supplied by callers, not hidden defaults.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace vritra {
constexpr double pi = 3.14159265358979323846;
constexpr double g0 = 9.80665;
inline double wrapPi(double x) {
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    return std::remainder(x, 2.0 * pi);
}
enum class Origin : uint8_t { None, LiveUart, Replay, Synthetic, Default };

struct NavEpoch {
    Origin origin{Origin::None};
    bool coherent{false};
    uint8_t fix_quality{0}, fix_dimension{0}, satellites{0};
    double lat_deg{NAN}, lon_deg{NAN}, msl_m{NAN};
    double north_mps{NAN}, east_mps{NAN}, hdop{NAN};
    uint64_t epoch_key{0};
    uint64_t gga_rx_us{0}, rmc_rx_us{0}, quality_rx_us{0};
};
struct NavGateConfig {
    uint64_t fix_max_age_us;
    uint64_t quality_max_age_us;
    double max_hdop;
    uint8_t min_satellites;
    double max_ground_speed_mps;
};
inline bool fresh(uint64_t now, uint64_t stamp, uint64_t max_age) {
    return now >= stamp && now - stamp <= max_age;
}
inline bool liveNavGate(const NavEpoch& n, uint64_t now, const NavGateConfig& c) {
    // Quality 6 (estimated/DR), manual and simulation fixes are excluded.
    const bool measured_quality = n.fix_quality == 1 || n.fix_quality == 2 ||
                                  n.fix_quality == 4 || n.fix_quality == 5;
    return n.origin == Origin::LiveUart && n.coherent && measured_quality &&
           n.fix_dimension == 3 && n.satellites >= c.min_satellites &&
           std::isfinite(n.hdop) && n.hdop > 0 && n.hdop <= c.max_hdop &&
           std::isfinite(n.lat_deg) && std::abs(n.lat_deg) <= 90 &&
           std::isfinite(n.lon_deg) && std::abs(n.lon_deg) <= 180 &&
           std::isfinite(n.msl_m) && std::isfinite(n.north_mps) &&
           std::isfinite(n.east_mps) &&
           std::hypot(n.north_mps,n.east_mps) <= c.max_ground_speed_mps &&
           fresh(now,n.gga_rx_us,c.fix_max_age_us) &&
           fresh(now,n.rmc_rx_us,c.fix_max_age_us) &&
           fresh(now,n.quality_rx_us,c.quality_max_age_us);
}

struct Target { double lat_deg{NAN}, lon_deg{NAN}; uint32_t revision{0}; };
struct TargetArea { double min_lat,max_lat,min_lon,max_lon; };
class TargetTransaction {
    uint32_t pending_id_{0}, committed_id_{0};
    int32_t lat_{0},lon_{0};
    bool have_lat_{false},have_lon_{false},have_commit_{false};
public:
    Target active;
    void stage(uint32_t id, bool latitude, int32_t e7) {
        if (!have_lat_ && !have_lon_) pending_id_=id;
        if (pending_id_!=id) { pending_id_=id;have_lat_=have_lon_=false; }
        if (latitude) { lat_=e7;have_lat_=true; }
        else { lon_=e7;have_lon_=true; }
    }
    bool commit(uint32_t id, bool phase_allowed, const TargetArea& a) {
        if (have_commit_ && id==committed_id_) return true; // idempotent ACK
        if (!phase_allowed || pending_id_!=id || !have_lat_ || !have_lon_) return false;
        const double lat=lat_*1e-7,lon=lon_*1e-7;
        if (!std::isfinite(a.min_lat) || !std::isfinite(a.max_lat) ||
            !std::isfinite(a.min_lon) || !std::isfinite(a.max_lon) ||
            a.min_lat>a.max_lat || a.min_lon>a.max_lon ||
            std::abs(lat)>90 || std::abs(lon)>180 || lat<a.min_lat || lat>a.max_lat ||
            lon<a.min_lon || lon>a.max_lon) return false;
        active={lat,lon,active.revision+1};committed_id_=id;have_commit_=true;
        have_lat_=have_lon_=false;return true;
    }
    // Session/mission ID, sequence-window validation and CRC are enforced by
    // the transport BEFORE stage/commit. This class is not authentication.
};

struct FlightPermit {
    bool armed{false},release_confirmed{false},secondary_commanded{false};
    bool canopy_stabilized{false},landed{false};
};
struct ControlConfig {
    double gain_per_rad, max_diff, slew_per_s, min_course_speed_mps;
};
struct ControlOutput { bool guided{false}; double brake_diff{0}; };
class CourseController {
    double applied_{0};
public:
    void reset() { applied_=0; }
    ControlOutput update(const NavEpoch& n, const Target& t, uint64_t now,
                         const NavGateConfig& gate, const FlightPermit& permit,
                         const ControlConfig& c, double dt_s) {
        const bool valid_config = std::isfinite(c.gain_per_rad) && c.gain_per_rad>=0 &&
            std::isfinite(c.max_diff) && c.max_diff>=0 && c.max_diff<=1 &&
            std::isfinite(c.slew_per_s) && c.slew_per_s>0 &&
            std::isfinite(c.min_course_speed_mps) && c.min_course_speed_mps>0;
        if (!valid_config || !std::isfinite(dt_s) || dt_s<=0 || dt_s>0.1 ||
            !permit.armed || !permit.release_confirmed || !permit.secondary_commanded ||
            !permit.canopy_stabilized || permit.landed || !liveNavGate(n,now,gate) ||
            !std::isfinite(t.lat_deg) || std::abs(t.lat_deg)>90 ||
            !std::isfinite(t.lon_deg) || std::abs(t.lon_deg)>180) {
            reset();return {}; // hardware adapter slews to measured glide trim
        }
        if (std::hypot(n.north_mps,n.east_mps)<c.min_course_speed_mps) {
            reset();return {};
        }
        // Spherical initial bearing avoids longitude discontinuity at +/-180.
        const double p=n.lat_deg*pi/180, q=t.lat_deg*pi/180;
        const double dl=wrapPi((t.lon_deg-n.lon_deg)*pi/180);
        const double y=std::sin(dl)*std::cos(q);
        const double x=std::cos(p)*std::sin(q)-std::sin(p)*std::cos(q)*std::cos(dl);
        if (std::hypot(x,y)<1e-12) { reset();return {}; } // target reached/direction undefined
        const double desired=std::atan2(y,x);
        const double measured=std::atan2(n.east_mps,n.north_mps);
        const double error=wrapPi(desired-measured);
        const double wanted=std::clamp(c.gain_per_rad*error,-c.max_diff,c.max_diff);
        applied_+=std::clamp(wanted-applied_,-c.slew_per_s*dt_s,c.slew_per_s*dt_s);
        return {true,applied_};
    }
};

struct ServoCalibration {
    uint16_t released_us, max_pull_us;
    uint16_t electrical_min_us,electrical_max_us;
};
inline bool pulseForBrake(double brake, const ServoCalibration& c, uint16_t& pulse) {
    if (!std::isfinite(brake) || brake<0 || brake>1 ||
        c.electrical_min_us>=c.electrical_max_us ||
        c.released_us<c.electrical_min_us || c.released_us>c.electrical_max_us ||
        c.max_pull_us<c.electrical_min_us || c.max_pull_us>c.electrical_max_us) return false;
    const double result=c.released_us+brake*(static_cast<int>(c.max_pull_us)-c.released_us);
    pulse=static_cast<uint16_t>(std::lround(result));return true;
}
// Final pulse slew, measured safe common brake and left/right sign mapping are
// intentionally board calibration, not assumed generic servo travel.

struct EventState {
    bool petri_started{false},petri_done{false},bay_commanded{false};
    uint64_t petri_close_mission_us{0};
};
struct EventActions { bool petri_open{false},petri_close{false},bay_open{false}; };
class Events {
    double previous_h_{NAN};
public:
    EventState state;
    void restore(EventState s) { state=s;previous_h_=NAN; }
    EventActions step(double h_m, double v_up_mps, bool baro_valid,
                      bool release_confirmed, bool armed, bool time_trusted,
                      uint64_t mission_us, double bay_latency_s) {
        EventActions out;
        if (state.petri_started && !state.petri_done &&
            (!time_trusted || mission_us>=state.petri_close_mission_us)) {
            state.petri_done=true;out.petri_close=true;
        }
        const bool good=baro_valid && std::isfinite(h_m) && std::isfinite(v_up_mps);
        if (!good || !release_confirmed || !armed) { previous_h_=NAN;return out; }
        if (v_up_mps< -0.5 && std::isfinite(previous_h_)) {
            if (!state.petri_started && time_trusted && previous_h_>950 && h_m<=950) {
                state.petri_started=true;state.petri_close_mission_us=mission_us+10000000ULL;
                out.petri_open=true;
            }
            // For actual flight track previous predicted height using the
            // same latency model; this reference fixes latency during the event.
            if (std::isfinite(bay_latency_s) && bay_latency_s>=0 && bay_latency_s<=2) {
                const double threshold=600-v_up_mps*bay_latency_s;
                if (!state.bay_commanded && previous_h_>threshold && h_m<=threshold) {
                    state.bay_commanded=true;out.bay_open=true;
                }
            }
        }
        previous_h_=h_m;return out;
    }
    // Persist state INTENT before executing actions; after reset reconcile
    // absolute physical position/deadline. This is not a transactional actuator.
    // Initial boot below an event threshold needs a distinct, reviewed recovery
    // policy; it must not silently masquerade as an observed crossing.
};

class TxDeadline {
    bool launched_{false},cutoff_{false},requested_{false};
    uint64_t launch_us_{0};
public:
    void restore(bool launched, uint64_t launch_us, bool cutoff) {
        launched_=launched;launch_us_=launch_us;cutoff_=cutoff;requested_=false;
    }
    void launch(uint64_t mission_us) { if (!launched_) { launched_=true;launch_us_=mission_us; } }
    void request(bool on) { requested_=on; } // NEVER clears deadline or launch
    bool allowed(uint64_t mission_us, bool time_trusted) {
        if (!time_trusted) return false;
        if (launched_ && (mission_us<launch_us_ || mission_us-launch_us_>=5400000000ULL)) cutoff_=true;
        return requested_ && !cutoff_;
    }
    bool cutoff() const { return cutoff_; }
};

inline uint32_t crc32(const uint8_t* data,size_t n) {
    uint32_t crc=0xFFFFFFFFu;
    for (size_t i=0;i<n;++i) {
        crc^=data[i];
        for (int j=0;j<8;++j) crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);
    }
    return crc^0xFFFFFFFFu;
}
inline double altitudeAgl(double pa,double pad_pa,double pad_kelvin) {
    if (!std::isfinite(pa) || !std::isfinite(pad_pa) || !std::isfinite(pad_kelvin) ||
        pa<=0 || pad_pa<=0 || pad_kelvin<230 || pad_kelvin>330) return NAN;
    return pad_kelvin/0.0065*(1-std::pow(pa/pad_pa,287.05*0.0065/g0));
}
} // namespace vritra
