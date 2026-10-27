#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "../Firmware/Shared/MasterScreen.h"
#include "../Firmware/Shared/SlaveUi.h"

struct Op {char type;int x,y,w,h;uint16_t color;std::string text;};
struct Canvas {
  int x=0,y=0,size=1;uint16_t color=Screen::INK;std::vector<Op> ops;
  void fillRect(int x,int y,int w,int h,uint16_t c){ops.push_back({'F',x,y,w,h,c,""});}
  void drawRect(int x,int y,int w,int h,uint16_t c){ops.push_back({'R',x,y,w,h,c,""});}
  void setTextSize(int s){size=s;}void setTextColor(uint16_t c){color=c;}void setCursor(int a,int b){x=a;y=b;}
  void print(const char*s){ops.push_back({'T',x,y,int(strlen(s))*6*size,8*size,color,s});}
};
static bool has(const Canvas&c,const char*s){for(auto&o:c.ops)if(o.text==s)return true;return false;}
static Boiler::Safety normal(){Boiler::Safety r;r.ready=true;r.pFault=r.tFault=false;r.pSim=r.tSim=true;r.config.pressure=500;r.pressure=300;r.temperature=1200;r.evaluate(100);return r;}
static void emit(FILE*f,const char*name,Boiler::Safety left,Boiler::Safety right,bool online=true){
  Canvas c;c.fillRect(0,0,320,172,Screen::BG);MasterScreen::header(c,MasterScreen::MONITOR);
  MasterScreen::card(c,0,true,left);MasterScreen::card(c,1,online,right);MasterScreen::footer(c,"PB1:Hal PB2:Mute PB3:Purge",false);
  fprintf(f,"SCENE\t%s\n",name);for(auto&o:c.ops)fprintf(f,"%c\t%d\t%d\t%d\t%d\t%u\t%s\n",o.type,o.x,o.y,o.w,o.h,unsigned(o.color),o.text.c_str());
}
int main(int argc,char**argv){
  Boiler::Safety r=normal();assert(r.state()==0&&!r.relay&&!r.buzzer);
  r.pressure=449;r.evaluate(200);assert(r.state()==0);
  r.pressure=450;r.evaluate(300);assert(r.state()==1&&!r.relay&&!r.buzzer);
  r.pressure=499;r.evaluate(400);assert(r.state()==1);
  r.pressure=300;r.temperature=1549;r.evaluate(500);assert(r.state()==0);
  r.temperature=1550;r.evaluate(600);assert(r.state()==1&&!r.relay);
  r.temperature=1600;r.evaluate(700);assert(r.state()==2&&r.buzzer&&r.relay);
  r.silence();r.evaluate(800);assert(!r.buzzer&&r.relay); // Silence cannot close an alarmed actuator.
  r.temperature=1200;r.evaluate(900);r.evaluate(3000);assert(r.trip&&r.relay); // Default latch is retained.
  r.config.latch=0;r.evaluate(3100);assert(r.trip&&r.relay); // Preserve existing control behavior: explicit reset.
  assert(r.reset(3100));r.evaluate(3100);assert(!r.trip&&!r.relay);
  r.local=true;r.evaluate(3100);assert(r.relay);r.local=false;r.remote=true;r.evaluate(3200);assert(r.relay);
  r.remote=false;r.global=true;r.evaluate(3300);assert(r.relay);r.global=false;r.evaluate(3400);assert(!r.relay);
  for(uint8_t mask=0;mask<8;mask++){
    auto held=normal();held.trip=true;held.config.latch=0;
    held.local=mask&1;held.remote=mask&2;held.global=mask&4;held.evaluate(5000);
    assert(held.trip&&held.relay);assert(held.reset(5000));held.evaluate(5000);
    assert(!held.trip&&held.relay==bool(mask)); // Reset never discards independent purge requests.
  }
  r.pFault=true;r.evaluate(3500);assert(r.relay&&r.state()==3);
  r=normal();r.ready=false;r.trip=true;r.config.latch=0;r.evaluate(10000);assert(r.trip&&r.relay);
  r=normal();r.trip=true;r.config.latch=0;r.safeSince=UINT32_MAX-1000;
  r.evaluate(998);assert(r.trip&&!r.reset(998));assert(r.reset(999));r.evaluate(999);assert(!r.trip&&!r.relay);
  SlaveUi nav;nav.selected=3;nav.update(1);assert(nav.visible()==1&&nav.selected==3);
  nav.update(0);assert(nav.visible()==3); // MUTE or restored cause returns to selected page.
  nav.update(1);nav.next();assert(nav.visible()==2);nav.update(1);assert(nav.visible()==2); // no repeated forced jump.
  nav.update(3);assert(nav.visible()==1);nav.update(0);assert(nav.visible()==2); // new independent alarm then return.
  nav.home();nav.update(1);nav.update(0);assert(nav.visible()==0);
  nav.selected=1;nav.update(2);nav.update(0);assert(nav.visible()==1); // manually selected alarm page stays selected.
  for(uint8_t page=0;page<MasterScreen::PAGE_COUNT;page++){
    Canvas c;MasterScreen::header(c,page);MasterScreen::footer(c,"PB1:Hal PB2:Mute PB3:Purge",false);
    for(auto&o:c.ops)if(o.type=='T'){assert(o.x>=16&&o.x+o.w<=304);assert(o.y>=5&&o.y+o.h<=164);}
    assert(strcmp(MasterScreen::title(page),"ALARM")&&strcmp(MasterScreen::title(page),"SISTEM"));
  }
  Canvas c;r=normal();r.pressure=460;r.evaluate(100);MasterScreen::card(c,0,true,r);assert(has(c,"WASPADA"));
  assert(c.ops[1].color==Screen::WARNING_EDGE);
  c.ops.clear();r.pressure=520;r.evaluate(200);MasterScreen::card(c,1,true,r);assert(has(c,"ALARM"));assert(c.ops[1].color==Screen::ALARM_EDGE);
  for(auto&o:c.ops){assert(o.x>=164&&o.x+o.w<=316);assert(o.y>=23&&o.y+o.h<=144);}
  c.ops.clear();MasterScreen::card(c,0,false,r);assert(has(c,"B1 OFFLINE")&&!has(c,"ALARM"));
  c.ops.clear();r=normal();r.trip=true;r.evaluate(5000);MasterScreen::card(c,0,true,r);
  assert(r.trip&&r.relay&&!r.causes&&!has(c,"TRIP")&&!has(c,"ALARM"));assert(has(c,"Relay ON / TRIP"));assert(c.ops[1].color==Screen::LINE);
  c.ops.clear();r.pressure=460;r.evaluate(5100);MasterScreen::card(c,0,true,r);assert(has(c,"WASPADA")&&!has(c,"TRIP"));
  c.ops.clear();r.pressure=520;r.evaluate(5200);r.silence();r.evaluate(5300);MasterScreen::card(c,0,true,r);
  assert(has(c,"ALARM")&&!r.buzzer); // Silence does not hide an active cause.
  assert(Screen::index(Screen::WARNING_EDGE)!=Screen::index(Screen::BG));
  assert(Screen::index(Screen::ALARM_EDGE)!=Screen::index(Screen::BG));
  if(argc==2){FILE*f=fopen(argv[1],"w");assert(f);auto a=normal(),b=normal();emit(f,"Normal",a,b);
    a.pressure=460;a.evaluate(100);b.temperature=1650;b.evaluate(100);emit(f,"Waspada dan alarm",a,b);
    a=normal();a.trip=true;a.evaluate(100);emit(f,"Trip dan offline",a,b,false);fclose(f);}
  puts("Operator UI: warning boundaries, preserved actuator arbitration/explicit reset, alarm return, page count, text margins and exact monitor draw operations passed.");
}
