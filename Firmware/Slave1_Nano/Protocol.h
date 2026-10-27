#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
namespace Link {
static const uint16_t MAX_FRAME=176;
inline uint16_t crc16(const void *data,size_t n){uint16_t crc=0xFFFF;const uint8_t*p=(const uint8_t*)data;
  while(n--){crc^=uint16_t(*p++)<<8;for(uint8_t i=0;i<8;i++)crc=(crc&0x8000)?(crc<<1)^0x1021:crc<<1;}return crc;}
inline bool number(const char*s,int32_t lo,int32_t hi,int32_t&out){
  if(!s||!*s||(*s!='-'&&(*s<'0'||*s>'9')))return false;char*end;long v=strtol(s,&end,10);
  if(*end||v<lo||v>hi)return false;out=v;return true;}
inline bool unsignedNumber(const char*s,uint32_t&out){if(!s||!*s)return false;uint32_t v=0;
  for(;*s;s++){if(*s<'0'||*s>'9')return false;uint8_t d=*s-'0';if(v>(UINT32_MAX-d)/10)return false;v=v*10+d;}out=v;return true;}
inline uint8_t split(char*s,char**fields,uint8_t capacity){uint8_t n=1;fields[0]=s;
  for(char*p=s;*p;p++)if(*p==','){if(n==capacity)return 0;*p=0;fields[n++]=p+1;}return n;}
template<class Port>inline void send(Port&port,const char*payload){char suffix[9];snprintf(suffix,sizeof suffix,"*%04X\n",crc16(payload,strlen(payload)));port.write('#');port.print(payload);port.print(suffix);}
// Keep a poll sequence valid until its reply or timeout; never replace it mid-frame.
struct PollCycle {
  uint16_t sequence=0;uint32_t sent=0;bool waiting=false;
  void start(uint16_t seq,uint32_t now){sequence=seq;sent=now;waiting=true;}
  bool matches(uint16_t seq)const{return waiting&&seq==sequence;}
  bool complete(uint16_t seq){if(!matches(seq))return false;waiting=false;return true;}
  bool expired(uint32_t now)const{return waiting&&uint32_t(now-sent)>=700;}
};
struct Receiver {
  char data[MAX_FRAME];uint16_t length=0;uint32_t since=0,errors=0;bool receiving=false;
  char*feed(char c,uint32_t now){
    if(receiving&&uint32_t(now-since)>300){receiving=false;length=0;errors++;}
    if(c=='#'){length=0;since=now;receiving=true;return nullptr;}
    if(!receiving||c=='\r')return nullptr;since=now;
    if(c!='\n'){if(length>=MAX_FRAME-1){receiving=false;errors++;return nullptr;}data[length++]=c;return nullptr;}
    receiving=false;data[length]=0;
    if(length<6||data[length-5]!='*'){errors++;return nullptr;}char*tag=data+length-4;
    for(uint8_t i=0;i<4;i++)if(!((tag[i]>='0'&&tag[i]<='9')||(tag[i]>='A'&&tag[i]<='F'))){errors++;return nullptr;}
    uint16_t received=strtoul(tag,nullptr,16);data[length-5]=0;
    if(received!=crc16(data,length-5)){errors++;return nullptr;}return data;
  }
};
}
