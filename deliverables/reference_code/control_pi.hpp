#pragma once
#include "flight_core.hpp"
namespace vritra {
struct PiConfig {double kp,ki,kaw,limit,slew_per_s;};
class RatePi {
    double integral_{0},applied_{0};
public:
    void reset(){integral_=applied_=0;}
    double integral()const{return integral_;}
    double update(double desired_rps,double measured_rps,double feedforward,double dt,
                  bool measurement_valid,const PiConfig& c) {
        if(!measurement_valid||!std::isfinite(desired_rps)||!std::isfinite(measured_rps)||
          !std::isfinite(feedforward)||!std::isfinite(dt)||dt<=0||dt>0.1||
          !std::isfinite(c.kp)||!std::isfinite(c.ki)||!std::isfinite(c.kaw)||
          !std::isfinite(c.limit)||!std::isfinite(c.slew_per_s)||
          c.kp<0||c.ki<0||c.kaw<0||c.limit<=0||c.limit>1||c.slew_per_s<=0){reset();return 0;}
        const double error=desired_rps-measured_rps;
        const double unsat=feedforward+c.kp*error+integral_;
        const double limited=std::clamp(unsat,-c.limit,c.limit);
        applied_+=std::clamp(limited-applied_,-c.slew_per_s*dt,c.slew_per_s*dt);
        integral_+=dt*(c.ki*error+c.kaw*(applied_-unsat));
        integral_=std::clamp(integral_,-c.limit,c.limit);return applied_;
    }
};
} // namespace vritra
