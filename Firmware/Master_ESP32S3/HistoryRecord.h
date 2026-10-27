#pragma once
#include "Protocol.h"
#include "HistoryTiming.h"
namespace Historian {
// Reserved historian-only flag bits; UART actuator flags remain unchanged.
constexpr uint16_t STATUS_KNOWN=8192,WAS_WARNING=4096;
constexpr char LEGACY_HEADER[]="epoch,boot,uptime_s,p1_cbar,t1_dC,f1,c1,m1,a1,p2_cbar,t2_dC,f2,c2,m2,a2,crc16\n";
constexpr char CSV_HEADER[]="epoch,boot,uptime_s,p1_cbar,t1_dC,f1,c1,m1,a1,p2_cbar,t2_dC,f2,c2,m2,a2,waktu_wib,crc16\n";
struct Record {
  uint32_t epoch=0,boot=0,seconds=0;
  int16_t p[2]={-1,-1},t[2]={-1,-1};
  uint16_t flags[2]={0,0};uint8_t causes[2]={0,0},muted[2]={0,0},ack[2]={0,0};
  uint8_t connected=0,persist=0;uint16_t encodedBytes=0;
};
struct Alarm {uint32_t epoch=0,boot=0,seconds=0;uint8_t node=0,causes=0;};
struct AlarmStore {uint32_t magic=0x414C4D33;Alarm rows[5];uint16_t crc=0;};
inline size_t csv(const Record&r,char*out,size_t capacity){
  // Connectivity is packed in bit 15 of each flags field. Values are scaled integers.
  char stamp[24];HistoryTiming::timestamp(r.epoch,stamp,sizeof stamp);
  int n=snprintf(out,capacity,"%lu,%lu,%lu,%d,%d,%u,%u,%u,%u,%d,%d,%u,%u,%u,%u,%s",
    (unsigned long)r.epoch,(unsigned long)r.boot,(unsigned long)r.seconds,r.p[0],r.t[0],r.flags[0]|((r.connected&1)?32768:0),r.causes[0],r.muted[0],r.ack[0],
    r.p[1],r.t[1],r.flags[1]|((r.connected&2)?32768:0),r.causes[1],r.muted[1],r.ack[1],stamp);
  if(n<0||size_t(n)+7>=capacity)return 0;
  return n+snprintf(out+n,capacity-n,",%04X\n",Link::crc16(out,n));
}
inline bool parseCsv(char*line,Record&r){
  char*last=strrchr(line,',');if(!last||strlen(last+1)!=4)return false;
  for(char*p=last+1;*p;p++)if(!((*p>='0'&&*p<='9')||(*p>='A'&&*p<='F')))return false;
  uint16_t crc=strtoul(last+1,nullptr,16);*last=0;if(Link::crc16(line,strlen(line))!=crc)return false;
  char*f[16];uint8_t fields=Link::split(line,f,16);if(fields!=15&&fields!=16)return false;uint32_t u;r.connected=0;
  if(!Link::unsignedNumber(f[0],r.epoch)||!Link::unsignedNumber(f[1],r.boot)||!Link::unsignedNumber(f[2],r.seconds))return false;
  for(uint8_t i=0;i<2;i++){
    uint8_t j=3+i*6;int32_t v;
    if(!Link::number(f[j],-1,1000,v))return false;r.p[i]=v;
    if(!Link::number(f[j+1],-1,10240,v))return false;r.t[i]=v;
    if(!Link::unsignedNumber(f[j+2],u)||u>65535)return false;r.flags[i]=u&32767;if(u&32768)r.connected|=1<<i;
    if(!Link::number(f[j+3],0,31,v))return false;r.causes[i]=v;
    if(!Link::number(f[j+4],0,31,v))return false;r.muted[i]=v;
    if(!Link::number(f[j+5],0,31,v))return false;r.ack[i]=v;
  }
  if(fields==16){char stamp[24];HistoryTiming::timestamp(r.epoch,stamp,sizeof stamp);if(strcmp(f[15],stamp))return false;}
  return true;
}
}
