// Run with a native C++11 compiler; tests use the same headers as both firmwares.
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "../Firmware/Shared/Control.h"
#include "../Firmware/Shared/Protocol.h"
#include "../Firmware/Shared/Fat16.h"
static std::string contents="epoch,boot\n1,2\n";
static int readFile(uint32_t pos,uint8_t*out,size_t n){memcpy(out,contents.data()+pos,n);return n;}
int main(){
  Boiler::Safety s;s.ready=true;s.pFault=false;s.tFault=false;s.pressure=450;s.temperature=1000;s.evaluate(10);assert(s.relay&&s.buzzer&&s.trip);
  s.silence();s.evaluate(20);assert(!s.buzzer);s.temperature=1700;s.evaluate(30);assert(s.buzzer&&s.causes==3); // new cause reannunciates
  s.pressure=390;s.temperature=1000;s.evaluate(40);assert(s.causes==1); // hysteresis
  s.pressure=380;s.evaluate(100);assert(!s.causes&&s.trip&&s.relay);assert(!s.reset(1000));assert(s.reset(2100));
  s.global=true;s.local=true;s.evaluate(2200);s.local=false;s.evaluate(2300);assert(s.relay&&s.global);assert(s.releaseGlobal(2400));s.evaluate(2400);assert(!s.relay);
  s.pFault=true;s.evaluate(2500);assert(s.causes&Boiler::PRESSURE_FAULT);assert(s.relay);s.silence();s.pFault=false;s.evaluate(2600);s.pFault=true;s.evaluate(2700);assert(s.buzzer);
  s.config.live=1;s.pSim=true;s.evaluate(2800);assert(s.causes&Boiler::MODE_FAULT);
  // REAL/LIVE has the same actuator path as SIM. Neither mute nor source
  // selection may prevent a real high reading from requesting the relay.
  for(int real=0;real<2;real++){
    Boiler::Safety actual;actual.ready=true;actual.pFault=actual.tFault=false;actual.pSim=actual.tSim=!real;actual.config.live=real;
    actual.pressure=200;actual.temperature=1000;actual.evaluate(100);assert(!actual.relay&&!actual.causes);
    actual.pressure=actual.config.pressure;actual.evaluate(200);assert(actual.relay&&actual.trip&&(actual.causes&Boiler::PRESSURE_HIGH));assert(actual.flags()&Boiler::RELAY_ON);
    actual.silence();actual.evaluate(300);assert(actual.relay&&!actual.buzzer);
    actual.pressure=200;actual.evaluate(400);actual.evaluate(2500);assert(actual.reset(2500));actual.evaluate(2500);assert(!actual.relay);
    actual.remote=true;actual.evaluate(2600);assert(actual.relay&&!actual.causes);actual.remote=false;actual.evaluate(2700);assert(!actual.relay);
    actual.temperature=actual.config.temperature;actual.evaluate(2800);assert(actual.relay&&(actual.causes&Boiler::TEMP_HIGH));
  }
  int32_t number;assert(!Link::number("8x",0,1000,number));assert(!Link::number("nan",0,1000,number));uint32_t u;assert(!Link::unsignedNumber("4294967296",u));assert(Link::unsignedNumber("4294967295",u));
  assert(Link::crc16("123456789",9)==0x29B1);
  Link::Receiver rx;std::string payload="C,1,123,4,POLL";char tag[12];snprintf(tag,sizeof tag,"*%04X\n",Link::crc16(payload.data(),payload.size()));std::string frame="#"+payload+tag;char*parsed=nullptr;
  for(char c:frame){char*p=rx.feed(c,100);if(p)parsed=p;}assert(parsed&&payload==parsed);
  frame[3]^=1;parsed=nullptr;for(char c:frame){char*p=rx.feed(c,200);if(p)parsed=p;}assert(!parsed&&rx.errors==1);
  rx.feed('#',300);rx.feed('X',301);rx.feed('Y',700);assert(!rx.receiving);
  Link::PollCycle poll;poll.start(42,1000);assert(poll.matches(42)&&!poll.matches(41));assert(!poll.expired(1699));assert(poll.complete(42));assert(!poll.matches(42));
  poll.start(43,2000);assert(poll.expired(2700));assert(!poll.complete(42));assert(poll.waiting);assert(poll.complete(43));
  poll.start(44,UINT32_MAX-100);assert(!poll.expired(100));assert(poll.expired(600));
  // A slow but continuously arriving valid frame can exceed 300 ms total.
  Link::Receiver slow;payload="C,1,123,4,POLL";snprintf(tag,sizeof tag,"*%04X\n",Link::crc16(payload.data(),payload.size()));frame="#"+payload+tag;
  parsed=nullptr;uint32_t tick=1000;for(char c:frame){char*p=slow.feed(c,tick);tick+=25;if(p)parsed=p;}assert(parsed&&payload==parsed&&slow.errors==0);
  // A corrupt frame and a partial frame must not poison a later good response.
  frame[5]^=1;for(char c:frame)slow.feed(c,tick++);assert(slow.errors==1);slow.feed('#',tick++);slow.feed('X',tick++);
  frame="#"+payload+tag;parsed=nullptr;for(char c:frame){char*p=slow.feed(c,tick++);if(p)parsed=p;}assert(parsed&&payload==parsed);
  uint8_t sector[512];UsbExport::sector(0,sector,contents.size(),readFile);assert(sector[510]==0x55&&sector[511]==0xAA);assert(sector[13]==4);
  UsbExport::sector(UsbExport::ROOT_START,sector,contents.size(),readFile);assert(!memcmp(sector+96,"HISTOR~1CSV",11));assert(sector[32]==0x42&&sector[64]==1);assert(sector[109]==UsbExport::checksum("HISTOR~1CSV")||sector[45]==UsbExport::checksum("HISTOR~1CSV"));
  UsbExport::sector(UsbExport::DATA_START,sector,contents.size(),readFile);assert(!memcmp(sector,contents.data(),contents.size()));
  FILE*f=fopen("usb-test.img","wb");if(f){for(uint32_t i=0;i<UsbExport::SECTORS;i++){UsbExport::sector(i,sector,contents.size(),readFile);fwrite(sector,1,512,f);}fclose(f);}
  puts("Safety, CRC/framing and virtual FAT16 tests passed.");
}
