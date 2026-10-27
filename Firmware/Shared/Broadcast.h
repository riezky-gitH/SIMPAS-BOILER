#pragma once
#include <stdint.h>
#include <string.h>
namespace Broadcast {
constexpr uint32_t INTERVAL=1000,KEEPALIVE=3000,MASTER_WINDOW=150,SLAVE_TX_GUARD=400,RX_QUIET=40;
enum Kind:uint8_t {INVALID,REPLY,AUTOMATIC};
inline Kind classify(const char*type,uint32_t session,uint16_t sequence,uint32_t expected){
  if(!strcmp(type,"T"))return session==expected&&sequence?REPLY:INVALID;
  if(!strcmp(type,"B"))return !sequence&&(session==expected||!session)?AUTOMATIC:INVALID;
  return INVALID;
}
inline bool masterMaySend(bool automatic,bool connected,uint32_t now,uint32_t lastReply){return !automatic||!connected||uint32_t(now-lastReply)<=MASTER_WINDOW;}
inline bool slaveMaySend(bool automatic,uint32_t now,uint32_t lastBroadcast,uint32_t lastTx,uint32_t lastRx){
  return automatic&&uint32_t(now-lastBroadcast)>=INTERVAL&&uint32_t(now-lastTx)>=SLAVE_TX_GUARD&&uint32_t(now-lastRx)>=RX_QUIET;
}
struct Gesture {
  bool held=false,fired=false,cancelled=false;uint32_t since=0;
  bool update(bool second,bool third,uint32_t now,uint8_t&secondEvents,uint8_t&thirdEvents){
    if(!held&&second&&third){held=true;fired=cancelled=false;since=now;}
    if(!held)return false;secondEvents=thirdEvents=0;
    if(!second||!third)cancelled=true;
    bool trigger=!cancelled&&!fired&&uint32_t(now-since)>=2000;if(trigger)fired=true;
    if(!second&&!third)held=fired=cancelled=false;
    return trigger;
  }
};
}
