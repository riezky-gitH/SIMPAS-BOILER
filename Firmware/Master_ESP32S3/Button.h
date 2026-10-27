#pragma once
#include <Arduino.h>
struct PanelButton {
  explicit PanelButton(uint8_t p):pin(p){}
  uint8_t pin;bool raw=false,stable=false,longSent=false;uint32_t changed=0,pressed=0;
  uint8_t update(uint32_t now){bool r=digitalRead(pin)==LOW;uint8_t event=0;
    if(r!=raw){raw=r;changed=now;}
    if(uint32_t(now-changed)>=30&&raw!=stable){stable=raw;if(stable){pressed=now;longSent=false;event|=1;}else if(!longSent)event|=2;}
    if(stable&&!longSent&&uint32_t(now-pressed)>=2000){longSent=true;event|=4;}return event;
  }
};
