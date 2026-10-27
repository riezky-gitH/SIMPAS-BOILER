#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <esp_system.h>
#include <time.h>
#include "Protocol.h"
#include "Control.h"
#include "Button.h"
#include "Broadcast.h"
#include "History.h"
#include "OfflineUsb.h"
#include "GuideQr.h"
#include "IndexedFrame.h"
#include "MasterScreen.h"
#include "NetworkConfig.h"
#include "dashboard_html.h"

namespace App {
using Screen::BG;using Screen::PAPER;using Screen::INK;using Screen::WHITE;using Screen::MUTED;using Screen::ACCENT;using Screen::RED;using Screen::GREEN;using Screen::AMBER;using Screen::LINE;using Screen::FOOTER;
static WebServer server(80);static Preferences prefs;static Historian::Store history;
// Hardware SPI; rotation 3 turns the previous landscape view by 180 degrees.
static Adafruit_ST7789 tft(10,9,14);
class Panel:public Adafruit_GFX {
  uint8_t*previous=nullptr;bool previousValid=false,memoryErrorShown=false;
  uint16_t transfer[Screen::WIDTH*Screen::TILE_H];
 public:
  uint8_t*pixels=nullptr;Panel():Adafruit_GFX(Screen::WIDTH,Screen::HEIGHT){}
  void begin(){
    if(psramFound()){pixels=(uint8_t*)heap_caps_malloc(Screen::BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);previous=(uint8_t*)heap_caps_malloc(Screen::BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
    if(!pixels||!previous){free(pixels);free(previous);pixels=previous=nullptr;
      if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)>Screen::BYTES*2+96*1024){
        pixels=(uint8_t*)heap_caps_malloc(Screen::BYTES,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);previous=(uint8_t*)heap_caps_malloc(Screen::BYTES,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);}}
    if(!pixels||!previous){free(pixels);free(previous);pixels=previous=nullptr;}
    Screen::fill(pixels,Screen::index(BG));
    Serial.printf("[TFT] indexed double buffer: %s; frame bytes: %u; PSRAM: %u; heap: %u\n",pixels?"ready":"unavailable",unsigned(Screen::BYTES),ESP.getPsramSize(),ESP.getFreeHeap());
  }
  void drawPixel(int16_t x,int16_t y,uint16_t c)override{Screen::pixel(pixels,x,y,Screen::index(c));}
  void fillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c)override{Screen::rect(pixels,x,y,w,h,Screen::index(c));}
  void fillScreen(uint16_t c)override{Screen::fill(pixels,Screen::index(c));}
  void invalidate(){previousValid=false;memoryErrorShown=false;}
  bool ready(){
    if(pixels)return true;
    if(!memoryErrorShown){tft.fillScreen(BG);tft.setTextSize(1);tft.setTextColor(INK);tft.setCursor(8,40);tft.print("Memori layar tidak cukup");tft.setCursor(8,60);tft.print("Aktifkan OPI PSRAM / restart");memoryErrorShown=true;}
    return false;
  }
  void present(){
    Screen::present(pixels,previous,!previousValid,transfer,[](int x,int y,int w,int h,uint16_t*data){tft.drawRGBBitmap(x,y,data,w,h);});previousValid=true;
  }
};
static void bootScreen(const char*stage){
  Serial.printf("[BOOT] %s\n",stage);
  // Draw directly: diagnostics must not depend on PSRAM, filesystem or WiFi.
  tft.fillScreen(BG);tft.fillRect(0,0,320,36,ACCENT);tft.setTextWrap(false);tft.setTextColor(WHITE);tft.setTextSize(2);
  tft.setCursor(MasterScreen::SIDE_PAD,12);tft.print("SIMPAS");
  tft.setTextSize(1);tft.setTextColor(INK);tft.setCursor(MasterScreen::SIDE_PAD,52);tft.print(stage);

}
static void initTft(){
  SPI.begin(12,-1,11,10);tft.init(172,320);tft.setRotation(3);
  tft.setSPISpeed(20000000); // Conservative clock for jumper-wire connections.
}
static Panel panel;
struct Pending {bool active=false;uint16_t seq=0;uint8_t tries=0;uint32_t sent=0;char operation[112]={};Boiler::Config proposed;};
struct Node {
  Boiler::Safety report;Boiler::Config saved;Pending pending;Link::Receiver receiver;
  bool seen=false,connected=false,globalWanted=false,hasSaved=false,automatic=false;
  uint8_t state=4;uint16_t sequence=0,lastCommandSeq=0,adc=0;Link::PollCycle poll;
  uint32_t session=0,boot=0,sample=0,lastSeen=0,lastWire=0,nextPoll=0,packets=0,slaveErrors=0,pollTimeouts=0,disconnects=0;
  char commandStatus[24]="NONE",lastOperation[112]={};
};
static Node nodes[2];static uint32_t boot=0,lastRecord=0,lastDisplay=0,lastRequest=0;
static PanelButton pb1(5),pb2(6),pb3(7);static uint8_t page=0,selected=0;
static bool wasGlobalAtPress=false,wifiEnabled=true;static float knob=0;static uint32_t knobAt=0;
static char operatorToken[7]="123456",apPassword[17]="00000000";
// Only Arduino's loop owns nodes, preferences, events and control decisions.
// HTTP/USB/serial storage I/O runs on core 0 and uses queues for mutations.
enum RpcKind:uint8_t {RPC_COMMAND,RPC_CONFIG,RPC_TIME,RPC_WIFI_OFF,RPC_WIFI_ON};
struct RpcRequest{uint32_t id=0,queued=0;RpcKind kind=RPC_COMMAND;char payload[769]={};};
struct RpcReply{uint32_t id=0;uint16_t status=503;char body[240]="{}";};
static QueueHandle_t rpcRequests=nullptr,rpcReplies=nullptr;
static SemaphoreHandle_t snapshotLock=nullptr;
static String dataSnapshot,eventSnapshot;
static uint32_t maxLinkGap=0,lastLinkService=0,lastSnapshot=0,cachedEventId=UINT32_MAX;
static bool ioStarted=false;
struct TftSnapshot {Node nodes[2];uint8_t page=0,selected=0;float knob=0;};
static TftSnapshot tftSnapshot;
static void replySend(RpcReply&r,int status,const char*,const char*body){r.status=status;strlcpy(r.body,body,sizeof r.body);}
static void runRpc(RpcKind kind,const char*payload="");

