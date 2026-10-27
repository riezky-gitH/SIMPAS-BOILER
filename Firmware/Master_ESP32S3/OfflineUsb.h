#pragma once
#include <USB.h>
#include <USBMSC.h>
#include "Fat16.h"
#include "History.h"
#if ARDUINO_USB_MODE != 0
#error "Select USB Mode: USB-OTG (TinyUSB), CDC On Boot: Enabled, MSC On Boot: Disabled."
#endif
#if !ARDUINO_USB_CDC_ON_BOOT
#error "USB CDC On Boot must be Enabled: otherwise USB history can appear without a COM port."
#endif
#if ARDUINO_USB_MSC_ON_BOOT
#error "USB Firmware MSC On Boot must be Disabled: SIMPAS creates its own history disk."
#endif
#include <atomic>
#include "UsbSession.h"
namespace OfflineUsb {
#ifndef ENABLE_USB_MSC
#define ENABLE_USB_MSC 1
#endif
#if ENABLE_USB_MSC
static USBMSC disk;
static Historian::Store*store=nullptr;
static std::atomic<bool> mounted{false};
static SemaphoreHandle_t readLock=nullptr;
static UsbExport::Session session;
static bool configured=false;
static portMUX_TYPE signalLock=portMUX_INITIALIZER_UNLOCKED;
static uint8_t signals=0;
enum Signal:uint8_t {DISCONNECT=1,EJECT=2};
enum Status:uint8_t {STARTING,NO_HOST,FS_UNAVAILABLE,RECOVERING,EJECTED,WAITING,SNAPSHOT_RETRY,READY,INIT_ERROR};
static std::atomic<uint8_t> report{STARTING};
static const char*statusText(){const char*names[]={"STARTING","NO_HOST","FS_UNAVAILABLE","RECOVERING","EJECTED","WAITING","SNAPSHOT_RETRY","READY","INIT_ERROR"};return names[report.load()];}
static void signal(uint8_t value){portENTER_CRITICAL(&signalLock);signals|=value;portEXIT_CRITICAL(&signalLock);}
static uint8_t peekSignals(){portENTER_CRITICAL(&signalLock);uint8_t value=signals;portEXIT_CRITICAL(&signalLock);return value;}
static uint8_t takeSignals(){portENTER_CRITICAL(&signalLock);uint8_t value=signals;signals=0;portEXIT_CRITICAL(&signalLock);return value;}
static int readFile(uint32_t position,uint8_t*out,size_t bytes){return store?store->readSnapshot(position,out,bytes):0;}
static int32_t read(uint32_t lba,uint32_t offset,void*buffer,uint32_t bytes){
  if(!readLock)return -1;xSemaphoreTake(readLock,portMAX_DELAY);
  if(!mounted||uint64_t(lba)*512+offset+bytes>uint64_t(UsbExport::SECTORS)*512){xSemaphoreGive(readLock);return -1;}
  uint8_t sector[512];uint8_t*out=(uint8_t*)buffer;uint32_t done=0;
  while(done<bytes){uint32_t absolute=offset+done;if(!UsbExport::sector(lba+absolute/512,sector,store->frozenSize,readFile)){xSemaphoreGive(readLock);return -1;}
    uint32_t start=absolute%512,n=min(bytes-done,512-start);memcpy(out+done,sector+start,n);done+=n;}xSemaphoreGive(readLock);return done;
}
static int32_t write(uint32_t,uint32_t,uint8_t*,uint32_t){return -1;}
static bool startStop(uint8_t,bool start,bool eject){
  // The TinyUSB callback must not take filesystem locks or prepare a snapshot.
  if(eject&&!start){disk.mediaPresent(false);signal(EJECT);}return true;
}
static void event(void*,esp_event_base_t base,int32_t id,void*){
  if(base==ARDUINO_USB_EVENTS&&id==ARDUINO_USB_STOPPED_EVENT)signal(DISCONNECT);
}
static void release(){
  disk.mediaPresent(false);if(!readLock)return;xSemaphoreTake(readLock,portMAX_DELAY);
  if(mounted.exchange(false)&&store)store->releaseSnapshot();xSemaphoreGive(readLock);
}
static void begin(Historian::Store&s){
  store=&s;readLock=xSemaphoreCreateMutex();if(!readLock){report=INIT_ERROR;return;}
  USB.onEvent(event);disk.vendorID("SIMPAS");disk.productID("History ReadOnly");disk.productRevision("3.1");
  disk.onRead(read);disk.onWrite(write);disk.onStartStop(startStop);disk.isWritable(false);disk.mediaPresent(false);
  configured=disk.begin(UsbExport::SECTORS,512)&&USB.begin();
  if(!configured){report=INIT_ERROR;return;}
  // USB_STARTED may already have happened during Arduino CDC-on-boot startup.
  session.observe(bool(USB),millis());report=session.connected?WAITING:NO_HOST;
}
static void refresh(){if(!configured)return;release();session.refresh(millis());report=WAITING;}
static void service(){
  if(!configured)return;uint32_t now=millis();uint8_t pending=takeSignals();
  if(pending&DISCONNECT){release();session.observe(false,now);}
  bool online=bool(USB);
  if(!online&&mounted)release();session.observe(online,now);
  if(pending&EJECT){release();session.eject();}
  if(!session.connected){report=NO_HOST;return;}
  if(session.ejected){report=EJECTED;return;}
  if(mounted){report=READY;return;}
  if(!store->mounted){report=FS_UNAVAILABLE;return;}
  if(!store->ready){report=RECOVERING;return;}
  if(session.due(now,store->ready,store->mounted)){
    xSemaphoreTake(readLock,portMAX_DELAY);bool acquired=store->acquireSnapshot();
    if(acquired&&bool(USB)&&!(peekSignals()&(DISCONNECT|EJECT))){mounted=true;session.inserted();disk.mediaPresent(true);report=READY;}
    else {if(acquired)store->releaseSnapshot();session.failed(millis());report=SNAPSHOT_RETRY;}
    xSemaphoreGive(readLock);
  }
}
#else
static bool mounted=false;
static const char*statusText(){return "DISABLED";}
static void begin(Historian::Store&){USB.begin();}
static void refresh(){}
static void service(){}
#endif
}
