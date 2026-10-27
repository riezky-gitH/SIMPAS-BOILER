#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

namespace HistoryTiming {
constexpr uint32_t INTERVAL_MS=5000;
struct Schedule {
  bool started=false;uint32_t last=0;uint8_t masks[2]={0,0};
  bool periodic(uint32_t now){
    if(!started){started=true;last=now;return true;}
    uint32_t elapsed=now-last;if(elapsed<INTERVAL_MS)return false;
    last+=(elapsed/INTERVAL_MS)*INTERVAL_MS;return true;
  }
  bool observe(uint8_t node,uint8_t mask){
    bool rising=(mask&~masks[node])!=0;masks[node]=mask;return rising;
  }
};
inline uint32_t inferredEpoch(uint32_t epoch,uint32_t rowBoot,uint32_t seconds,uint32_t boot,uint32_t nowEpoch,uint32_t nowSeconds){
  if(epoch||rowBoot!=boot||!nowEpoch||seconds>nowSeconds||nowEpoch<nowSeconds-seconds)return epoch;
  return nowEpoch-(nowSeconds-seconds);
}
inline void timestamp(uint32_t epoch,char*out,size_t size){
  if(!epoch){snprintf(out,size,"UNSYNCED");return;}
  time_t shifted=time_t(epoch)+7*3600;struct tm date;
#ifdef _WIN32
  gmtime_s(&date,&shifted);
#else
  gmtime_r(&shifted,&date);
#endif
  strftime(out,size,"%Y-%m-%d %H:%M:%S",&date);
}
}