struct Event {uint32_t id,epoch,seconds;uint8_t node;char type[24],detail[100];};
static Event events[128];static uint32_t eventId=0;
static uint32_t uptimeSeconds(){return uint32_t(esp_timer_get_time()/1000000ULL);}
static uint32_t epoch(){time_t t=time(nullptr);return t>1700000000?uint32_t(t):0;}
static void log(uint8_t node,const char*type,const char*detail){
  Event&e=events[eventId%128];e.id=++eventId;e.epoch=epoch();e.seconds=uptimeSeconds();e.node=node;
  strlcpy(e.type,type,sizeof e.type);strlcpy(e.detail,detail,sizeof e.detail);
  String safeDetail=detail;safeDetail.replace("\"","\"\"");char row[256];snprintf(row,sizeof row,"%lu,%lu,%lu,%u,%s,\"%s\"\n",(unsigned long)e.epoch,(unsigned long)boot,(unsigned long)e.seconds,node,type,safeDetail.c_str());history.addEvent(row);
}
static bool globalActive(){for(auto&n:nodes)if(n.globalWanted||(n.connected&&n.report.global))return true;return false;}
static uint16_t nextSeq(Node&n){if(n.sequence==65535){n.session=esp_random()|1;n.sequence=0;}return ++n.sequence;}
static HardwareSerial&port(uint8_t i){return i?Serial2:Serial1;}
static void transmit(uint8_t i,uint16_t sequence,const char*operation){char payload[160];snprintf(payload,sizeof payload,"C,%u,%lu,%u,%s",i+1,(unsigned long)nodes[i].session,sequence,operation);Link::send(port(i),payload);}
static bool enqueue(uint8_t i,const char*operation,const Boiler::Config*proposed=nullptr,bool priority=false){
  Node&n=nodes[i];if(n.pending.active&&!priority)return false;
  if(n.pending.active)log(i+1,"CANCELLED",n.pending.operation);
  n.pending=Pending();n.pending.active=true;n.pending.seq=nextSeq(n);strlcpy(n.pending.operation,operation,sizeof n.pending.operation);
  if(proposed)n.pending.proposed=*proposed;
  strlcpy(n.lastOperation,operation,sizeof n.lastOperation);strlcpy(n.commandStatus,"QUEUED",sizeof n.commandStatus);n.lastCommandSeq=n.pending.seq;
  log(i+1,"COMMAND_QUEUED",operation);return true;
}
static bool request(uint8_t node,const String&action){
  bool known=action=="PURGE,1"||action=="PURGE,0"||action=="GLOBAL,1"||action=="GLOBAL,0"||action=="MUTE"||action=="ACK"||action=="RESET"||action=="TEST"||action=="DEMO,1"||action=="DEMO,0";
  if(!known||node>2)return false;bool priority=action=="GLOBAL,1";
  for(uint8_t i=0;i<2;i++)if((node==0||node==i+1)&&nodes[i].pending.active&&!priority)return false;
  for(uint8_t i=0;i<2;i++)if(node==0||node==i+1){
    if(priority){nodes[i].globalWanted=true;prefs.putBool(i?"global2":"global1",true);}
    enqueue(i,action.c_str(),nullptr,priority);
  }return true;
}
static void saveConfig(uint8_t i,const Boiler::Config&c){nodes[i].saved=c;nodes[i].hasSaved=true;prefs.putBytes(i?"config2":"config1",&c,sizeof c);}
static bool configEqual(const Boiler::Config&a,const Boiler::Config&b){return a.version==b.version&&a.pressure==b.pressure&&a.temperature==b.temperature&&a.zeroMv==b.zeroMv&&a.spanMv==b.spanMv&&a.vccMv==b.vccMv&&a.live==b.live&&a.latch==b.latch;}
static HistoryTiming::Schedule historySchedule;
static void record(bool immediate=false);
static void receive(uint8_t i,char*payload){
  char*f[24];if(Link::split(payload,f,24)!=24)return;
  Node&n=nodes[i];uint32_t s,b,sp,err;int32_t v[24]={};
  if(!Link::number(f[1],i+1,i+1,v[1])||!Link::unsignedNumber(f[2],s)||!Link::number(f[3],0,65535,v[3]))return;
  if(!strcmp(f[0],"T")||!strcmp(f[0],"B"))n.lastWire=millis(); // TX phase is separate from accepted telemetry/session freshness.
  Broadcast::Kind kind=Broadcast::classify(f[0],s,v[3],n.session);if(kind==Broadcast::INVALID)return;
  bool automatic=kind==Broadcast::AUTOMATIC;
  bool commandReply=!automatic&&n.pending.active&&v[3]==n.pending.seq;
  if(!automatic&&!commandReply&&!n.poll.matches(v[3]))return;
  if(!Link::unsignedNumber(f[4],b)||!b||!Link::unsignedNumber(f[5],sp)||!Link::unsignedNumber(f[18],err))return;
  const int32_t lo[]={-1,-1,0,0,0,0,1,50,500,0,0,0};
  const int32_t hi[]={1000,10240,Boiler::WIRE_FLAGS,31,31,31,65535,1000,3000,4,4,1023};
  for(uint8_t j=6;j<=17;j++)if(!Link::number(f[j],lo[j-6],hi[j-6],v[j]))return;
  for(uint8_t j=19;j<24;j++)if(!Link::number(f[j],0,65535,v[j]))return;
  Boiler::Config c;c.version=v[12];c.pressure=v[13];c.temperature=v[14];c.zeroMv=v[19];c.spanMv=v[20];c.vccMv=v[21];c.live=v[22];c.latch=v[23];
  if(v[22]>1||v[23]>1||!Boiler::validConfig(c)||(v[10]&~v[9])||(v[11]&~v[9]))return;
  if(uint16_t(v[8])&~Boiler::WIRE_FLAGS)return;
  if((v[8]&Boiler::AUTO_DEMO)&&(c.live||!(v[8]&Boiler::P_SIM)||!(v[8]&Boiler::T_SIM)))return;
  if(automatic&&!(v[8]&Boiler::AUTO_DEMO)&&!n.automatic)return;
  if(automatic&&n.seen&&b==n.boot&&int32_t(sp-n.sample)<=0)return;
  uint8_t previous=n.report.causes;bool wasConnected=n.connected,wasAutomatic=n.automatic;
  if(n.seen&&b!=n.boot)log(i+1,"SLAVE_RESTART","Slave boot ID changed");
  if(!automatic&&!commandReply)n.poll.complete(v[3]);
  n.boot=b;n.sample=sp;n.lastSeen=millis();n.seen=true;n.connected=true;n.packets++;n.slaveErrors=err;n.adc=v[17];n.state=v[15];
  auto&r=n.report;r.config=c;r.pressure=v[6];r.temperature=v[7];uint16_t flags=v[8];
  r.pSim=flags&Boiler::P_SIM;r.tSim=flags&Boiler::T_SIM;r.pFault=flags&Boiler::P_BAD;r.tFault=flags&Boiler::T_BAD;
  r.relay=flags&Boiler::RELAY_ON;r.local=flags&Boiler::LOCAL;r.remote=flags&Boiler::REMOTE;r.global=flags&Boiler::GLOBAL;
  n.automatic=flags&Boiler::AUTO_DEMO;
  if(!automatic)n.nextPoll=millis()+(n.automatic?Broadcast::KEEPALIVE:100);
  else if(wasAutomatic&&!n.automatic)n.nextPoll=millis()+100;
  r.trip=flags&Boiler::TRIP;r.buzzer=flags&Boiler::BUZZER;r.ready=!(flags&Boiler::STARTUP);r.causes=v[9];r.muted=v[10];r.acknowledged=v[11];
  uint8_t historyMask=r.causes|(r.relay?32:0);
  if(!r.pFault&&r.pressure>=r.config.pressure)historyMask|=64;
  if(!r.tFault&&r.temperature>=r.config.temperature)historyMask|=128;
  if(historySchedule.observe(i,historyMask))record(true);
  if(!wasConnected)log(i+1,"ONLINE",automatic?"Valid automatic telemetry":"Valid bidirectional reply");
  if(r.causes!=previous){char detail[40];snprintf(detail,sizeof detail,"cause mask %u -> %u",previous,r.causes);log(i+1,r.causes?"ALARM_CHANGE":"ALARM_RESTORED",detail);
    if(r.causes&~previous){Historian::Alarm a;a.epoch=epoch();a.boot=boot;a.seconds=uptimeSeconds();a.node=i+1;a.causes=r.causes;history.addAlarm(a);}}
  if(commandReply){
    uint8_t result=v[16];const char*status=result==Boiler::APPLIED?"APPLIED":result==Boiler::BLOCKED?"BLOCKED":result==Boiler::BUSY?"BUSY":"INVALID";
    strlcpy(n.commandStatus,status,sizeof n.commandStatus);log(i+1,status,n.pending.operation);
    if(result==Boiler::APPLIED&&!strncmp(n.pending.operation,"SET,",4)){
      if(configEqual(c,n.pending.proposed))saveConfig(i,c);else{strlcpy(n.commandStatus,"CONFIG_MISMATCH",sizeof n.commandStatus);log(i+1,"CONFIG_MISMATCH","ACK values differ from proposed configuration");}}
    if(result==Boiler::APPLIED&&!strcmp(n.pending.operation,"GLOBAL,0")){n.globalWanted=false;prefs.putBool(i?"global2":"global1",false);}
    n.pending.active=false;n.nextPoll=millis()+(n.automatic?Broadcast::KEEPALIVE:100);
  }else if(!strcmp(n.commandStatus,"TIMEOUT")){
    if(!strncmp(n.lastOperation,"SET,",4)&&configEqual(c,n.pending.proposed)){saveConfig(i,c);strlcpy(n.commandStatus,"RECONCILED",sizeof n.commandStatus);log(i+1,"CONFIG_OBSERVED","Effective slave configuration matches proposal after lost ACK");}
    if(!strcmp(n.lastOperation,"GLOBAL,0")&&!r.global){n.globalWanted=false;prefs.putBool(i?"global2":"global1",false);strlcpy(n.commandStatus,"OBSERVED_RELEASE",sizeof n.commandStatus);log(i+1,"GLOBAL_OBSERVED","Slave reports global request released after lost ACK");}
  }
}
static void serviceLinks(){
  uint32_t tick=millis();if(lastLinkService)maxLinkGap=max(maxLinkGap,uint32_t(tick-lastLinkService));lastLinkService=tick;
  for(uint8_t i=0;i<2;i++){
    Node&n=nodes[i];uint16_t budget=512;
    while(budget--&&port(i).available()){char*p=n.receiver.feed(port(i).read(),millis());if(p)receive(i,p);}
    uint32_t now=millis();
    if(n.connected&&uint32_t(now-n.lastSeen)>=3000){n.connected=false;n.disconnects++;n.nextPoll=now;log(i+1,"OFFLINE","No valid telemetry for 3 seconds");}
    // One request on each SoftwareSerial link at a time, including POLL.
    if(n.poll.waiting){if(!n.poll.expired(now))continue;n.poll.waiting=false;n.pollTimeouts++;n.nextPoll=now+60;}
    if(n.connected&&n.globalWanted&&!n.report.global&&!n.pending.active)enqueue(i,"GLOBAL,1",nullptr,true);
    Pending&p=n.pending;
    if(p.active){if(!p.tries||uint32_t(now-p.sent)>=700){if(p.tries>=3){strlcpy(n.commandStatus,"TIMEOUT",sizeof n.commandStatus);log(i+1,"TIMEOUT",p.operation);p.active=false;n.nextPoll=now+100;}
      else if(Broadcast::masterMaySend(n.automatic,n.connected,now,n.lastWire)){transmit(i,p.seq,p.operation);p.tries++;p.sent=now;strlcpy(n.commandStatus,"SENT",sizeof n.commandStatus);}}}
    else if(int32_t(now-n.nextPoll)>=0&&Broadcast::masterMaySend(n.automatic,n.connected,now,n.lastWire)){
      if(n.connected&&n.globalWanted&&!n.report.global)enqueue(i,"GLOBAL,1",nullptr,true);
      else{uint16_t seq=nextSeq(n);n.poll.start(seq,now);transmit(i,seq,"POLL");}
    }
  }
}
static String causeText(uint8_t causes){String s;if(causes&1)s+="PRESSURE_HIGH ";if(causes&2)s+="TEMP_HIGH ";if(causes&4)s+="PRESSURE_FAULT ";if(causes&8)s+="TEMP_FAULT ";if(causes&16)s+="SIM_IN_LIVE ";return s.length()?s:"NONE";}
static String compactCause(uint8_t c){String s;if(c&1)s+="P-HI ";if(c&2)s+="T-HI ";if(c&4)s+="P-ERR ";if(c&8)s+="T-ERR ";if(c&16)s+="SIM/LIVE ";return s.length()?s:"NONE";}
static const char*stateText(const Node&n){if(!n.connected)return"OFFLINE";switch(n.state){case 0:return"NORMAL";case 1:return"WARNING";case 2:return"ALARM";case 3:return"FAULT";default:return"STARTUP";}}
static void nodeJson(JsonObject j,const Node&n){
  const auto&r=n.report;j["demo_auto"]=n.connected&&n.automatic;j["connected"]=n.connected;j["state"]=stateText(n);j["age_ms"]=n.seen?millis()-n.lastSeen:0;
  if(!n.connected||r.pFault)j["pressure"]=nullptr;else j["pressure"]=r.pressure/100.0;
  if(!n.connected||r.tFault)j["temp"]=nullptr;else j["temp"]=r.temperature/10.0;
  j["src_p"]=r.pSim?"SIM":"REAL";j["src_t"]=r.tSim?"SIM":"REAL";
  j["quality_p"]=!n.connected?"STALE":r.pFault?"FAULT":"GOOD";j["quality_t"]=!n.connected?"STALE":r.tFault?"FAULT":"GOOD";
  j["relay"]=n.connected?(r.relay?"ON":"OFF"):"UNKNOWN";j["valve_feedback"]="NOT_INSTALLED";
  j["buzzer"]=n.connected&&r.buzzer;j["causes"]=r.causes;j["cause_text"]=n.connected?causeText(r.causes):"LINK_LOST";
  j["muted"]=r.muted;j["acknowledged"]=r.acknowledged;j["trip"]=r.trip;j["local"]=r.local;j["remote"]=r.remote;j["global"]=r.global;
  j["command_id"]=n.lastCommandSeq;j["command_status"]=n.commandStatus;j["operation"]=n.lastOperation;
  j["poll_timeouts"]=n.pollTimeouts;j["disconnects"]=n.disconnects;j["packets"]=n.packets;j["crc_errors"]=n.receiver.errors;j["slave_errors"]=n.slaveErrors;j["boot"]=n.boot;j["sample"]=n.sample;j["raw_adc"]=n.adc;
  auto c=j["config"].to<JsonObject>();c["version"]=r.config.version;c["pressure"]=r.config.pressure/100.0;c["temp"]=r.config.temperature/10.0;
  j["config_mismatch"]=n.hasSaved&&!configEqual(n.saved,r.config);
  c["live"]=r.config.live;c["latch"]=r.config.latch;c["zero_mv"]=r.config.zeroMv;c["span_mv"]=r.config.spanMv;c["vcc_mv"]=r.config.vccMv;
}
static void dataJson(JsonDocument&d){
  d["clock_synced"]=bool(epoch());d["timezone"]="Asia/Jakarta";d["schema"]=3;d["boot"]=boot;d["epoch"]=epoch();d["uptime"]=uptimeSeconds();d["ap_clients"]=WiFi.softAPgetStationNum();d["purge_all"]=globalActive();
  bool alarm=(nodes[0].connected&&(nodes[0].state==2||nodes[0].state==3))||(nodes[1].connected&&(nodes[1].state==2||nodes[1].state==3));
  d["overall"]=alarm?"ALARM":(!nodes[0].connected||!nodes[1].connected)?"DEGRADED":(nodes[0].state==4||nodes[1].state==4)?"STARTUP":(nodes[0].state==1||nodes[1].state==1)?"WARNING":"NORMAL";
  nodeJson(d["slave1"].to<JsonObject>(),nodes[0]);nodeJson(d["slave2"].to<JsonObject>(),nodes[1]);
  auto h=d["history"].to<JsonObject>();h["mounted"]=history.mounted;h["ready"]=history.ready;h["psram"]=history.psram;h["capacity_records"]=history.capacity;
  h["flash_bytes"]=history.totalBytes;h["budget_bytes"]=Historian::FLASH_BUDGET;h["pending_est_bytes"]=history.approxPending;h["write_error"]=history.writeError;h["paused_by_snapshot"]=history.paused;
  h["dropped"]=history.dropped;h["corrupt"]=history.corrupt;h["rotated_segments"]=history.rotated;h["flushes"]=history.flushes;h["last_flush_age_ms"]=millis()-history.lastFlush;
  h["event_drops"]=history.eventDrops;
  h["row_bytes"]=history.lastRowBytes;h["estimated_normal_days"]=float(Historian::FLASH_BUDGET)/max(uint16_t(1),history.lastRowBytes)*5/86400;
  h["interval_s"]=5;h["immediate_threshold"]=true;
  h["usb_status"]=OfflineUsb::statusText();h["usb_snapshot"]=bool(OfflineUsb::mounted);h["usb_bytes"]=OfflineUsb::mounted?history.frozenSize:0;
  auto peaks=h["peaks"].to<JsonArray>();for(uint8_t i=0;i<2;i++){auto p=peaks.add<JsonObject>();if(history.peakP[i]>=0)p["pressure"]=history.peakP[i]/100.0;else p["pressure"]=nullptr;if(history.peakT[i]>=0)p["temp"]=history.peakT[i]/10.0;else p["temp"]=nullptr;}
  auto alarms=h["alarms"].to<JsonArray>();for(auto&a:history.alarms)if(a.node){auto j=alarms.add<JsonObject>();j["node"]=a.node;j["epoch"]=a.epoch;j["boot"]=a.boot;j["uptime"]=a.seconds;j["causes"]=a.causes;}
  d["max_uart_gap_ms"]=maxLinkGap;d["io_task"]=ioStarted;d["free_heap"]=ESP.getFreeHeap();d["free_psram"]=ESP.getFreePsram();d["reset_reason"]=int(esp_reset_reason());
  d["knob_pressure"]=roundf((1+knob*7)*10)/10;d["knob_temp"]=roundf(50+knob*250);d["guide_url"]=GUIDE_URL;
}
static bool authorized(){
  String origin=server.header("Origin");if(origin.length()&&origin!=String("http://")+server.hostHeader()){server.send(403,"application/json","{\"error\":\"Origin rejected\"}");return false;}
  if(server.header("Authorization")!=String("Bearer ")+operatorToken){server.send(401,"application/json","{\"error\":\"Operator token required; see master network page or USB TOKEN\"}");return false;}
  return true;
}
static bool body(JsonDocument&d){if(!authorized())return false;if(server.arg("plain").length()>768||deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"error\":\"Invalid JSON\"}");return false;}return true;}
static bool exportActive=false;static uint32_t exportId=0,exportTouched=0;
static void serviceControl();
static void sendJson(const JsonDocument&d,int status=200){String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.send(status,"application/json",out);}
static bool configRequest(uint8_t i,const Boiler::Config&c){char op[112];snprintf(op,sizeof op,"SET,%u,%u,%u,%u,%u,%u,%u,%u,2",c.pressure,c.temperature,c.version,c.live,c.latch,c.zeroMv,c.spanMv,c.vccMv);return enqueue(i,op,&c);}
static void executeCommand(const JsonDocument&d,RpcReply&reply){
    if(!d["node"].is<int>()||!d["action"].is<const char*>()||d["node"].as<int>()<0||d["node"].as<int>()>2){replySend(reply,400,"application/json","{\"error\":\"Invalid node/action\"}");return;}
    if(millis()-lastRequest<150){replySend(reply,429,"application/json","{\"error\":\"Wait before next command\"}");return;}
    bool ok=request(d["node"].as<int>(),String(d["action"].as<const char*>()));if(!ok){replySend(reply,409,"application/json","{\"error\":\"Invalid action or command already pending\"}");return;}
    lastRequest=millis();JsonDocument response;response["status"]="QUEUED";response["id1"]=nodes[0].lastCommandSeq;response["id2"]=nodes[1].lastCommandSeq;reply.status=202;serializeJson(response,reply.body,sizeof reply.body);
  }
