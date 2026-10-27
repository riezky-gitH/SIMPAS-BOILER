#pragma once
#include "HistoryRecord.h"

namespace Historian {
constexpr char EXPORT_HEADER[]="Waktu WIB,Tekanan Slave 1 (bar),Suhu Slave 1 (C),Tekanan Slave 2 (bar),Suhu Slave 2 (C),Status Slave 1,Status Slave 2\n";
constexpr size_t EXPORT_ROW_SIZE=512;
constexpr uint32_t EXPORT_STRIDE=32768;
constexpr uint8_t EXPORT_POINTS=8;
inline void status(const Record&r,uint8_t node,char*out,size_t size){
  if(!(r.connected&(1<<node))){snprintf(out,size,"TERPUTUS");return;}
  uint16_t flags=r.flags[node];uint8_t causes=r.causes[node];
  if(flags&2048){snprintf(out,size,"MENUNGGU SENSOR");return;}
  if(causes){
    const char*names[]={"Tekanan tinggi","Suhu tinggi","Gangguan sensor tekanan","Gangguan sensor suhu","Sumber simulasi digunakan saat mode LIVE"};
    size_t used=snprintf(out,size,"ALARM: ");bool first=true;
    for(uint8_t i=0;i<5;i++)if(causes&(1<<i)){
      if(used>=size)return;int n=snprintf(out+used,size-used,"%s%s",first?"":"; ",names[i]);
      if(n<0)return;used+=size_t(n);first=false;
    }
    return;
  }
  const char*label=flags&256?"ALARM: Trip tertahan":!(flags&STATUS_KNOWN)?"STATUS LAMA TIDAK TERSEDIA":flags&WAS_WARNING?"WASPADA":"NORMAL";
  snprintf(out,size,"%s",label);
}
inline void value(int16_t v,uint16_t scale,bool valid,char*out,size_t size){
  if(!valid||v<0){if(size)out[0]=0;return;}
  if(scale==100)snprintf(out,size,"%u.%02u",unsigned(v/100),unsigned(v%100));
  else snprintf(out,size,"%u.%u",unsigned(v/10),unsigned(v%10));
}
inline size_t exportCsv(const Record&r,char*out,size_t capacity){
  char stamp[24],p[2][16],t[2][16],states[2][160];
  if(r.epoch)HistoryTiming::timestamp(r.epoch,stamp,sizeof stamp);else snprintf(stamp,sizeof stamp,"Belum sinkron");
  for(uint8_t i=0;i<2;i++){
    bool online=r.connected&(1<<i);value(r.p[i],100,online&&!(r.flags[i]&4),p[i],sizeof p[i]);
    value(r.t[i],10,online&&!(r.flags[i]&8),t[i],sizeof t[i]);status(r,i,states[i],sizeof states[i]);
  }
  int n=snprintf(out,capacity,"%s,%s,%s,%s,%s,%s,%s\n",stamp,p[0],t[0],p[1],t[1],states[0],states[1]);
  return n>0&&size_t(n)<capacity?size_t(n):0;
}
struct ExportPoint{uint32_t output=0,source=0;};
struct ExportMap {
  uint32_t bytes=0;uint8_t count=0;ExportPoint points[EXPORT_POINTS];
  bool add(const Record&r,uint32_t source){
    char line[EXPORT_ROW_SIZE];size_t n=exportCsv(r,line,sizeof line);if(!n)return false;
    if(!count||(count<EXPORT_POINTS&&source-points[count-1].source>=EXPORT_STRIDE)){
      points[count].output=bytes;points[count].source=source;count++;
    }
    bytes+=n;return true;
  }
};
// Buffered input avoids a filesystem call per character while converting USB sectors.
template<class Source,size_t LocalSize=512> class CsvLines {
  Source&source;uint32_t position,limit;uint8_t local[LocalSize];uint8_t*buffer;size_t bufferSize,cursor=0,used=0;bool valid;
 public:
  CsvLines(Source&s,uint32_t start,uint32_t end,uint8_t*external=nullptr,size_t externalSize=0):source(s),position(start),limit(end),buffer(external?external:local),bufferSize(external?externalSize:sizeof local),valid(s.seek(start)&&bufferSize){}
  int next(char*out,size_t capacity,uint32_t&start){
    if(!valid){valid=true;position=limit;return -1;}if(position>=limit)return 0;
    start=position;size_t length=0;bool overflow=false;
    while(position<limit){
      if(cursor==used){size_t wanted=limit-position<bufferSize?limit-position:bufferSize;used=source.read(buffer,wanted);cursor=0;
        if(!used){position=limit;return -1;}}
      char c=buffer[cursor++];position++;
      if(c=='\n'){if(capacity)out[length]=0;return overflow?-1:1;}
      if(c!='\r'){if(length+1<capacity)out[length++]=c;else overflow=true;}
    }
    if(capacity)out[length]=0;return -1; // A partial tail is never exported as a valid reading.
  }
};
struct ProjectScratch {uint8_t input[512];char raw[192],row[EXPORT_ROW_SIZE];};
template<class Source> int projectSlice(Source&source,const ExportMap&map,uint32_t rawBytes,uint32_t offset,uint8_t*out,size_t bytes,ProjectScratch&scratch){
  if(!map.count||offset>=map.bytes)return 0;
  if(bytes>map.bytes-offset)bytes=map.bytes-offset;
  uint8_t point=0;while(point+1<map.count&&map.points[point+1].output<=offset)point++;
  CsvLines<Source,1> lines(source,map.points[point].source,rawBytes,scratch.input,sizeof scratch.input);uint32_t logical=map.points[point].output,start;
  size_t done=0;int result;
  while(done<bytes&&(result=lines.next(scratch.raw,sizeof scratch.raw,start))!=0){
    if(result<0)return -1;Record r;if(!parseCsv(scratch.raw,r))return -1;size_t n=exportCsv(r,scratch.row,sizeof scratch.row);if(!n)return -1;
    if(logical+n>offset){size_t skip=offset-logical,wanted=n-skip;if(wanted>bytes-done)wanted=bytes-done;
      memcpy(out+done,scratch.row+skip,wanted);done+=wanted;offset+=wanted;}
    logical+=n;
  }return done;
}
template<class Source> int projectSlice(Source&source,const ExportMap&map,uint32_t rawBytes,uint32_t offset,uint8_t*out,size_t bytes){
  ProjectScratch scratch;return projectSlice(source,map,rawBytes,offset,out,bytes,scratch);
}
struct ExportCache {
  uint8_t data[4096];uint32_t start=UINT32_MAX;size_t used=0;
  void clear(){start=UINT32_MAX;used=0;}
  template<class Reader> int read(uint32_t position,uint8_t*out,size_t bytes,uint32_t total,Reader reader){
    if(position>=total)return 0;if(bytes>total-position)bytes=total-position;size_t done=0;
    while(done<bytes){
      if(start==UINT32_MAX||position<start||position-start>=used){
        start=position;size_t wanted=total-position<sizeof data?total-position:sizeof data;
        int n=reader(position,data,wanted);used=n>0?size_t(n):0;if(!used)break;
      }
      size_t offset=position-start,n=bytes-done;if(n>used-offset)n=used-offset;
      memcpy(out+done,data+offset,n);done+=n;position+=n;
    }return done;
  }
};
}
