#pragma once
#include <array>
#include <cstddef>
namespace vritra::pins {
struct Main {
    static constexpr int sda=18,scl=19,gnss_rx=7,gnss_tx=8,gnss_reset=6;
    static constexpr int spi_mosi=26,spi_sck=27,spi_miso=39,radio_cs=29;
    static constexpr int radio_busy=24,radio_reset=25,radio_irq=30,flash_cs=9;
    static constexpr int camera_tx=1,camera_rx=0,p4_tx=14,p4_rx=15;
    static constexpr int heartbeat=5,buzzer=2,led_green=31,led_red=32,battery_adc=20;
};
struct Backup {
    static constexpr int sda=18,scl=19,gnss_rx=0,heartbeat_in=4;
    static constexpr int p4_tx=14,p4_rx=15,sd_cs=10,sd_mosi=11,sd_miso=12,sd_sck=13;
    static constexpr int led_green=36,led_red=37;
};
struct Parafoil {
    static constexpr int sda=7,scl=8,gnss_rx=14,main_rx=19,backup_rx=16,shared_tx=54;
    static constexpr int steer_left=20,steer_right=21,bay=22,petri=23,spare=32,esc=33,hall=5;
};
template<size_t N> constexpr bool unique(const std::array<int,N>& a) {
    for(size_t i=0;i<N;++i)for(size_t j=i+1;j<N;++j)if(a[i]==a[j])return false;
    return true;
}
static_assert(unique(std::array<int,14>{7,8,14,19,16,54,20,21,22,23,32,33,5,-1}));
} // namespace vritra::pins
