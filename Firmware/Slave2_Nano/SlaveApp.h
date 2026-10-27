#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include "Protocol.h"
#include "Control.h"
#include "Button.h"
#include "SlaveUi.h"
#include "DemoSensors.h"
#include "Broadcast.h"
#ifndef ENABLE_AVR_WATCHDOG
#define ENABLE_AVR_WATCHDOG 0
#endif
static LiquidCrystal_I2C lcd(0x27,16,2);
static SoftwareSerial masterSerial(2,3);
static Boiler::Safety safety;
static Link::Receiver receiver;
static PanelButton pb1(6),pb2(7),pb3(8);
static Demo::Sensors demoSensors;static Broadcast::Gesture demoGesture;
static uint32_t lastBroadcast=0,lastTx=0,lastRx=0;static bool demoExitPending=false,urgentBroadcast=false;static uint8_t lastAlarmCauses=0;
static SlaveUi slaveUi;static char shadow[2][17];
static uint32_t lastSensor=0,lastLcd=0,lastMaster=0,sampleSeq=0,bootId=0,testStarted=0;
static bool testActive=false,noticeActive=false;
static uint32_t cachedSession=0;static uint16_t cachedSeq=0;static uint8_t cachedResult=Boiler::INVALID;
static bool masterSeen=false;static uint16_t rawPressure=0;
static char notice[17]={};static uint32_t noticeUntil=0;
struct StoredSlave{uint32_t magic;Boiler::Config config;uint32_t boots,generation;uint8_t global,trip;uint16_t crc;};
static_assert(sizeof(StoredSlave)<=64,"EEPROM configuration slot too small");
static StoredSlave stored;
static uint8_t storedSlot=0;
static bool validStored(const StoredSlave&s){return s.magic==0x42535634&&s.global<=1&&s.trip<=1&&s.crc==Link::crc16(&s,offsetof(StoredSlave,crc))&&Boiler::validConfig(s.config);}
static void saveSlave(){
  StoredSlave next={};next.magic=0x42535634;next.config=safety.config;next.boots=bootId;next.generation=stored.generation+1;
  next.global=safety.global;next.trip=safety.trip;next.crc=Link::crc16(&next,offsetof(StoredSlave,crc));
  uint8_t slot=1-storedSlot;int address=slot*64;EEPROM.update(address,0);
  const uint8_t*bytes=(const uint8_t*)&next;for(size_t i=sizeof(next.magic);i<sizeof next;i++)EEPROM.update(address+i,bytes[i]);
  EEPROM.put(address,next.magic);stored=next;storedSlot=slot;
}
static int adcAverage(uint8_t pin){analogRead(pin);long sum=0;for(uint8_t i=0;i<8;i++)sum+=analogRead(pin);return sum/8;}
static bool thermocouple(int16_t&t){
  digitalWrite(10,LOW);delayMicroseconds(2);uint16_t raw=0;
  for(uint8_t i=0;i<16;i++){digitalWrite(A0,HIGH);delayMicroseconds(2);raw=(raw<<1)|digitalRead(12);digitalWrite(A0,LOW);delayMicroseconds(2);}digitalWrite(10,HIGH);
  if(raw==0xFFFF||(raw&0x8006))return false;t=((raw>>3)*5L)/2;return true;
}
static void sampleSensors(){
  bool pSim=digitalRead(A3)==HIGH,tSim=digitalRead(11)==HIGH;
  if(demoSensors.enabled&&(!pSim||!tSim||safety.config.live)){
    demoSensors.stop();demoExitPending=true;strlcpy(notice,"DEMO AUTO OFF",sizeof notice);noticeUntil=millis()+2000;noticeActive=true;
  }
  safety.pSim=pSim;safety.tSim=tSim;
  if(demoSensors.enabled){
    demoSensors.update(millis(),safety.config,safety.relay);safety.pSim=safety.tSim=true;safety.pFault=safety.tFault=false;
    safety.pressure=demoSensors.pressure();safety.temperature=demoSensors.temperature();rawPressure=int32_t(safety.pressure)*1023/1000;
  }else{
    rawPressure=adcAverage(pSim?A2:A6);int32_t mv=rawPressure*long(safety.config.vccMv)/1023;safety.pFault=!pSim&&(mv<350||mv>4650);
    safety.pressure=safety.pFault?-1:(pSim?rawPressure*1000L/1023:constrain((mv-safety.config.zeroMv)*1000L/safety.config.spanMv,0L,1000L));
    if(tSim){safety.temperature=adcAverage(A1)*2000L/1023;safety.tFault=false;}
    else{safety.tFault=!thermocouple(safety.temperature);if(safety.tFault)safety.temperature=-1;}
  }safety.ready=true;sampleSeq++;
}
static bool setDemo(bool enabled){
  if(enabled&&!Demo::allowed(safety.config,safety.ready,digitalRead(A3)==HIGH,digitalRead(11)==HIGH))return false;
  if(enabled&&!demoSensors.enabled){uint32_t now=millis();demoSensors.start(now,bootId^now,SLAVE_ID,safety.pressure,safety.temperature);urgentBroadcast=false;}
  else if(!enabled&&demoSensors.enabled){demoSensors.stop();demoExitPending=true;urgentBroadcast=false;}
  sampleSensors();return true;
}
static bool testing(){return testActive&&uint32_t(millis()-testStarted)<2000;}
static void outputs(){if(demoSensors.enabled&&(safety.causes&~lastAlarmCauses))urgentBroadcast=true;lastAlarmCauses=safety.causes;slaveUi.update(safety.causes&~safety.muted);digitalWrite(9,safety.relay?LOW:HIGH);digitalWrite(4,(safety.relay||testing())?HIGH:LOW);digitalWrite(5,(safety.buzzer||testing())?HIGH:LOW);}
static void telemetry(uint32_t masterSession,uint16_t sequence,uint8_t result,char kind='T'){
  uint32_t started=millis();
  char out[Link::MAX_FRAME];snprintf(out,sizeof out,"%c,%u,%lu,%u,%lu,%lu,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%u,%u,%u",
    kind,SLAVE_ID,(unsigned long)masterSession,sequence,(unsigned long)bootId,(unsigned long)sampleSeq,safety.pressure,safety.temperature,
    safety.flags(testing())|(demoSensors.enabled?Boiler::AUTO_DEMO:0),safety.causes,safety.muted,safety.acknowledged,safety.config.version,safety.config.pressure,
    safety.config.temperature,safety.state(),result,rawPressure,(unsigned long)receiver.errors,safety.config.zeroMv,safety.config.spanMv,safety.config.vccMv,safety.config.live,safety.config.latch);Link::send(masterSerial,out);lastTx=millis();if(kind=='B'){lastBroadcast=started;urgentBroadcast=false;}if(!demoSensors.enabled)demoExitPending=false;
}
static void command(char*payload){
  bool tripBefore=safety.trip;
  char*f[15];uint8_t n=Link::split(payload,f,15);int32_t id,seq;uint32_t incomingSession;
  if(n<5||strcmp(f[0],"C")||!Link::number(f[1],SLAVE_ID,SLAVE_ID,id)||!Link::unsignedNumber(f[2],incomingSession)||!incomingSession||!Link::number(f[3],1,65535,seq))return;
  bool poll=!strcmp(f[4],"POLL");uint8_t result=Boiler::INVALID;bool valid=false;
  if(cachedSession==incomingSession&&cachedSeq==seq){lastMaster=millis();masterSeen=true;telemetry(incomingSession,seq,cachedResult);return;}
  if(cachedSession==incomingSession&&uint16_t(seq)<cachedSeq){telemetry(incomingSession,seq,Boiler::INVALID);return;}
  if(poll&&n==5){result=Boiler::POLLED;valid=true;}
  else if(!strcmp(f[4],"SET")&&n==14){
    int32_t v[9]={};bool ok=true;for(uint8_t i=0;i<9;i++)ok=ok&&Link::number(f[i+5],0,65535,v[i]);
    Boiler::Config c;c.pressure=v[0];c.temperature=v[1];c.version=v[2];c.live=v[3];c.latch=v[4];c.zeroMv=v[5];c.spanMv=v[6];c.vccMv=v[7];
    if(ok&&v[8]==2&&v[3]<=1&&v[4]<=1&&Boiler::validConfig(c)){
      valid=true;if(c.live&&(safety.pSim||safety.tSim))result=Boiler::BLOCKED;else{safety.config=c;saveSlave();result=Boiler::APPLIED;}
    }
  }else{
    int32_t value=0;bool arg=n==6&&Link::number(f[5],0,1,value);
    if(!strcmp(f[4],"PURGE")&&arg){safety.remote=value;result=(!value&&(safety.causes||safety.global||safety.trip||safety.local))?Boiler::BLOCKED:Boiler::APPLIED;valid=true;}
    else if(!strcmp(f[4],"GLOBAL")&&arg){valid=true;if(value){safety.global=true;result=Boiler::APPLIED;}else result=safety.releaseGlobal(millis())?Boiler::APPLIED:Boiler::BLOCKED;saveSlave();}
    else if(!strcmp(f[4],"DEMO")&&arg){valid=true;result=setDemo(value)?Boiler::APPLIED:Boiler::BLOCKED;}
    else if(!strcmp(f[4],"MUTE")&&n==5){safety.silence();result=Boiler::APPLIED;valid=true;}
    else if(!strcmp(f[4],"ACK")&&n==5){safety.acknowledge();result=Boiler::APPLIED;valid=true;}
    else if(!strcmp(f[4],"RESET")&&n==5){result=safety.reset(millis())?Boiler::APPLIED:Boiler::BLOCKED;valid=true;saveSlave();}
    else if(!strcmp(f[4],"TEST")&&n==5){valid=true;if(!safety.config.live&&safety.pSim&&safety.tSim&&!safety.causes&&!safety.trip){testStarted=millis();testActive=true;result=Boiler::APPLIED;}else result=Boiler::BLOCKED;}
  }
  if(valid){lastMaster=millis();masterSeen=true;}safety.evaluate(millis());if(tripBefore!=safety.trip)saveSlave();outputs();
  cachedSession=incomingSession;cachedSeq=seq;cachedResult=result;telemetry(incomingSession,seq,result);
}
static void lcdLine(uint8_t row,char*line){size_t n=strlen(line);bool positioned=false;
  for(uint8_t i=0;i<16;i++){char c=i<n?line[i]:' ';if(shadow[row][i]!=c){if(!positioned)lcd.setCursor(i,row);lcd.write(c);shadow[row][i]=c;positioned=true;}else positioned=false;}}
