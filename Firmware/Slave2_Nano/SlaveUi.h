#pragma once
#include <stdint.h>

// Keep the operator's selected page separate from the temporary sounding alarm.
struct SlaveUi {
  uint8_t selected=0,sounding=0;bool overlay=false;
  uint8_t visible()const{return overlay?1:selected;}
  void update(uint8_t activeSound){
    if(activeSound&~sounding)overlay=true;
    if(!activeSound)overlay=false;
    sounding=activeSound;
  }
  void next(){selected=(visible()+1)%5;overlay=false;}
  void home(){selected=0;overlay=false;}
};
