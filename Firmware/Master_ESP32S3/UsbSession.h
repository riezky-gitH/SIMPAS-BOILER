#pragma once
#include <stdint.h>

namespace UsbExport {
// Track the actual host state, not just callbacks: CDC may start USB before setup.
struct Session {
  bool connected=false,requested=false,ejected=false;
  uint32_t nextAttempt=0;
  void observe(bool online,uint32_t now){
    if(online==connected)return;
    connected=online;ejected=false;requested=online;nextAttempt=now+500;
  }
  void eject(){requested=false;ejected=true;}
  void refresh(uint32_t now){ejected=false;requested=true;nextAttempt=now+600;}
  bool due(uint32_t now,bool ready,bool filesystem)const{
    return connected&&requested&&!ejected&&ready&&filesystem&&int32_t(now-nextAttempt)>=0;
  }
  void failed(uint32_t now){nextAttempt=now+1000;}
  void inserted(){requested=false;}
};
}