static void serviceMaster(){
  if(masterSerial.overflow()){receiver.receiving=false;receiver.length=0;receiver.errors++;}
  uint8_t budget=128;while(budget--&&masterSerial.available()){lastRx=millis();char*p=receiver.feed(masterSerial.read(),lastRx);if(p)command(p);}
}
static void serviceBroadcast(){
  if(!demoSensors.enabled&&!demoExitPending)return;uint32_t now=millis();
  uint32_t due=(demoExitPending||urgentBroadcast)?now-Broadcast::INTERVAL:lastBroadcast;
  if(Broadcast::slaveMaySend(true,now,due,lastTx,lastRx))telemetry(cachedSession,0,Boiler::POLLED,'B');
}
static void renderLcd(){
  if(millis()-lastLcd<250)return;lastLcd=millis();char a[24],b[24];uint8_t page=slaveUi.visible();
  if(page==0){
    if(safety.pFault)snprintf(a,sizeof a,"B%u P:FAULT [%c]",SLAVE_ID,safety.pSim?'S':'R');else snprintf(a,sizeof a,"B%u P:%u.%02u [%c]",SLAVE_ID,safety.pressure/100,safety.pressure%100,safety.pSim?'S':'R');
    if(safety.tFault)snprintf(b,sizeof b,"T:FAULT[%c] R:%s",safety.tSim?'S':'R',safety.relay?"ON":"OFF");else snprintf(b,sizeof b,safety.temperature>=10000?"T:%u.%u[%c]R:%s":"T:%u.%u[%c] R:%s",safety.temperature/10,safety.temperature%10,safety.tSim?'S':'R',safety.relay?"ON":"OFF");
  }else if(page==1){
    uint8_t selectedCause=0;for(uint8_t j=0;j<5;j++){uint8_t k=(millis()/1500+j)%5;if(safety.causes&(1<<k)){selectedCause=1<<k;break;}}
    const char*reason=selectedCause==1?PSTR("PRESSURE HIGH"):selectedCause==2?PSTR("TEMP HIGH"):selectedCause==4?PSTR("PRESSURE FAULT"):selectedCause==8?PSTR("TEMP FAULT"):selectedCause==16?PSTR("SIM IN LIVE"):safety.trip?PSTR("TRIP LATCHED"):PSTR("NO ACTIVE ALARM");
    strcpy_P(a,reason);snprintf(b,sizeof b,"M:%02X A:%02X L:%u",safety.muted,safety.acknowledged,safety.trip);if(safety.trip&&!safety.causes)strcpy_P(b,PSTR("PB1 HOLD: RESET"));
  }else if(page==2){snprintf(a,sizeof a,"RELAY:%s %s",safety.relay?"ON":"OFF",safety.global?"GLOBAL":safety.causes?"AUTO":safety.trip?"TRIP":safety.local?"LOCAL":safety.remote?"REMOTE":"IDLE");snprintf(b,sizeof b,"LOCAL:%u REMOTE:%u",safety.local,safety.remote);
  }else if(page==3){snprintf(a,sizeof a,"MASTER:%s%s",masterSeen&&millis()-lastMaster<5000?"OK":"--",demoSensors.enabled?" AUTO":"");snprintf(b,sizeof b,"TH:%u.%uB/%uC",safety.config.pressure/100,(safety.config.pressure%100)/10,safety.config.temperature/10);}
  else{strcpy(a,"GUIDEBOOK ONLINE");strcpy(b,"SCAN QR MASTER");}
  if(noticeActive&&int32_t(noticeUntil-millis())>0)strlcpy(b,notice,sizeof b);else noticeActive=false;lcdLine(0,a);lcdLine(1,b);
}
void setup(){
  MCUSR=0;wdt_disable();digitalWrite(9,HIGH);pinMode(9,OUTPUT);pinMode(4,OUTPUT);pinMode(5,OUTPUT);
  for(uint8_t p=6;p<=8;p++)pinMode(p,INPUT_PULLUP);pinMode(11,INPUT_PULLUP);pinMode(A3,INPUT_PULLUP);pinMode(10,OUTPUT);pinMode(A0,OUTPUT);pinMode(12,INPUT);digitalWrite(10,HIGH);digitalWrite(A0,LOW);
  StoredSlave a,b;EEPROM.get(0,a);EEPROM.get(64,b);bool va=validStored(a),vb=validStored(b);
  if(va||vb){storedSlot=vb&&(!va||int32_t(b.generation-a.generation)>0)?1:0;stored=storedSlot?b:a;safety.config=stored.config;bootId=stored.boots+1;safety.global=stored.global;safety.trip=stored.trip;}
  else{stored=StoredSlave();bootId=1;}
  saveSlave();safety.safeSince=millis();safety.evaluate(millis());outputs();Wire.begin();Wire.setWireTimeout(25000,true);lcd.init();lcd.backlight();masterSerial.begin(9600);lastSensor=millis();
#if ENABLE_AVR_WATCHDOG
  wdt_enable(WDTO_2S);
#endif
}
void loop(){
  wdt_reset();serviceMaster();uint32_t now=millis();bool wasTrip=safety.trip;
  if(now-lastSensor>=250){lastSensor=now;sampleSensors();}uint8_t a=pb1.update(now),b=pb2.update(now),c=pb3.update(now);
  if(demoGesture.update(pb2.stable,pb3.stable,now,b,c)){
    bool ok=setDemo(!demoSensors.enabled);strlcpy(notice,!ok?"DEMO: PILIH SIM":demoSensors.enabled?"DEMO AUTO ON":"DEMO AUTO OFF",sizeof notice);noticeUntil=now+2000;noticeActive=true;
  }
  if(a&2){safety.local=!safety.local;strlcpy(notice,safety.local?"LOCAL PURGE ON":(safety.causes||safety.trip||safety.global||safety.remote)?"HELD BY PRIORITY":"LOCAL PURGE OFF",sizeof notice);noticeUntil=now+2000;noticeActive=true;}
  if(a&4){strlcpy(notice,safety.reset(now)?"TRIP RESET OK":"RESET BLOCKED",sizeof notice);noticeUntil=now+2000;noticeActive=true;}
  if(testActive&&!testing())testActive=false;
  if(b&2)safety.silence();if(b&4)safety.acknowledge();if(c&2)slaveUi.next();if(c&4)slaveUi.home();
  safety.evaluate(now);if(safety.trip!=wasTrip)saveSlave();outputs();
  serviceMaster();serviceBroadcast();renderLcd();
}
