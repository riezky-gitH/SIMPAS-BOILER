#pragma once
#include "Control.h"
namespace Demo {
// Accelerated presentation profile; wire sampling and real sensor control stay unchanged.
constexpr int16_t PRESSURE_STEP=10,TEMPERATURE_STEP=15,RELIEF_PRESSURE_STEP=20,RELIEF_TEMPERATURE_STEP=30;
constexpr uint32_t TARGET_INTERVAL=2000;
inline bool allowed(const Boiler::Config&config,bool ready,bool pressureSim,bool temperatureSim){return ready&&!config.live&&pressureSim&&temperatureSim;}
// Demonstration trajectory, not a calibrated thermodynamic model of a boiler.
struct Sensors {
  bool enabled=false;uint8_t phase=0;uint32_t randomState=1,last=0,phaseAt=0,duration=0,targetAt=0;
  int32_t pQ=0,tQ=0;int16_t pTarget=0,tTarget=0;
  uint32_t random(){uint32_t x=randomState;x^=x<<13;x^=x>>17;x^=x<<5;return randomState=x?x:1;}
  int32_t between(int32_t low,int32_t high){return low+random()%uint32_t(high-low+1);}
  static int32_t bounded(int32_t value,int32_t low,int32_t high){return value<low?low:value>high?high:value;}
  void phaseDuration(){duration=phase==4?between(28000,36000):phase==2?between(12000,16000):between(6000,8000);}
  void start(uint32_t now,uint32_t seed,uint8_t node,int16_t pressure,int16_t temperature){
    randomState=seed^(uint32_t(node)*0x9E3779B9UL);if(!randomState)randomState=1;
    enabled=true;phase=(node-1)%2;last=phaseAt=now;targetAt=now-TARGET_INTERVAL;phaseDuration();
    pQ=bounded(pressure<0?200:pressure,0,1000)*256;tQ=bounded(temperature<0?250:temperature,0,3500)*256;
  }
  void stop(){enabled=false;}
  int16_t pressure()const{return (pQ+128)/256;}
  int16_t temperature()const{return (tQ+128)/256;}
  void targets(const Boiler::Config&config,bool relief){
    if(relief){pTarget=int32_t(config.pressure)*between(20,32)/100;tTarget=int32_t(config.temperature)*between(55,65)/100;}
    else if(phase==1){pTarget=int32_t(config.pressure)-between(12,37);tTarget=int32_t(config.temperature)-between(12,37);}
    else if(phase==2){pTarget=int32_t(config.pressure)+between(10,35);tTarget=int32_t(config.temperature)*between(75,90)/100;}
    else if(phase==4){pTarget=int32_t(config.pressure)*between(55,75)/100;tTarget=int32_t(config.temperature)+between(50,120);}
    else{pTarget=int32_t(config.pressure)*between(55,80)/100;tTarget=int32_t(config.temperature)*between(60,82)/100;}
    pTarget=bounded(pTarget,0,1000);tTarget=bounded(tTarget,0,3500);
  }
  static void approach(int32_t&value,int32_t target,int32_t rate){
    int32_t distance=target*256-value,step=distance/8;
    if(distance&&step==0)step=distance;
    value+=bounded(step,-rate*256,rate*256);
  }
  void update(uint32_t now,const Boiler::Config&config,bool relief){
    if(!enabled)return;uint32_t elapsed=now-last;if(elapsed<250)return;
    uint32_t steps=elapsed/250;last+=steps*250;if(steps>4)steps=4;
    if(uint32_t(now-phaseAt)>=duration){phase=(phase+1)%6;phaseAt=now;phaseDuration();targetAt=now-TARGET_INTERVAL;}
    // Re-evaluate relief immediately; jitter only changes targets, never the output directly.
    if(relief||uint32_t(now-targetAt)>=TARGET_INTERVAL){targets(config,relief);targetAt=now;}
    while(steps--){approach(pQ,pTarget,relief?RELIEF_PRESSURE_STEP:PRESSURE_STEP);approach(tQ,tTarget,relief?RELIEF_TEMPERATURE_STEP:TEMPERATURE_STEP);}
  }
};
}
