#pragma once
#include "flight_core.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <string_view>

namespace vritra {
// Feed only bytes from the physical L89 UART in a flight build. No replay/file
// adapter is part of this class. Its parser cannot authenticate GNSS RF signals.
class NmeaEpochParser {
    struct Gga {
        bool valid{false}; uint32_t utc_ms{0};uint64_t rx{0};
        double lat{NAN},lon{NAN},alt{NAN},hdop{NAN};uint8_t q{0},sats{0};
    } gga_;
    struct Rmc {
        bool valid{false};uint32_t utc_ms{0},date{0};uint64_t rx{0};
        double lat{NAN},lon{NAN},speed{NAN},course{NAN};
    } rmc_;
    uint8_t dimension_{0};uint64_t quality_rx_{0},last_published_{0};
    bool published_{false};NavEpoch nav_;
    std::array<char,161> line_{};size_t used_{0};bool collecting_{false};

    static int hex(char c) {
        if (c>='0' && c<='9') return c-'0';
        if (c>='A' && c<='F') return c-'A'+10;
        if (c>='a' && c<='f') return c-'a'+10;
        return -1;
    }
    static bool number(std::string_view s,double& v) {
        if (s.empty()) return false;
        // NMEA decimal grammar deliberately excludes exponents, NaN and inf.
        size_t i=0;if (s[0]=='-' || s[0]=='+') ++i;
        bool digit=false,dot=false;
        for (;i<s.size();++i) {
            if (s[i]>='0' && s[i]<='9') { digit=true;continue; }
            if (s[i]=='.' && !dot) { dot=true;continue; }
            return false;
        }
        if (!digit) return false;
        // from_chars does not accept leading '+'.
        if (s.front()=='+') s.remove_prefix(1);
        auto r=std::from_chars(s.data(),s.data()+s.size(),v,std::chars_format::fixed);
        return r.ec==std::errc{} && r.ptr==s.data()+s.size() && std::isfinite(v);
    }
    static bool integer(std::string_view s,unsigned& v) {
        if (s.empty()) return false;
        for (char c:s) if (c<'0' || c>'9') return false;
        auto r=std::from_chars(s.data(),s.data()+s.size(),v);
        return r.ec==std::errc{} && r.ptr==s.data()+s.size();
    }
    static bool utc(std::string_view s,uint32_t& ms) {
        if (s.size()<6) return false;
        for (size_t i=0;i<6;++i) if (s[i]<'0' || s[i]>'9') return false;
        if (s.size()>6 && s[6]!='.') return false;
        const unsigned h=(s[0]-'0')*10+(s[1]-'0'),m=(s[2]-'0')*10+(s[3]-'0');
        const unsigned sec=(s[4]-'0')*10+(s[5]-'0');
        // Leap-second handling requires explicit time policy; reject 60 here.
        if (h>23 || m>59 || sec>59) return false;
        unsigned fraction=0,scale=100;
        for (size_t i=7;i<s.size();++i) {
            if (s[i]<'0' || s[i]>'9') return false;
            if (scale) { fraction+=(s[i]-'0')*scale;scale/=10; }
        }
        ms=((h*60+m)*60+sec)*1000+fraction;return true;
    }
    static bool date(std::string_view s,uint32_t& key) {
        if (s.size()!=6) return false;
        for (char c:s) if (c<'0' || c>'9') return false;
        const unsigned d=(s[0]-'0')*10+s[1]-'0',m=(s[2]-'0')*10+s[3]-'0';
        const unsigned y=2000+(s[4]-'0')*10+s[5]-'0';
        constexpr unsigned days[]{0,31,28,31,30,31,30,31,31,30,31,30,31};
        if (m<1 || m>12 || d<1 || d>days[m]+((m==2 && y%4==0)?1:0)) return false;
        key=y*10000+m*100+d;return true; // ordering key, NOT Unix time
    }
    static bool coord(std::string_view s,std::string_view hemi,bool latitude,double& deg) {
        double raw;if (!number(s,raw) || raw<0 || hemi.size()!=1) return false;
        if ((latitude && hemi!="N" && hemi!="S") || (!latitude && hemi!="E" && hemi!="W")) return false;
        const size_t dot=s.find('.');
        const size_t whole=dot==std::string_view::npos?s.size():dot;
        if (whole!=(latitude?4u:5u)) return false;
        const double whole_degrees=std::floor(raw/100),minutes=raw-whole_degrees*100;
        if (minutes<0 || minutes>=60 || whole_degrees>(latitude?90:180) ||
            (whole_degrees==(latitude?90:180) && minutes>0)) return false;
        deg=whole_degrees+minutes/60;
        if (hemi=="S" || hemi=="W") deg=-deg;
        return true;
    }
    bool commit(uint64_t now) {
        if (!gga_.valid || !rmc_.valid || dimension_!=3 || gga_.utc_ms!=rmc_.utc_ms ||
            !fresh(now,gga_.rx,500000) || !fresh(now,rmc_.rx,500000) ||
            !fresh(now,quality_rx_,2500000)) return false;
        const uint64_t epoch=static_cast<uint64_t>(rmc_.date)*86400000ULL+rmc_.utc_ms;
        if (published_ && epoch<=last_published_) return false;
        // GGA and RMC positions for one epoch must agree within ~1m in degrees.
        if (std::abs(gga_.lat-rmc_.lat)>1e-5 || std::abs(gga_.lon-rmc_.lon)>1e-5) return false;
        nav_={Origin::LiveUart,true,gga_.q,dimension_,gga_.sats,gga_.lat,gga_.lon,gga_.alt,
              rmc_.speed*std::cos(rmc_.course),rmc_.speed*std::sin(rmc_.course),gga_.hdop,
              epoch,gga_.rx,rmc_.rx,quality_rx_};
        last_published_=epoch;published_=true;return true;
    }
public:
    const NavEpoch& latest() const { return nav_; }
    bool sentence(std::string_view input,uint64_t now) {
        while (!input.empty() && (input.back()=='\r' || input.back()=='\n')) input.remove_suffix(1);
        if (input.size()<9 || input.size()>160 || input.front()!='$') return false;
        const auto star=input.find('*');
        if (star==std::string_view::npos || star+3!=input.size()) return false;
        const int a=hex(input[star+1]),b=hex(input[star+2]);if (a<0 || b<0) return false;
        uint8_t xor_sum=0;for (size_t i=1;i<star;++i) xor_sum^=static_cast<uint8_t>(input[i]);
        if (xor_sum!=static_cast<uint8_t>(a*16+b)) return false;
        std::array<std::string_view,32> f{};size_t count=0,start=1;
        for (size_t i=1;i<=star;++i) if (i==star || input[i]==',') {
            if (count==f.size()) return false;
            f[count++]=input.substr(start,i-start);start=i+1;
        }
        if (f[0].size()!=5) return false; // proprietary responses handled elsewhere
        const auto tag=f[0].substr(2);
        if (tag=="GSA") {
            unsigned dim;
            if (count<18 || !integer(f[2],dim) || dim<1 || dim>3) return false;
            dimension_=static_cast<uint8_t>(dim);quality_rx_=now;
            if (dimension_!=3) nav_.coherent=false;
        } else if (tag=="GGA") {
            Gga n;unsigned q,sats;
            if (count<15 || !integer(f[6],q) || q>8) return false;
            if (q!=1 && q!=2 && q!=4 && q!=5) {
                gga_.valid=false;nav_.coherent=false;return false;
            }
            if (!utc(f[1],n.utc_ms) || !coord(f[2],f[3],true,n.lat) ||
                !coord(f[4],f[5],false,n.lon) || !integer(f[7],sats) || sats>99 ||
                !number(f[8],n.hdop) || !number(f[9],n.alt) || f[10]!="M") return false;
            n.valid=true;n.q=static_cast<uint8_t>(q);n.sats=static_cast<uint8_t>(sats);n.rx=now;gga_=n;
        } else if (tag=="RMC") {
            Rmc n;
            if (count<10) return false;
            if (f[2]!="A") { rmc_.valid=false;nav_.coherent=false;return false; }
            double knots,course;
            if (!utc(f[1],n.utc_ms) || !coord(f[3],f[4],true,n.lat) ||
                !coord(f[5],f[6],false,n.lon) || !number(f[7],knots) || knots<0 ||
                !number(f[8],course) || course<0 || course>=360 || !date(f[9],n.date)) return false;
            // Optional FAA mode: estimated/manual/simulation/invalid excluded.
            if (count>12 && !f[12].empty() && f[12]!="A" && f[12]!="D" && f[12]!="R" && f[12]!="F") {
                rmc_.valid=false;nav_.coherent=false;return false;
            }
            n.valid=true;n.speed=knots*0.5144444444444444;n.course=course*pi/180;n.rx=now;rmc_=n;
        } else return false;
        return commit(now);
    }
    bool byte(char c,uint64_t rx_us) {
        if (c=='$') { used_=0;collecting_=true;line_[used_++]=c;return false; }
        if (!collecting_) return false;
        if (c=='\n') {
            collecting_=false;
            return sentence(std::string_view(line_.data(),used_),rx_us);
        }
        if (used_==160) { used_=0;collecting_=false;return false; }
        line_[used_++]=c;return false;
    }
};
} // namespace vritra