static void executeConfig(const JsonDocument&d,RpcReply&reply){int node=d["node"].is<int>()?d["node"].as<int>():0;
    if(node<1||node>2||!nodes[node-1].connected){replySend(reply,409,"application/json","{\"error\":\"Online node 1 or 2 required\"}");return;}
    if(!d["pressure"].is<float>()||!d["temp"].is<float>()){replySend(reply,400,"application/json","{\"error\":\"Numeric pressure/temp required\"}");return;}
    float p=d["pressure"],t=d["temp"];if(!isfinite(p)||!isfinite(t)||p<0.5||p>10||t<50||t>300){replySend(reply,400,"application/json","{\"error\":\"Setpoint out of range\"}");return;}
    auto c=nodes[node-1].report.config;c.pressure=lroundf(p*100);c.temperature=lroundf(t*10);c.version=c.version==65535?1:c.version+1;
    const char*names[]={"live","latch","zero_mv","span_mv","vcc_mv"};uint16_t values[]={c.live,c.latch,c.zeroMv,c.spanMv,c.vccMv};
    for(uint8_t i=0;i<5;i++)if(!d[names[i]].isNull()){if(!d[names[i]].is<unsigned int>()||d[names[i]].as<unsigned int>()>65535){replySend(reply,400,"application/json","{\"error\":\"Invalid config field\"}");return;}values[i]=d[names[i]];}
    if(values[0]>1||values[1]>1){replySend(reply,400,"application/json","{\"error\":\"Mode/latch must be 0 or 1\"}");return;}
    c.live=values[0];c.latch=values[1];c.zeroMv=values[2];c.spanMv=values[3];c.vccMv=values[4];
    if(!Boiler::validConfig(c)){replySend(reply,400,"application/json","{\"error\":\"Calibration out of range\"}");return;}
    if(!configRequest(node-1,c)){replySend(reply,409,"application/json","{\"error\":\"Command pending\"}");return;}replySend(reply,202,"application/json","{\"status\":\"QUEUED\"}");
  }
