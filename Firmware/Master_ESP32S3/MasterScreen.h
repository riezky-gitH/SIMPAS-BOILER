#pragma once
#include <stdio.h>
#include "Control.h"
#include "IndexedFrame.h"

namespace MasterScreen {
enum Page:uint8_t {MONITOR,HISTORY,NETWORK,GUIDE,LIMITS,PAGE_COUNT};
constexpr int SIDE_PAD=16;
inline const char*title(uint8_t page){const char*names[]={"SIMPAS","RIWAYAT","WIFI","PANDUAN","BATAS ALARM"};return page<PAGE_COUNT?names[page]:names[0];}
template<class Canvas> void text(Canvas&c,int x,int y,const char*s,uint16_t color=Screen::INK,uint8_t size=1){
  c.setTextSize(size);c.setTextColor(color);c.setCursor(x,y);c.print(s);
}
template<class Canvas> void header(Canvas&c,uint8_t page){
  c.fillRect(0,0,320,19,Screen::ACCENT);text(c,SIDE_PAD,5,title(page),Screen::WHITE);
  char number[8];snprintf(number,sizeof number,"%u/%u",page+1,PAGE_COUNT);text(c,320-SIDE_PAD-18,5,number,Screen::WHITE);
}
template<class Canvas> void footer(Canvas&c,const char*s,bool purge){
  c.fillRect(0,151,320,21,purge?Screen::ACCENT:Screen::FOOTER);text(c,SIDE_PAD,156,s,Screen::WHITE);
}
template<class Canvas> void card(Canvas&c,uint8_t index,bool connected,const Boiler::Safety&r){
  int x=8+index*160;char line[32];uint8_t state=r.state();
  // A retained trip is a relay state, not a new active alarm notification.
  bool alarm=connected&&r.causes,warning=connected&&!alarm&&r.nearThreshold();
  uint16_t border=alarm?Screen::ALARM_EDGE:warning?Screen::WARNING_EDGE:Screen::LINE;
  c.fillRect(x-4,23,152,121,Screen::PAPER);
  for(int k=0;k<(alarm||warning?3:1);k++)c.drawRect(x-4+k,23+k,152-2*k,121-2*k,border);
  if(alarm||warning){
    snprintf(line,sizeof line,"B%u",index+1);text(c,x+2,30,line);
    const char*label=warning?"WASPADA":state==3?"GANGGUAN":"ALARM";
    int width=int(strlen(label))*6+8,left=x+148-width-5;c.fillRect(left,27,width,13,border);
    text(c,left+4,30,label,warning?Screen::INK:Screen::WHITE);
  }else{
    if(connected&&r.trip)snprintf(line,sizeof line,"B%u",index+1);
    else snprintf(line,sizeof line,"B%u %s",index+1,!connected?"OFFLINE":state==4?"MENUNGGU":"NORMAL");
    text(c,x,29,line,connected&&state==0?Screen::GREEN:Screen::MUTED);
  }
  if(!connected)snprintf(line,sizeof line,"P --");else if(r.pFault||r.pressure<0)snprintf(line,sizeof line,"P ERROR");
  else snprintf(line,sizeof line,"%u.%02u bar",unsigned(r.pressure/100),unsigned(r.pressure%100));
  text(c,x,48,line,Screen::INK,2);
  if(!connected)snprintf(line,sizeof line,"T --");else if(r.tFault||r.temperature<0)snprintf(line,sizeof line,"T ERROR");
  else snprintf(line,sizeof line,"%u.%u C",unsigned(r.temperature/10),unsigned(r.temperature%10));
  text(c,x,76,line,Screen::INK,2);
  if(!connected)snprintf(line,sizeof line,"Sumber --");else snprintf(line,sizeof line,"P:%s T:%s",r.pSim?"SIM":"REAL",r.tSim?"SIM":"REAL");
  text(c,x,105,line,Screen::MUTED);
  if(!connected)snprintf(line,sizeof line,"Relay --");else snprintf(line,sizeof line,"Relay %s%s",r.relay?"ON":"OFF",r.trip?" / TRIP":"");
  text(c,x,124,line);
}
}
