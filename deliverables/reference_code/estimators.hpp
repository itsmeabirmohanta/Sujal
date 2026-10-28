#pragma once
#include "flight_core.hpp"
#include <array>
namespace vritra {
// Two-state scalar-measurement Kalman filter. Units are metres and m/s.
class AxisKalman {
    bool initialized_{false};std::array<uint64_t,2> last_measurement_{};std::array<bool,2> have_measurement_{};
public:
    double position{NAN},velocity{NAN},p00{0},p01{0},p11{0};
    bool initialize(double p,double v,double pos_var,double vel_var) {
        if(!std::isfinite(p)||!std::isfinite(v)||!std::isfinite(pos_var)||pos_var<=0||
           !std::isfinite(vel_var)||vel_var<=0)return false;
        position=p;velocity=v;p00=pos_var;p11=vel_var;p01=0;initialized_=true;have_measurement_={false,false};return true;
    }
    bool predict(double dt,double accel_sigma) {
        if(!initialized_||!std::isfinite(dt)||dt<=0||dt>1||!std::isfinite(accel_sigma)||accel_sigma<0)return false;
        const double q=accel_sigma*accel_sigma,d2=dt*dt;
        position+=velocity*dt;
        p00+=2*dt*p01+d2*p11+q*d2*d2/4;
        p01+=dt*p11+q*d2*dt/2;p11+=q*d2;return finite();
    }
    bool measure(double z,double variance,uint64_t measurement_id,bool velocity_measurement=false,double gate_sigma=5) {
        if(!initialized_||!std::isfinite(z)||!std::isfinite(variance)||variance<=0||
           !std::isfinite(gate_sigma)||gate_sigma<=0||
           (have_measurement_[velocity_measurement]&&measurement_id<=last_measurement_[velocity_measurement]))return false;
        const double innovation=z-(velocity_measurement?velocity:position);
        const double S=(velocity_measurement?p11:p00)+variance;
        if(!std::isfinite(S)||S<=0||innovation*innovation>gate_sigma*gate_sigma*S)return false;
        const double k0=(velocity_measurement?p01:p00)/S,k1=(velocity_measurement?p11:p01)/S;
        // Joseph covariance update (I-KH)P(I-KH)' + KRK'.
        const double a=1-(velocity_measurement?0:k0),b=velocity_measurement?-k0:0;
        const double c=velocity_measurement?0:-k1,d=1-(velocity_measurement?k1:0);
        const double aa=a*p00+b*p01,ab=a*p01+b*p11,ca=c*p00+d*p01,cb=c*p01+d*p11;
        const double new00=aa*a+ab*b+k0*k0*variance;
        const double new01=aa*c+ab*d+k0*k1*variance;
        const double new11=ca*c+cb*d+k1*k1*variance;
        position+=k0*innovation;velocity+=k1*innovation;p00=new00;p01=new01;p11=new11;
        last_measurement_[velocity_measurement]=measurement_id;have_measurement_[velocity_measurement]=true;return finite();
    }
    bool finite()const {
        return initialized_&&std::isfinite(position)&&std::isfinite(velocity)&&std::isfinite(p00)&&
          std::isfinite(p01)&&std::isfinite(p11)&&p00>=0&&p11>=0&&p00*p11-p01*p01>=-1e-8;
    }
};

struct WindEstimate { bool valid{false};double north_mps{NAN},east_mps{NAN},airspeed_mps{NAN},rms_mps{NAN};uint64_t fitted_us{0}; };
template<size_t Capacity=64> class WindCircle {
    struct V {double n,e;};std::array<V,Capacity> samples_{};size_t count_{0};
    uint64_t last_epoch_{0},last_sample_us_{0},first_sample_us_{0};bool have_epoch_{false};
public:
    bool add(const NavEpoch& n,uint64_t now,const NavGateConfig& gate,bool steady_trim) {
        if(!steady_trim||!liveNavGate(n,now,gate)||(have_epoch_&&n.epoch_key<=last_epoch_))return false;
        if(count_==Capacity)return false; // caller starts a fresh bounded identification window
        if(count_==0)first_sample_us_=std::min(n.gga_rx_us,n.rmc_rx_us);
        samples_[count_++]={n.north_mps,n.east_mps};last_epoch_=n.epoch_key;
        last_sample_us_=std::min(n.gga_rx_us,n.rmc_rx_us);have_epoch_=true;return true;
    }
    void clear(){count_=0;have_epoch_=false;}
    WindEstimate fit(uint64_t now,size_t min_samples,double min_coverage_rad,double max_rms,
                     double min_airspeed,double max_airspeed,uint64_t max_sample_age_us)const {
        WindEstimate out;
        if(!fresh(now,last_sample_us_,max_sample_age_us)||!fresh(now,first_sample_us_,max_sample_age_us)||count_<std::max(size_t(3),min_samples)||!std::isfinite(min_coverage_rad)||min_coverage_rad<=0||
           min_coverage_rad>2*pi||!std::isfinite(max_rms)||max_rms<=0||
           !std::isfinite(min_airspeed)||!std::isfinite(max_airspeed)||min_airspeed<=0||max_airspeed<=min_airspeed)return out;
        // Center data to reduce conditioning problems for large mean drift.
        double mn=0,me=0;for(size_t i=0;i<count_;++i){mn+=samples_[i].n;me+=samples_[i].e;}mn/=count_;me/=count_;
        double m[3][4]{};
        for(size_t i=0;i<count_;++i) {
            const double n=samples_[i].n-mn,e=samples_[i].e-me;
            const double a[3]={2*n,2*e,1},b=n*n+e*e;
            for(int r=0;r<3;++r){for(int c=0;c<3;++c)m[r][c]+=a[r]*a[c];m[r][3]+=a[r]*b;}
        }
        double max_pivot=0,min_pivot=INFINITY;
        for(int c=0;c<3;++c) {
            int pivot=c;for(int r=c+1;r<3;++r)if(std::abs(m[r][c])>std::abs(m[pivot][c]))pivot=r;
            const double mag=std::abs(m[pivot][c]);if(mag<1e-10)return out;
            max_pivot=std::max(max_pivot,mag);min_pivot=std::min(min_pivot,mag);
            if(max_pivot/min_pivot>1e6)return out;
            for(int j=c;j<4;++j)std::swap(m[c][j],m[pivot][j]);
            const double div=m[c][c];for(int j=c;j<4;++j)m[c][j]/=div;
            for(int r=0;r<3;++r)if(r!=c){const double f=m[r][c];for(int j=c;j<4;++j)m[r][j]-=f*m[c][j];}
        }
        const double wn=m[0][3]+mn,we=m[1][3]+me,r2=m[2][3]+m[0][3]*m[0][3]+m[1][3]*m[1][3];
        if(!std::isfinite(r2)||r2<=0)return out;
        const double speed=std::sqrt(r2);if(speed<min_airspeed||speed>max_airspeed)return out;
        std::array<double,Capacity>angles{};double square=0;
        for(size_t i=0;i<count_;++i) {
            angles[i]=std::atan2(samples_[i].e-we,samples_[i].n-wn);
            const double residual=std::hypot(samples_[i].n-wn,samples_[i].e-we)-speed;square+=residual*residual;
        }
        std::sort(angles.begin(),angles.begin()+count_);
        double largest_gap=angles[0]+2*pi-angles[count_-1];
        for(size_t i=1;i<count_;++i)largest_gap=std::max(largest_gap,angles[i]-angles[i-1]);
        const double rms=std::sqrt(square/count_);
        if(2*pi-largest_gap<min_coverage_rad||rms>max_rms)return out;
        return {true,wn,we,speed,rms,now};
    }
};
} // namespace vritra