static void executeTime(const JsonDocument&d,RpcReply&reply){if(!d["epoch"].is<uint32_t>()||d["epoch"].as<uint32_t>()<1700000000){replySend(reply,400,"application/json","{\"error\":\"Valid epoch required\"}");return;}timeval tv={time_t(d["epoch"].as<uint32_t>()),0};settimeofday(&tv,nullptr);log(0,"TIME_SYNC","Clock synchronized by operator");replySend(reply,200,"application/json","{\"status\":\"APPLIED\"}");}

static void serviceRpc(){
  RpcRequest req;if(xQueueReceive(rpcRequests,&req,0)!=pdTRUE)return;RpcReply reply;reply.id=req.id;
  if(uint32_t(millis()-req.queued)>400)replySend(reply,503,"","{\"error\":\"Control request expired; retry after connection recovers\"}");
  else if(req.kind<=RPC_TIME){JsonDocument d;if(deserializeJson(d,req.payload))replySend(reply,400,"","{\"error\":\"Invalid control payload\"}");
    else if(req.kind==RPC_COMMAND)executeCommand(d,reply);else if(req.kind==RPC_CONFIG)executeConfig(d,reply);else executeTime(d,reply);}
  else {if(req.kind==RPC_WIFI_OFF){WiFi.mode(WIFI_OFF);wifiEnabled=false;}
    else if(req.kind==RPC_WIFI_ON){WiFi.mode(WIFI_AP);WiFi.softAP(AP_SSID,apPassword,6);WiFi.setSleep(false);wifiEnabled=true;}
    replySend(reply,200,"","{\"status\":\"APPLIED\"}");}
  xQueueSend(rpcReplies,&reply,0);
}
static RpcReply controlCall(RpcKind kind,const char*payload=""){
  static uint32_t sequence=0;RpcRequest req;req.id=++sequence;req.queued=millis();req.kind=kind;strlcpy(req.payload,payload,sizeof req.payload);
  RpcReply reply;reply.id=req.id;
  if(!rpcRequests||xQueueSend(rpcRequests,&req,0)!=pdTRUE){strlcpy(reply.body,"{\"error\":\"Control queue busy\"}",sizeof reply.body);return reply;}
  uint32_t start=millis();while(uint32_t(millis()-start)<600){if(xQueueReceive(rpcReplies,&reply,pdMS_TO_TICKS(20))==pdTRUE&&reply.id==req.id)return reply;}
  reply.status=503;strlcpy(reply.body,"{\"error\":\"Control result unconfirmed; inspect command status before retrying\"}",sizeof reply.body);return reply;
}
static void runRpc(RpcKind kind,const char*payload){RpcReply reply=controlCall(kind,payload);server.send(reply.status,"application/json",reply.body);}
static void publishSnapshots(){
  if(millis()-lastSnapshot<100)return;lastSnapshot=millis();JsonDocument d;dataJson(d);String data;serializeJson(d,data);
  bool changed=eventId!=cachedEventId;String ev;
  if(changed){JsonDocument e;auto arr=e["events"].to<JsonArray>();uint32_t start=eventId>128?eventId-128:0;
    for(uint32_t i=start;i<eventId;i++){auto&v=events[i%128];auto j=arr.add<JsonObject>();j["id"]=v.id;j["epoch"]=v.epoch;j["uptime"]=v.seconds;j["boot"]=boot;j["node"]=v.node;j["type"]=v.type;j["detail"]=v.detail;}
    serializeJson(e,ev);cachedEventId=eventId;}
  xSemaphoreTake(snapshotLock,portMAX_DELAY);dataSnapshot=data;if(changed)eventSnapshot=ev;memcpy(tftSnapshot.nodes,nodes,sizeof nodes);tftSnapshot.page=page;tftSnapshot.selected=selected;tftSnapshot.knob=knob;xSemaphoreGive(snapshotLock);
}
static void sendSnapshot(bool events=false){
  xSemaphoreTake(snapshotLock,portMAX_DELAY);String data=events?eventSnapshot:dataSnapshot;xSemaphoreGive(snapshotLock);
  server.sendHeader("Cache-Control","no-store");server.send(data.length()?200:503,"application/json",data.length()?data:"{\"error\":\"Master starting\"}");
}
static void historyResponse(){
  int seconds=server.hasArg("seconds")?server.arg("seconds").toInt():300;seconds=constrain(seconds,60,3600);
  size_t limit=psramFound()?720:180,bytes=limit*128+32;
  uint32_t caps=psramFound()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):MALLOC_CAP_8BIT;
  auto*indices=(uint64_t*)heap_caps_malloc(limit*sizeof(uint64_t),caps);char*out=(char*)heap_caps_malloc(bytes,caps);
  if(!indices||!out){free(indices);free(out);server.send(503,"application/json","{\"error\":\"History memory unavailable\"}");return;}
  uint64_t end=history.count(),begin=end>history.capacity?end-history.capacity:0;
  uint32_t stride=max(uint32_t(1),(uint32_t(seconds)+limit-1)/limit),lastBucket=UINT32_MAX;size_t count=0;
  Historian::Record latest;if(end&&history.get(end-1,latest)){
    for(uint64_t k=end;k>begin&&count<limit;){--k;Historian::Record r;if(!history.get(k,r))continue;uint32_t age;
      if(r.boot==latest.boot){if(r.seconds>latest.seconds)continue;age=latest.seconds-r.seconds;if(age>uint32_t(seconds))break;}
      else if(r.epoch&&latest.epoch&&latest.epoch>=r.epoch)age=latest.epoch-r.epoch;else continue;
      if(age>uint32_t(seconds))continue;uint32_t bucket=age/stride;if(bucket!=lastBucket){indices[count++]=k;lastBucket=bucket;}
      if((k&255)==0)vTaskDelay(1);
    }
  }
  size_t used=snprintf(out,bytes,"{\"records\":[");bool first=true;
  while(count){Historian::Record r;if(!history.get(indices[--count],r))continue;
    int n=snprintf(out+used,bytes-used,"%s[%lu,%lu,%lu,%d,%d,%u,%u,%d,%d,%u,%u]",first?"":",",(unsigned long)r.epoch,(unsigned long)r.boot,(unsigned long)r.seconds,r.p[0],r.t[0],r.flags[0],r.connected&1,r.p[1],r.t[1],r.flags[1],(r.connected>>1)&1);
    if(n<0||size_t(n)>=bytes-used-3){free(indices);free(out);server.send(503,"application/json","{\"error\":\"History response too large\"}");return;}
    used+=n;first=false;
  }
  used+=snprintf(out+used,bytes-used,"]}");server.sendHeader("Cache-Control","no-store");server.setContentLength(used);server.send(200,"application/json","");server.sendContent(out,used);free(indices);free(out);
}
static void downloadHistory(){
  if(!authorized())return;if(!history.acquireSnapshot()){server.send(503,"text/plain","History unavailable or recovering");return;}
  uint32_t size=history.frozenSize;server.sendHeader("Content-Disposition","attachment; filename=history_boiler.csv");server.setContentLength(size);server.send(200,"text/csv; charset=utf-8","");
  uint8_t chunk[512];for(uint32_t pos=0;pos<size&&server.client().connected();){int n=history.readSnapshot(pos,chunk,sizeof chunk);if(n<=0)break;size_t written=server.client().write(chunk,n);if(written!=size_t(n))break;pos+=n;vTaskDelay(1);}history.releaseSnapshot();
}
static void setupHttp(){
  const char*headers[]={"Authorization","Origin","Content-Type"};server.collectHeaders(headers,3);
  server.on("/",HTTP_GET,[]{server.sendHeader("Cache-Control","no-cache");server.send_P(200,"text/html; charset=utf-8",INDEX_HTML);});
  server.on("/api/data",HTTP_GET,[]{sendSnapshot();});
  server.on("/api/history",HTTP_GET,historyResponse);server.on("/api/history.csv",HTTP_GET,downloadHistory);
  server.on("/api/export/start",HTTP_POST,[]{if(!authorized())return;if(exportActive){server.send(409,"application/json","{\"error\":\"Export already active; retry after expiry\"}");return;}
    if(!history.acquireSnapshot()){server.send(503,"application/json","{\"error\":\"History unavailable or recovering\"}");return;}exportActive=true;exportTouched=millis();exportId=esp_random()|1;JsonDocument d;d["id"]=exportId;d["bytes"]=history.frozenSize;sendJson(d);});
  server.on("/api/export/chunk",HTTP_GET,[]{if(!authorized())return;uint32_t id,offset;if(!Link::unsignedNumber(server.arg("id").c_str(),id)||!Link::unsignedNumber(server.arg("offset").c_str(),offset)||!exportActive||id!=exportId||offset>history.frozenSize){server.send(400,"application/json","{\"error\":\"Invalid snapshot/offset\"}");return;}
    exportTouched=millis();uint32_t length=min(uint32_t(32768),history.frozenSize-offset);server.setContentLength(length);server.send(200,"application/octet-stream","");uint8_t data[512];
    for(uint32_t done=0;done<length&&server.client().connected();){int n=history.readSnapshot(offset+done,data,min(uint32_t(sizeof data),length-done));if(n<=0||server.client().write(data,n)!=size_t(n))break;done+=n;vTaskDelay(1);}});
  server.on("/api/export/end",HTTP_POST,[]{JsonDocument d;if(!body(d))return;if(!d["id"].is<uint32_t>()||!exportActive||d["id"].as<uint32_t>()!=exportId){server.send(400,"application/json","{\"error\":\"Invalid snapshot\"}");return;}history.releaseSnapshot();exportActive=false;server.send(200,"application/json","{\"status\":\"CLOSED\"}");});
  server.on("/api/events",HTTP_GET,[]{sendSnapshot(true);});
  server.on("/api/events.csv",HTTP_GET,[]{if(!authorized())return;if(!history.mounted){server.send(503,"text/plain","Filesystem unavailable");return;}
    xSemaphoreTake(history.files,portMAX_DELAY);history.flushPending();File previous=LittleFS.open("/events-prev.csv","r"),current=LittleFS.open("/events.csv","r");size_t length=(previous?previous.size():0)+(current?current.size():0);
    uint8_t*copy=(uint8_t*)heap_caps_malloc(length?length:1,psramFound()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):MALLOC_CAP_8BIT);size_t copied=0;
    if(copy){if(previous)copied+=previous.read(copy,previous.size());if(current)copied+=current.read(copy+copied,current.size());}previous.close();current.close();xSemaphoreGive(history.files);
    if(!copy){server.send(503,"text/plain","Insufficient snapshot RAM");return;}const char*header="epoch,boot,uptime,node,type,detail\n";server.sendHeader("Content-Disposition","attachment; filename=events_boiler.csv");server.setContentLength(strlen(header)+copied);server.send(200,"text/csv","");server.client().print(header);
    for(size_t offset=0;offset<copied&&server.client().connected();){size_t n=server.client().write(copy+offset,min(size_t(512),copied-offset));if(!n)break;offset+=n;vTaskDelay(1);}free(copy);});
  server.on("/api/command",HTTP_POST,[]{JsonDocument d;if(!body(d))return;String payload;serializeJson(d,payload);runRpc(RPC_COMMAND,payload.c_str());});
  server.on("/api/config",HTTP_POST,[]{JsonDocument d;if(!body(d))return;String payload;serializeJson(d,payload);runRpc(RPC_CONFIG,payload.c_str());});
  server.on("/api/time",HTTP_POST,[]{JsonDocument d;if(!body(d))return;String payload;serializeJson(d,payload);runRpc(RPC_TIME,payload.c_str());});
  server.on("/api/usb-refresh",HTTP_POST,[]{if(!authorized())return;OfflineUsb::refresh();server.send(202,"application/json","{\"status\":\"QUEUED\"}");});
  server.onNotFound([]{server.send(404,"application/json","{\"error\":\"Not found\"}");});server.begin();
}
static void record(bool immediate){
  if(!history.ready)return;uint32_t now=millis();bool periodic=historySchedule.periodic(now);
  if(!immediate&&!periodic&&uint32_t(now-lastRecord)<1000)return;
  lastRecord=now;Historian::Record r;r.epoch=epoch();r.boot=boot;r.seconds=uptimeSeconds();history.setClock(boot,r.epoch,r.seconds);
  for(uint8_t i=0;i<2;i++){auto&n=nodes[i];r.p[i]=n.connected?n.report.pressure:-1;r.t[i]=n.connected?n.report.temperature:-1;r.flags[i]=n.report.flags()|(n.automatic?Boiler::AUTO_DEMO:0);r.causes[i]=n.report.causes;r.muted[i]=n.report.muted;r.ack[i]=n.report.acknowledged;
    if(n.connected){r.connected|=1<<i;r.flags[i]|=Historian::STATUS_KNOWN;if(n.report.state()==1)r.flags[i]|=Historian::WAS_WARNING;}}
  r.persist=periodic||immediate;history.push(r);
}
static void serviceButtons(){
  uint32_t now=millis();uint8_t a=pb1.update(now),b=pb2.update(now),c=pb3.update(now);
  if(a&2)page=(page+1)%MasterScreen::PAGE_COUNT;
  if(a&4){if(page==MasterScreen::LIMITS)selected=(selected+1)%4;else page=0;}
  if(b&2)request(0,"MUTE");
  if(b&4){if(page==MasterScreen::LIMITS){uint8_t i=selected/2;if(nodes[i].connected){auto conf=nodes[i].report.config;conf.version=conf.version==65535?1:conf.version+1;if(selected%2)conf.temperature=lroundf(50+knob*250)*10;else conf.pressure=lroundf((1+knob*7)*10)*10;configRequest(i,conf);}}else request(0,"ACK");}
  if(c&1){wasGlobalAtPress=globalActive();if(!wasGlobalAtPress)request(0,"GLOBAL,1");}
  if((c&4)&&wasGlobalAtPress)request(0,"GLOBAL,0");
  if(now-knobAt>=50){knobAt=now;uint32_t sum=0;for(int k=0;k<16;k++)sum+=analogRead(4);knob=knob*0.8f+(sum/16.0f/4095)*0.2f;}
}
static void serviceControl(){feedLoopWDT();serviceLinks();serviceButtons();record();}
static void text(int x,int y,const String&s,uint16_t color=INK,uint8_t size=1){panel.setTextSize(size);panel.setTextColor(color);panel.setCursor(x,y);panel.print(s);}
static String stamp(uint32_t e,uint32_t seconds){if(!e)return String("U+")+seconds+"s";time_t t=e;tm local;localtime_r(&t,&local);char s[12];strftime(s,sizeof s,"%H:%M:%S",&local);return s;}
static String peak(int16_t value,float scale,uint8_t precision){return value<0?String("--"):String(value/scale,unsigned(precision));}
static void display(){
  if(millis()-lastDisplay<250)return;lastDisplay=millis();if(!panel.ready())return;
  // Render only a copied view; the I/O task never reads live control nodes.
  TftSnapshot view;xSemaphoreTake(snapshotLock,portMAX_DELAY);view=tftSnapshot;xSemaphoreGive(snapshotLock);
  Node*nodes=view.nodes;uint8_t page=view.page,selected=view.selected;float knob=view.knob;bool purge=false;
  for(auto&n:view.nodes)if(n.globalWanted||(n.connected&&n.report.global))purge=true;
  panel.fillScreen(BG);panel.setTextWrap(false);
  MasterScreen::header(panel,page);
  if(page==MasterScreen::MONITOR){for(uint8_t i=0;i<2;i++)MasterScreen::card(panel,i,nodes[i].connected,nodes[i].report);}
  else if(page==MasterScreen::HISTORY){text(12,24,String("P maks ")+peak(history.peakP[0],100,2)+" / "+peak(history.peakP[1],100,2)+" bar",ACCENT);
    text(12,37,String("T maks ")+peak(history.peakT[0],10,1)+" / "+peak(history.peakT[1],10,1)+" C",INK);
    for(uint8_t i=0;i<5;i++){auto&a=history.alarms[i];text(12,52+i*13,a.node?stamp(a.epoch,a.seconds)+" B"+a.node+" "+compactCause(a.causes):"--",MUTED);}
    text(12,120,history.writeError?"Penyimpanan error":history.paused?"Ekspor USB aktif":history.ready?"Tersimpan: "+String(history.totalBytes/1024)+" KB":"Memuat riwayat...",history.writeError?RED:INK);
    text(12,136,String("Sisa RAM: ")+ESP.getFreeHeap()/1024+"K  PSRAM: "+ESP.getFreePsram()/1024+"K",MUTED);}
  else if(page==MasterScreen::NETWORK){text(6,27,AP_SSID,ACCENT);text(6,48,"Password: "+String(apPassword));text(6,69,"IP: "+WiFi.softAPIP().toString());text(6,97,"Token kontrol:",MUTED);text(6,115,operatorToken,AMBER);}
  else if(page==MasterScreen::GUIDE){constexpr int scale=3,quiet=4;int size=(GUIDE_QR_SIZE+quiet*2)*scale;panel.fillRect(8,23,size,size,WHITE);
    for(int y=0;y<GUIDE_QR_SIZE;y++)for(int x=0;x<GUIDE_QR_SIZE;x++)if(pgm_read_byte(GUIDE_QR+y*((GUIDE_QR_SIZE+7)/8)+x/8)&(0x80>>(x%8)))panel.fillRect(8+(x+quiet)*scale,23+(y+quiet)*scale,scale,scale,0);
    text(141,66,"scan for guidebook",INK);text(141,91,"SIMPAS-BOILER",MUTED);}
  else if(page==MasterScreen::LIMITS){uint8_t i=selected/2;bool temp=selected%2;auto c=nodes[i].report.config;float proposed=temp?roundf(50+knob*250):roundf((1+knob*7)*10)/10;
    text(6,26,String("B")+(i+1)+(temp?" Suhu (C)":" Tekanan (bar)"),ACCENT);text(6,49,String(temp?c.temperature/10.0:c.pressure/100.0,1)+" -> "+String(proposed,1),AMBER,2);
    text(6,82,"Tahan PB1: pilih");text(6,101,"Tahan PB2: simpan");text(6,124,nodes[i].commandStatus,MUTED);}
  String footer=purge?"PURGE GLOBAL AKTIF":(!nodes[0].connected||!nodes[1].connected)?"Koneksi slave terputus":(nodes[0].report.causes||nodes[1].report.causes)?"ALARM AKTIF":(nodes[0].state==4||nodes[1].state==4)?"Menunggu data...":"PB1:Hal PB2:Mute PB3:Purge";
  MasterScreen::footer(panel,footer.c_str(),purge);panel.present();
}
static WiFiClient mqttSocket;static PubSubClient mqtt(mqttSocket);static SemaphoreHandle_t mqttLock;static String mqttSnapshot;
struct MqttCommand{uint8_t node;char action[16];};static QueueHandle_t mqttCommands;
static void mqttCallback(char*,uint8_t*payload,unsigned length){JsonDocument d;if(length>512||deserializeJson(d,payload,length))return;
  if(!d["token"].is<const char*>()||strcmp(d["token"],operatorToken)||!d["node"].is<int>()||d["node"].as<int>()<0||d["node"].as<int>()>2||!d["action"].is<const char*>())return;
  MqttCommand c;c.node=d["node"];strlcpy(c.action,d["action"],sizeof c.action);xQueueSend(mqttCommands,&c,0);}
