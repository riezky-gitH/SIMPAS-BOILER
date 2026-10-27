#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <unistd.h>
#include "Protocol.h"
#include "HistoryExport.h"

namespace Historian {
struct Segment {char path[24]={};uint32_t bytes=0;uint16_t headerBytes=0;ExportMap projection;};
constexpr uint32_t SEGMENT_SIZE=128*1024,FLASH_BUDGET=9*1024*1024;
constexpr uint16_t MAX_SEGMENTS=80;
class Store {
 public:
  Record*ring=nullptr;size_t capacity=0;uint64_t head=0,flushed=0;
  portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
  SemaphoreHandle_t files=nullptr;
  bool mounted=false,ready=false,psram=false,writeError=false,paused=false;
  uint32_t dropped=0,corrupt=0,flushes=0,totalBytes=0,rotated=0,lastFlush=0,approxPending=0;
  uint32_t clockBoot=0,clockEpoch=0,clockSeconds=0;
  void setClock(uint32_t b,uint32_t e,uint32_t seconds){if(!e)return;portENTER_CRITICAL(&mux);clockBoot=b;clockEpoch=e;clockSeconds=seconds;portEXIT_CRITICAL(&mux);}
  uint16_t lastRowBytes=100;
  int16_t peakP[2]={-1,-1},peakT[2]={-1,-1};Alarm alarms[5];
  bool alarmDirty=false;Segment active;Segment segments[MAX_SEGMENTS];uint16_t segmentCount=0;uint32_t nextId=1;
  ExportCache exportCache;
  ProjectScratch projectScratch; // Guarded by files; keep buffers off TinyUSB's 4 KiB stack.
  char eventQueue[64][256];uint32_t eventHead=0,eventFlushed=0,eventDrops=0,eventBytes=0;
  Segment frozen[MAX_SEGMENTS];uint16_t frozenCount=0;uint32_t frozenSize=0;uint16_t snapshotUsers=0;
  void begin(){
    files=xSemaphoreCreateMutex();psram=psramFound();capacity=psram?3*1024*1024/sizeof(Record):512;
    ring=(Record*)heap_caps_calloc(capacity,sizeof(Record),psram?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):MALLOC_CAP_8BIT);
    if(!ring){capacity=256;psram=false;ring=(Record*)calloc(capacity,sizeof(Record));}
    if(!ring){capacity=0;writeError=true;return;}
    // Never auto-format a failed filesystem: existing evidence must be preserved.
    mounted=LittleFS.begin(false,"/littlefs",12,"spiffs");
    if(!mounted){writeError=true;ready=true;return;}
    scan();
    recoverTail("/history_boiler.csv");recoverTail("/events.csv");
    xTaskCreatePinnedToCore(worker,"historian",8192,this,1,nullptr,0);
  }
  void scan(){
    segmentCount=0;totalBytes=0;File dir=LittleFS.open("/");File f=dir.openNextFile();
    while(f){String name=f.name();if(name.startsWith("/"))name.remove(0,1);if(name.startsWith("h")&&name.endsWith(".csv")&&name.length()==13&&segmentCount<MAX_SEGMENTS){
      Segment&s=segments[segmentCount++];s=Segment();snprintf(s.path,sizeof s.path,"/%s",name.c_str());s.bytes=f.size();s.headerBytes=f.readStringUntil('\n').length()+1;totalBytes+=s.bytes;
      uint32_t id=strtoul(name.substring(1,9).c_str(),nullptr,16);if(id>=nextId)nextId=id+1;
    }f.close();f=dir.openNextFile();}dir.close();
    for(uint16_t i=0;i<segmentCount;i++)for(uint16_t j=i+1;j<segmentCount;j++)if(strcmp(segments[i].path,segments[j].path)>0){Segment t=segments[i];segments[i]=segments[j];segments[j]=t;}
    active=Segment();strlcpy(active.path,"/history_boiler.csv",sizeof active.path);
    File activeFile=LittleFS.open(active.path,"r");if(activeFile){active.bytes=activeFile.size();active.headerBytes=activeFile.readStringUntil('\n').length()+1;totalBytes+=active.bytes;activeFile.close();}
    File a=LittleFS.open("/alarms.dat","r");AlarmStore disk;if(a&&a.size()==sizeof disk&&a.read((uint8_t*)&disk,sizeof disk)==sizeof disk&&disk.magic==0x414C4D33&&disk.crc==Link::crc16(&disk,offsetof(AlarmStore,crc)))memcpy(alarms,disk.rows,sizeof alarms);a.close();
  }
  void recoverTail(const char*path){
    File f=LittleFS.open(path,"r");if(!f)return;uint32_t size=f.size();
    if(size){uint32_t start=size>512?size-512:0;f.seek(start);char buf[513];size_t n=f.read((uint8_t*)buf,size-start);int last=int(n)-1;while(last>=0&&buf[last]!='\n')last--;f.close();
      uint32_t keep=start+last+1;if(keep<size){String full=String("/littlefs")+path;if(::truncate(full.c_str(),keep)!=0)writeError=true;else if(!strcmp(path,"/history_boiler.csv"))totalBytes-=size-keep;}}
    else f.close();
  }
  void push(Record r,bool recovery=false){
    if(!ring)return;if(r.persist){char row[192];r.encodedBytes=csv(r,row,sizeof row);}portENTER_CRITICAL(&mux);
    if(head-flushed>=capacity){Record&old=ring[head%capacity];if(old.persist){dropped++;approxPending=approxPending>=old.encodedBytes?approxPending-old.encodedBytes:0;}flushed=head-capacity+1;}
    ring[head%capacity]=r;head++;if(recovery)flushed=head;
    for(uint8_t i=0;i<2;i++)if(r.connected&(1<<i)){if(!(r.flags[i]&4)&&r.p[i]>peakP[i])peakP[i]=r.p[i];if(!(r.flags[i]&8)&&r.t[i]>peakT[i])peakT[i]=r.t[i];}
    if(r.persist){approxPending+=r.encodedBytes;lastRowBytes=r.encodedBytes;}portEXIT_CRITICAL(&mux);
  }
  void addAlarm(const Alarm&a){portENTER_CRITICAL(&mux);for(int i=4;i>0;i--)alarms[i]=alarms[i-1];alarms[0]=a;alarmDirty=true;portEXIT_CRITICAL(&mux);}
  void addEvent(const char*row){portENTER_CRITICAL(&mux);size_t n=strlen(row);
    if(n>=256){eventDrops++;portEXIT_CRITICAL(&mux);return;}
    if(eventHead-eventFlushed>=64){eventBytes-=strlen(eventQueue[eventFlushed%64]);eventFlushed++;eventDrops++;}
    strcpy(eventQueue[eventHead++%64],row);eventBytes+=n;portEXIT_CRITICAL(&mux);}
  bool flushEvents(){
    char batch[4096];size_t bytes=0;uint32_t end;portENTER_CRITICAL(&mux);end=eventFlushed;
    while(end<eventHead){size_t n=strlen(eventQueue[end%64]);if(bytes+n>sizeof batch)break;memcpy(batch+bytes,eventQueue[end%64],n);bytes+=n;end++;}portEXIT_CRITICAL(&mux);
    if(!bytes)return true;File old=LittleFS.open("/events.csv","r");uint32_t size=old?old.size():0;old.close();
    if(size+bytes>128*1024){LittleFS.remove("/events-prev.csv");if(!LittleFS.rename("/events.csv","/events-prev.csv"))return false;}
    File f=LittleFS.open("/events.csv","a");if(!f)return false;uint32_t before=f.size();bool ok=f.write((uint8_t*)batch,bytes)==bytes;f.flush();f.close();
    if(!ok){::truncate("/littlefs/events.csv",before);return false;}
    portENTER_CRITICAL(&mux);if(end>eventFlushed){eventFlushed=end;eventBytes=eventBytes>=bytes?eventBytes-bytes:0;}portEXIT_CRITICAL(&mux);return true;
  }
  bool get(uint64_t index,Record&out){bool ok;portENTER_CRITICAL(&mux);ok=index<head&&head-index<=capacity;if(ok){out=ring[index%capacity];out.epoch=HistoryTiming::inferredEpoch(out.epoch,out.boot,out.seconds,clockBoot,clockEpoch,clockSeconds);}portEXIT_CRITICAL(&mux);return ok;}
  uint64_t count(){portENTER_CRITICAL(&mux);uint64_t n=head;portEXIT_CRITICAL(&mux);return n;}
  bool pinned(const char*path){if(!snapshotUsers)return false;for(uint16_t i=0;i<frozenCount;i++)if(!strcmp(path,frozen[i].path))return true;return false;}
  bool space(){
    while(totalBytes>FLASH_BUDGET-8192||segmentCount>=MAX_SEGMENTS-2||LittleFS.totalBytes()-LittleFS.usedBytes()<32768){
      if(!segmentCount||pinned(segments[0].path)){paused=true;return false;}
      if(!LittleFS.remove(segments[0].path)){writeError=true;return false;}totalBytes-=segments[0].bytes;
      for(uint16_t i=1;i<segmentCount;i++)segments[i-1]=segments[i];segmentCount--;rotated++;
    }paused=false;return true;
  }
  bool rotate(){
    File f=LittleFS.open(active.path,"r");if(!f)return true;uint32_t size=f.size();f.close();if(size<=active.headerBytes)return true;
    if(segmentCount>=MAX_SEGMENTS)return false;
    Segment&s=segments[segmentCount];s=active;snprintf(s.path,sizeof s.path,"/h%08lX.csv",(unsigned long)nextId++);s.bytes=size;
    if(!LittleFS.rename(active.path,s.path))return false;segmentCount++;
    active=Segment();strlcpy(active.path,"/history_boiler.csv",sizeof active.path);return true;
  }
  void projectBatch(const char*batch,size_t bytes,uint32_t start){
    char line[192];size_t length=0;uint32_t rowStart=start;
    for(size_t i=0;i<bytes;i++){
      if(batch[i]=='\n'){line[length]=0;Record r;if(parseCsv(line,r)){if(!active.projection.add(r,rowStart))corrupt++;}else corrupt++;
        length=0;rowStart=start+i+1;}
      else if(length<sizeof line-1)line[length++]=batch[i];
    }
  }
  // Caller owns files mutex. Only advance flushed after durable append succeeds.
  bool flushPending(){
    if(!mounted||!ready)return false;char batch[4096];size_t used=0;uint32_t consumed=0;uint64_t end=flushed,limit=count();Record r;
    while(end<limit){if(!get(end,r)){end++;continue;}if(r.persist){char row[192];size_t n=csv(r,row,sizeof row);if(!n){corrupt++;consumed+=r.encodedBytes;end++;continue;}if(used+n>sizeof batch)break;memcpy(batch+used,row,n);used+=n;consumed+=r.encodedBytes;}end++;}
    if(used){
      if(!space())return false;File check=LittleFS.open("/history_boiler.csv","r");uint32_t size=check?check.size():0;check.close();
      if(size+used>SEGMENT_SIZE&&!rotate()){writeError=true;return false;}
      File f=LittleFS.open("/history_boiler.csv","a");if(!f){writeError=true;return false;}
      uint32_t before=f.size();if(!before){if(f.print(CSV_HEADER)!=strlen(CSV_HEADER)){f.close();writeError=true;return false;}totalBytes+=strlen(CSV_HEADER);before=f.size();active.headerBytes=before;active.bytes=before;}
      size_t written=f.write((const uint8_t*)batch,used);f.flush();f.close();
      if(written!=used){::truncate("/littlefs/history_boiler.csv",before);writeError=true;return false;}projectBatch(batch,used,before);active.bytes=before+used;totalBytes+=used;flushes++;
    }
    portENTER_CRITICAL(&mux);if(end>flushed)flushed=end;approxPending=approxPending>=consumed?approxPending-consumed:0;portEXIT_CRITICAL(&mux);
    if(alarmDirty){AlarmStore disk={};portENTER_CRITICAL(&mux);memcpy(disk.rows,alarms,sizeof alarms);alarmDirty=false;portEXIT_CRITICAL(&mux);disk.crc=Link::crc16(&disk,offsetof(AlarmStore,crc));
      File f=LittleFS.open("/alarms.tmp","w");bool ok=f&&f.write((uint8_t*)&disk,sizeof disk)==sizeof disk;if(f){f.flush();f.close();}
      if(!ok||!LittleFS.rename("/alarms.tmp","/alarms.dat")){alarmDirty=true;writeError=true;return false;}}
    if(!flushEvents()){writeError=true;return false;}writeError=false;lastFlush=millis();return true;
  }
  bool acquireSnapshot(){
    if(!ready||!mounted)return false;xSemaphoreTake(files,portMAX_DELAY);
    if(snapshotUsers){snapshotUsers++;xSemaphoreGive(files);return true;}
    // Bound the preparation; anything still pending remains visible in RAM stats.
    for(uint8_t i=0;i<4&&flushed<count();i++)if(!flushPending())break;
    if(!rotate()){xSemaphoreGive(files);return false;}
    frozenCount=segmentCount;memcpy(frozen,segments,sizeof(Segment)*segmentCount);frozenSize=strlen(EXPORT_HEADER);
    for(uint16_t i=0;i<frozenCount;i++)frozenSize+=frozen[i].projection.bytes;
    if(frozenSize>63*1024*1024){xSemaphoreGive(files);return false;}exportCache.clear();
    snapshotUsers=1;xSemaphoreGive(files);return true;
  }
  bool formatExplicit(){
    if(mounted||!files||!ring)return false;
    if(!LittleFS.format())return false;
    mounted=LittleFS.begin(false,"/littlefs",12,"spiffs");if(!mounted)return false;
    ready=false;scan();xTaskCreatePinnedToCore(worker,"historian",8192,this,1,nullptr,0);return true;
  }
  void releaseSnapshot(){xSemaphoreTake(files,portMAX_DELAY);if(snapshotUsers)snapshotUsers--;xSemaphoreGive(files);}
  // Caller owns files; indexes are maintained during recovery/flush, so acquiring a
  // snapshot never rescans all history or creates a second multi-megabyte file.
  int readProjected(uint32_t position,uint8_t*out,size_t bytes){
    size_t done=0,h=strlen(EXPORT_HEADER);bytes=min(bytes,size_t(frozenSize-position));
    if(position<h){size_t n=min(bytes,size_t(h-position));memcpy(out,EXPORT_HEADER+position,n);done+=n;position+=n;}
    uint32_t base=h;
    for(uint16_t i=0;i<frozenCount&&done<bytes;i++){
      uint32_t length=frozen[i].projection.bytes;
      if(position<base+length){File f=LittleFS.open(frozen[i].path,"r");if(!f)break;uint32_t offset=position-base;
        size_t wanted=min(bytes-done,size_t(length-offset));int n=projectSlice(f,frozen[i].projection,frozen[i].bytes,offset,out+done,wanted,projectScratch);f.close();
        if(n<=0)break;done+=n;position+=n;if(size_t(n)<wanted)break;}base+=length;
    }return done;
  }
  int readSnapshot(uint32_t position,uint8_t*out,size_t bytes){
    xSemaphoreTake(files,portMAX_DELAY);if(!snapshotUsers||position>=frozenSize){xSemaphoreGive(files);return 0;}
    int done=exportCache.read(position,out,bytes,frozenSize,[this](uint32_t p,uint8_t*data,size_t n){return readProjected(p,data,n);});
    xSemaphoreGive(files);return done;
  }
  bool upgrade(const char*path){
    File src=LittleFS.open(path,"r");if(!src)return true;
    String header=src.readStringUntil('\n');header.trim();
    if(header==String(CSV_HEADER).substring(0,strlen(CSV_HEADER)-1)){src.close();return true;}
    if(header!=String(LEGACY_HEADER).substring(0,strlen(LEGACY_HEADER)-1)){src.close();return false;}
    String temporary=String(path)+".upgrade";File out=LittleFS.open(temporary,"w");
    bool ok=out&&out.print(CSV_HEADER)==strlen(CSV_HEADER);uint32_t rows=0;
    while(ok&&src.available()){
      char line[192];size_t length=src.readBytesUntil('\n',line,sizeof line-1);line[length]=0;
      if(length&&line[length-1]=='\r')line[--length]=0;
      Record r;char converted[192];size_t n=0;
      ok=parseCsv(line,r)&&(n=csv(r,converted,sizeof converted))&&out.write((uint8_t*)converted,n)==n;
      if(++rows%32==0)vTaskDelay(1);
    }
    src.close();if(out){out.flush();out.close();}
    if(ok){
      File verify=LittleFS.open(temporary,"r");uint32_t checked=0;
      ok=verify&&verify.readStringUntil('\n')==String(CSV_HEADER).substring(0,strlen(CSV_HEADER)-1);
      while(ok&&verify.available()){
        char line[192];size_t n=verify.readBytesUntil('\n',line,sizeof line-1);line[n]=0;Record r;
        ok=parseCsv(line,r);checked++;if(checked%32==0)vTaskDelay(1);
      }
      verify.close();ok=ok&&checked==rows;
    }
    // Atomic replacement after complete CRC validation; on failure preserve source.
    if(ok)ok=LittleFS.rename(temporary,path);if(!ok)LittleFS.remove(temporary);return ok;
  }
  static void worker(void*arg){Store*s=(Store*)arg;
    for(uint16_t i=0;i<s->segmentCount;i++)if(!s->upgrade(s->segments[i].path)){s->writeError=true;vTaskDelete(nullptr);return;}
    if(!s->upgrade("/history_boiler.csv")){s->writeError=true;vTaskDelete(nullptr);return;}
    s->scan();s->recover();s->ready=true;s->lastFlush=millis();
    for(;;){if(s->approxPending>=4096||s->eventBytes>=4096||millis()-s->lastFlush>=60000){xSemaphoreTake(s->files,portMAX_DELAY);bool ok=s->flushPending();xSemaphoreGive(s->files);if(!ok)vTaskDelay(pdMS_TO_TICKS(5000));}vTaskDelay(pdMS_TO_TICKS(100));}}
  void recover(){
    for(uint16_t i=0;i<=segmentCount;i++){
      Segment&segment=i<segmentCount?segments[i]:active;File f=LittleFS.open(segment.path,"r");if(!f)continue;
      segment.projection=ExportMap();CsvLines<File> lines(f,segment.headerBytes,segment.bytes);char line[192];uint32_t start,rows=0;int result;
      while((result=lines.next(line,sizeof line,start))!=0){
        Record r;if(result>0&&parseCsv(line,r)){if(!segment.projection.add(r,start))corrupt++;push(r,true);}else corrupt++;
        if(++rows%32==0)vTaskDelay(1);
      }f.close();
    }
  }
};
}
