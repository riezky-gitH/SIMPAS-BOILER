#pragma once
#include <stdint.h>
#include <string.h>
namespace Boiler {
enum Cause : uint8_t { PRESSURE_HIGH=1, TEMP_HIGH=2, PRESSURE_FAULT=4, TEMP_FAULT=8, MODE_FAULT=16 };
enum Flag : uint16_t { P_SIM=1,T_SIM=2,P_BAD=4,T_BAD=8,RELAY_ON=16,LOCAL=32,REMOTE=64,GLOBAL=128,TRIP=256,BUZZER=512,TEST=1024,STARTUP=2048,AUTO_DEMO=16384 };
constexpr uint16_t WIRE_FLAGS=4095|AUTO_DEMO;
enum Result : uint8_t { POLLED=0,APPLIED=1,BLOCKED=2,INVALID=3,BUSY=4 };
struct Config {
  uint16_t version=1,pressure=400,temperature=1600,zeroMv=500,spanMv=4000,vccMv=5000;
  uint8_t live=0,latch=1;
};
inline bool validConfig(const Config &c) {
  return c.version && c.pressure>=50 && c.pressure<=1000 && c.temperature>=500 && c.temperature<=3000 &&
    c.zeroMv>=250 && c.zeroMv<=1000 && c.spanMv>=2500 && c.spanMv<=4500 && c.vccMv>=4500 && c.vccMv<=5500 && c.live<=1 && c.latch<=1;
}
struct Safety {
  Config config; int16_t pressure=-1,temperature=-1;
  bool pSim=false,tSim=false,pFault=true,tFault=true,ready=false;
  bool local=false,remote=false,global=false,trip=false,relay=false,buzzer=false;
  uint8_t causes=0,muted=0,acknowledged=0; uint32_t safeSince=0;
  void evaluate(uint32_t now) {
    uint8_t next=0;
    if(ready) {
      if(pFault)next|=PRESSURE_FAULT;
      else if(pressure>=config.pressure || ((causes&PRESSURE_HIGH)&&pressure>config.pressure-20))next|=PRESSURE_HIGH;
      if(tFault)next|=TEMP_FAULT;
      else if(temperature>=config.temperature || ((causes&TEMP_HIGH)&&temperature>config.temperature-30))next|=TEMP_HIGH;
      if(config.live&&(pSim||tSim))next|=MODE_FAULT;
    }
    uint8_t added=next&~causes;muted&=next&~added;acknowledged&=next&~added;causes=next;
    if(causes&&config.latch)trip=true;
    if(!ready||causes)safeSince=now;
    relay=!ready||causes||trip||local||remote||global;buzzer=ready&&(causes&~muted);
  }
  void silence(){muted=causes;}
  void acknowledge(){acknowledged=causes;}
  bool canRelease(uint32_t now)const{return ready&&!causes&&uint32_t(now-safeSince)>=2000;}
  bool reset(uint32_t now){if(!canRelease(now))return false;trip=false;return true;}
  bool releaseGlobal(uint32_t now){if(!canRelease(now))return false;global=false;return true;}
  bool nearThreshold()const {
    return ready&&((!pFault&&pressure>=0&&pressure>=int32_t(config.pressure)-50&&pressure<config.pressure)||
      (!tFault&&temperature>=0&&temperature>=int32_t(config.temperature)-50&&temperature<config.temperature));
  }
  uint16_t flags(bool test=false)const {
    return(pSim?P_SIM:0)|(tSim?T_SIM:0)|(pFault?P_BAD:0)|(tFault?T_BAD:0)|(relay?RELAY_ON:0)|
      (local?LOCAL:0)|(remote?REMOTE:0)|(global?GLOBAL:0)|(trip?TRIP:0)|(buzzer?BUZZER:0)|(test?TEST:0)|(!ready?STARTUP:0);
  }
  uint8_t state()const {
    if(!ready)return 4;if(causes&(PRESSURE_FAULT|TEMP_FAULT|MODE_FAULT))return 3;
    if(causes||trip)return 2;return nearThreshold()?1:0;
  }
};
}