static void mqttWorker(void*){uint32_t tried=0,published=0;char client[40];snprintf(client,sizeof client,"SIMPAS-%08lX",(unsigned long)boot);
  for(;;){if(WiFi.status()==WL_CONNECTED){if(!mqtt.connected()&&millis()-tried>5000){tried=millis();if(mqtt.connect(client,MQTT_USER,MQTT_PASSWORD,"boiler/availability",1,true,"offline")){mqtt.publish("boiler/availability","online",true);mqtt.subscribe("boiler/command");}}
    if(mqtt.connected()){mqtt.loop();if(millis()-published>=1000){published=millis();xSemaphoreTake(mqttLock,portMAX_DELAY);String copy=mqttSnapshot;xSemaphoreGive(mqttLock);if(copy.length())mqtt.publish("boiler/telemetry",copy.c_str());}}}vTaskDelay(pdMS_TO_TICKS(25));}}
static char serialInput[64];static uint8_t serialLength=0;static bool dumping=false;static uint32_t dumpPosition=0,dumpSize=0;
static void serialCommand(const char*line){
  if(!strcmp(line,"TOKEN")){Serial.printf("Operator token: %s\nAP password: %s\n",operatorToken,apPassword);}
  else if(!strcmp(line,"STATUS")){Serial.printf("Reset=%d flash=%u PSRAM=%u freePSRAM=%u heap=%u TFT=%s FS=%s history=%s USB=%s\n",int(esp_reset_reason()),ESP.getFlashChipSize(),ESP.getPsramSize(),ESP.getFreePsram(),ESP.getFreeHeap(),panel.pixels?"INDEXED":"UNAVAILABLE",history.mounted?"mounted":"unmounted",history.ready?"ready":"recovering",OfflineUsb::statusText());}
  else if(!strcmp(line,"TFT")){initTft();panel.invalidate();lastDisplay=millis()-1000;display();Serial.println("TFT reinitialized");}
  else if(!strcmp(line,"USB")){OfflineUsb::refresh();Serial.println("USB snapshot refresh queued; safely eject/reconnect if host caches old contents.");}
  else if(!strcmp(line,"DUMP")){if(!dumping&&history.acquireSnapshot()){dumping=true;dumpPosition=0;dumpSize=history.frozenSize;Serial.printf("# DUMP BEGIN %lu bytes\n",(unsigned long)dumpSize);}else Serial.println("History unavailable or dump already active");}
  else if(!strcmp(line,"STOP")){if(dumping){dumping=false;history.releaseSnapshot();Serial.println("# DUMP STOPPED");}}
  else if(!strcmp(line,"FLUSH")){if(history.ready){xSemaphoreTake(history.files,portMAX_DELAY);bool ok=history.flushPending();xSemaphoreGive(history.files);Serial.println(ok?"FLUSH OK":"FLUSH FAILED");}}
  else if(!strcmp(line,"FORMAT CONFIRM")){bool ok=history.formatExplicit();if(ok)OfflineUsb::refresh();Serial.println(ok?"Filesystem initialized; recovery starting":"Format refused (already mounted) or failed");}
  else if(!strcmp(line,"WIFI OFF")){RpcReply r=controlCall(RPC_WIFI_OFF);Serial.println(r.body);}
  else if(!strcmp(line,"WIFI ON")){RpcReply r=controlCall(RPC_WIFI_ON);Serial.println(r.body);}
  else Serial.println("HELP: STATUS | TFT | TOKEN | DUMP | STOP | USB | FLUSH | WIFI OFF | WIFI ON | FORMAT CONFIRM (only unmounted FS, ERASES data). 115200 Newline.");
}
static void serialService(){
  uint8_t budget=64;while(budget--&&Serial.available()){char c=Serial.read();if(c=='\n'){serialInput[serialLength]=0;serialCommand(serialInput);serialLength=0;}else if(c!='\r'){if(serialLength<sizeof serialInput-1)serialInput[serialLength++]=c;else serialLength=0;}}
  if(dumping&&Serial.availableForWrite()>=128){uint8_t data[128];int n=history.readSnapshot(dumpPosition,data,min(uint32_t(sizeof data),dumpSize-dumpPosition));
    if(n>0){size_t sent=Serial.write(data,n);dumpPosition+=sent;}if(dumpPosition>=dumpSize||n<=0){dumping=false;history.releaseSnapshot();Serial.println("\n# DUMP END");}}
}
static void ioWorker(void*){
  for(;;){server.handleClient();OfflineUsb::service();serialService();display();
    if(exportActive&&uint32_t(millis()-exportTouched)>120000){history.releaseSnapshot();exportActive=false;}
    vTaskDelay(1);
  }
}
static void setup(){
  boot=esp_random()|1;Serial.begin(115200);
  initTft();bootScreen("1/5 Layar");
  Serial.printf("[BOOT] reset=%d; flash=%u; PSRAM=%u\n",int(esp_reset_reason()),ESP.getFlashChipSize(),ESP.getPsramSize());
  setenv("TZ","WIB-7",1);tzset();prefs.begin("simpas-v3",false);
  if(prefs.getString("token","")!=operatorToken)prefs.putString("token",operatorToken);
  if(prefs.getString("ap-pass","")!=apPassword)prefs.putString("ap-pass",apPassword);
  Serial1.setRxBufferSize(2048);Serial2.setRxBufferSize(2048);Serial1.begin(9600,SERIAL_8N1,16,17);Serial2.begin(9600,SERIAL_8N1,8,18);for(int p=5;p<=7;p++)pinMode(p,INPUT_PULLUP);analogReadResolution(12);analogSetPinAttenuation(4,ADC_11db);
  for(uint8_t i=0;i<2;i++){nodes[i].session=esp_random()|1;nodes[i].nextPoll=millis()+i*300;nodes[i].globalWanted=prefs.getBool(i?"global2":"global1",false);
    if(prefs.getBytesLength(i?"config2":"config1")==sizeof(Boiler::Config)){prefs.getBytes(i?"config2":"config1",&nodes[i].saved,sizeof(Boiler::Config));nodes[i].hasSaved=Boiler::validConfig(nodes[i].saved);}}
  bootScreen("2/5 Riwayat");panel.begin();history.begin();
  bootScreen("3/5 WiFi");
  WiFi.persistent(false);WiFi.setAutoReconnect(false);WiFi.mode(strlen(STA_SSID)?WIFI_AP_STA:WIFI_AP);WiFi.softAP(AP_SSID,apPassword,6);WiFi.setSleep(false);if(strlen(STA_SSID)){WiFi.begin(STA_SSID,STA_PASSWORD);configTime(0,0,"pool.ntp.org");}
  snapshotLock=xSemaphoreCreateMutex();rpcRequests=xQueueCreate(2,sizeof(RpcRequest));rpcReplies=xQueueCreate(2,sizeof(RpcReply));
  if(!snapshotLock||!rpcRequests||!rpcReplies){bootScreen("Memori layanan tidak cukup");for(;;){serviceControl();delay(1);}}
  bootScreen("4/5 Layanan");setupHttp();OfflineUsb::begin(history);log(0,"MASTER_BOOT","N16R8 / schema 3 / local history");
  enableLoopWDT();
  if(strlen(MQTT_HOST)){mqttLock=xSemaphoreCreateMutex();mqttCommands=xQueueCreate(4,sizeof(MqttCommand));mqtt.setServer(MQTT_HOST,MQTT_PORT);mqtt.setBufferSize(8192);mqtt.setSocketTimeout(2);mqttSocket.setConnectionTimeout(2000);mqtt.setCallback(mqttCallback);xTaskCreatePinnedToCore(mqttWorker,"mqtt",8192,nullptr,1,nullptr,0);}
  publishSnapshots();bootScreen("5/5 Siap");lastDisplay=millis()-1000;display();
  ioStarted=xTaskCreatePinnedToCore(ioWorker,"web-usb-tft",16384,nullptr,1,nullptr,0)==pdPASS;
  if(!ioStarted)Serial.println("ERROR: I/O task allocation failed; UART/local control continue");
  Serial.printf("SIMPAS v3 N16R8. AP: %s / %s. Type TOKEN or HELP.\n",AP_SSID,apPassword);
}
static void loop(){
  serviceControl();serviceRpc();serviceControl();publishSnapshots();
  if(strlen(MQTT_HOST)){MqttCommand c;while(xQueueReceive(mqttCommands,&c,0)==pdTRUE)request(c.node,String(c.action));static uint32_t last=0;if(millis()-last>=1000){last=millis();JsonDocument d;dataJson(d);String out;serializeJson(d,out);xSemaphoreTake(mqttLock,portMAX_DELAY);mqttSnapshot=out;xSemaphoreGive(mqttLock);}}
  static uint32_t staStarted=millis();static bool staStopped=false;if(strlen(STA_SSID)&&wifiEnabled&&!staStopped&&WiFi.status()!=WL_CONNECTED&&millis()-staStarted>10000){WiFi.disconnect(false,false);staStopped=true;log(0,"STA_UNAVAILABLE","Router attempt stopped; AP remains running");}
  delay(1);
}
}
