#include <assert.h>
#include <stdio.h>
#include <string>
#include "../Firmware/Shared/DemoSensors.h"
#include "../Firmware/Shared/Broadcast.h"
#include "../Firmware/Shared/Protocol.h"
struct Port {std::string data;void write(char c){data+=c;}void print(const char*s){data+=s;}};
int main(int argc,char**argv){
  Boiler::Config config;assert(Demo::allowed(config,true,true,true));
  assert(!Demo::allowed(config,false,true,true)&&!Demo::allowed(config,true,false,true)&&!Demo::allowed(config,true,true,false));
  config.live=1;assert(!Demo::allowed(config,true,true,true));config.live=0;
  Demo::Sensors a,b,same;a.start(1000,12345,1,261,807);same.start(1000,12345,1,261,807);b.start(1000,12345,2,261,807);
  assert(a.pressure()==261&&a.temperature()==807);bool different=false;
  for(uint32_t now=1250;now<90000;now+=250){a.update(now,config,false);same.update(now,config,false);b.update(now,config,false);
    assert(a.pressure()==same.pressure()&&a.temperature()==same.temperature());different|=a.pressure()!=b.pressure()||a.temperature()!=b.temperature();}
  assert(different);
  Boiler::Safety state;state.config=config;state.config.latch=0;state.ready=true;state.pFault=state.tFault=false;state.pSim=state.tSim=true;
  Demo::Sensors simulator;simulator.start(1000,12345,1,261,807);bool pressureAlarm=false,tempAlarm=false,warning=false,falling=false,rising=false;
  FILE*trace=argc==2?fopen(argv[1],"w"):nullptr;if(trace)fputs("time_s,pressure_bar,temperature_c,relay,state,phase\n",trace);
  int16_t previousP=261,previousT=807;
  for(uint32_t now=1250;now<3601000;now+=250){
    bool relief=state.relay;simulator.update(now,state.config,relief);int p=simulator.pressure(),t=simulator.temperature();
    int dp=p-previousP,dt=t-previousT;assert(dp<=Demo::PRESSURE_STEP&&dp>=-(relief?Demo::RELIEF_PRESSURE_STEP:Demo::PRESSURE_STEP));assert(dt<=Demo::TEMPERATURE_STEP&&dt>=-(relief?Demo::RELIEF_TEMPERATURE_STEP:Demo::TEMPERATURE_STEP));
    assert(p>=0&&p<=1000&&t>=0&&t<=3500);rising|=dp>0||dt>0;falling|=dp<0||dt<0;
    state.pressure=p;state.temperature=t;state.evaluate(now);pressureAlarm|=state.causes&1;tempAlarm|=state.causes&2;warning|=state.state()==1;
    if(trace&&(now-1000)%1000==0&&now<=1201000)fprintf(trace,"%lu,%.2f,%.1f,%u,%u,%u\n",(unsigned long)((now-1000)/1000),p/100.0,t/10.0,state.relay,state.state(),simulator.phase);
    previousP=p;previousT=t;
  }if(trace)fclose(trace);assert(pressureAlarm&&tempAlarm&&warning&&falling&&rising);
  simulator.stop();int p=simulator.pressure();simulator.update(4000000,config,false);assert(simulator.pressure()==p);
  // From cold readings, the first real threshold crossing must be reached quickly
  // without forcing an alarm bit or changing the default trip latch.
  uint32_t slowestFirstAlarm=0;
  for(uint8_t node=1;node<=2;node++)for(uint32_t seed=1;seed<=40;seed++){
    Boiler::Safety presentation;presentation.config=config;presentation.ready=true;presentation.pFault=presentation.tFault=false;presentation.pSim=presentation.tSim=true;
    Demo::Sensors fast;fast.start(1000,seed,node,0,0);uint32_t firstAlarm=0;
    for(uint32_t now=1250;now<=46000;now+=250){fast.update(now,presentation.config,presentation.relay);presentation.pressure=fast.pressure();presentation.temperature=fast.temperature();presentation.evaluate(now);if(presentation.causes){firstAlarm=now-1000;break;}}
    assert(firstAlarm&&firstAlarm<=30000);if(firstAlarm>slowestFirstAlarm)slowestFirstAlarm=firstAlarm;
    assert(presentation.trip&&presentation.relay);
  }
  printf("Accelerated demo: first alarm from zero, 80 node/seed cases, worst %lu ms (default thresholds, latch retained).\n",(unsigned long)slowestFirstAlarm);
  Demo::Sensors wrap;uint32_t start=UINT32_MAX-500;wrap.start(start,1234,1,300,1200);wrap.update(start+750,config,false);assert(wrap.pressure()>=300-3*Demo::PRESSURE_STEP&&wrap.pressure()<=300+3*Demo::PRESSURE_STEP);
  assert(Broadcast::classify("T",7,22,7)==Broadcast::REPLY);assert(Broadcast::classify("B",7,0,7)==Broadcast::AUTOMATIC);
  assert(Broadcast::classify("B",0,0,7)==Broadcast::AUTOMATIC); // Unbound slave can broadcast before the first request.
  assert(Broadcast::classify("T",7,0,7)==Broadcast::INVALID&&Broadcast::classify("B",7,22,7)==Broadcast::INVALID);
  assert(Broadcast::classify("B",8,0,7)==Broadcast::INVALID&&Broadcast::classify("C",7,22,7)==Broadcast::INVALID);
  assert(Broadcast::masterMaySend(true,true,1150,1000));assert(!Broadcast::masterMaySend(true,true,1151,1000));
  assert(Broadcast::masterMaySend(true,false,2000,1000)&&Broadcast::masterMaySend(false,true,2000,1000));
  assert(Broadcast::masterMaySend(true,true,20,UINT32_MAX-100));
  assert(Broadcast::slaveMaySend(true,1000,0,500,950));assert(!Broadcast::slaveMaySend(true,999,0,500,950));
  assert(!Broadcast::slaveMaySend(true,1000,0,601,950)&&!Broadcast::slaveMaySend(true,1000,0,500,961));
  assert(!Broadcast::slaveMaySend(false,1000,0,500,950));
  assert(Broadcast::MASTER_WINDOW+(Link::MAX_FRAME+8)*10UL*1000/9600<Broadcast::SLAVE_TX_GUARD);
  Broadcast::Gesture gesture;uint8_t second=7,third=7;
  assert(!gesture.update(true,true,100,second,third)&&!second&&!third);
  assert(!gesture.update(true,true,2099,second,third));assert(gesture.update(true,true,2100,second,third));assert(!gesture.update(true,true,2200,second,third));
  second=third=2;assert(!gesture.update(false,false,2300,second,third)&&!second&&!third);
  assert(!gesture.update(true,true,3000,second,third));assert(!gesture.update(true,false,3500,second,third));assert(!gesture.update(true,true,6000,second,third));
  gesture.update(false,false,6100,second,third);assert(!gesture.update(true,true,6200,second,third));assert(gesture.update(true,true,8200,second,third));
  assert((Boiler::WIRE_FLAGS&Boiler::AUTO_DEMO)&&!(Boiler::WIRE_FLAGS&8192)&&!(Boiler::WIRE_FLAGS&4096));
  Port wire;Link::send(wire,"B,1,7,0,3,104,269,815,16387,0,0,0,7,500,1600,0,0,275,0,500,4000,5000,0,1");
  Link::Receiver rx;char*decoded=nullptr;uint32_t at=100;for(char c:wire.data){char*r=rx.feed(c,at++);if(r)decoded=r;}
  assert(decoded);char*fields[24];assert(Link::split(decoded,fields,24)==24&&!strcmp(fields[0],"B")&&!strcmp(fields[3],"0"));
  puts("Demo broadcast: gradual reproducible trajectories, all warning/alarm scenarios, SIM gate, unsolicited frame/session isolation, safe duplex windows, button chord and rollover passed.");
}
