#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace vritra {
struct ScheduledJob {
    uint64_t period_us,next_due_us;
    void (*run)(void*,uint64_t);void* context;
    uint32_t deadline_misses{0},runs{0};
    bool poll(uint64_t now) {
        if(period_us==0 || !run || now<next_due_us)return false;
        if(now-next_due_us>=period_us)++deadline_misses;
        next_due_us=now+period_us; // no catch-up burst using duplicate samples
        run(context,now);++runs;return true;
    }
};
class ProgressWatchdog {
    uint32_t required_,progress_{0};uint64_t window_us_,window_start_;
public:
    ProgressWatchdog(uint32_t required,uint64_t window_us,uint64_t start)
        :required_(required),window_us_(window_us),window_start_(start){}
    void advanced(uint32_t mask){progress_|=mask;}
    bool mayFeed(uint64_t now) {
        if(now<window_start_ || !window_us_)return false;
        if((progress_&required_)==required_ && now-window_start_<window_us_) {
            progress_=0;window_start_=now;return true;
        }
        return false; // hardware watchdog owns expiry; never feed merely from ISR
    }
};
} // namespace vritra
