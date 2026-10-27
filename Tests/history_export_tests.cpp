#include <assert.h>
#include <string>
#include <stdio.h>
#include <vector>
#include "../Firmware/Shared/HistoryExport.h"
#include "../Firmware/Shared/Fat16.h"

struct MemorySource {
  const std::string&bytes;size_t pos=0,reads=0;
  explicit MemorySource(const std::string&s):bytes(s){}
  bool seek(uint32_t at){pos=at;return pos<=bytes.size();}
  size_t read(uint8_t*out,size_t n){if(n>bytes.size()-pos)n=bytes.size()-pos;memcpy(out,bytes.data()+pos,n);pos+=n;reads++;return n;}
};
static Historian::Record record(){
  Historian::Record r;r.epoch=1704067200;r.boot=42;r.seconds=5;r.connected=3;
  r.p[0]=261;r.t[0]=807;r.p[1]=216;r.t[1]=525;r.flags[0]=r.flags[1]=Historian::STATUS_KNOWN|3;return r;
}
int main(int argc,char**argv){
  auto r=record();char row[Historian::EXPORT_ROW_SIZE];size_t n=Historian::exportCsv(r,row,sizeof row);
  assert(n&&std::string(row)=="2024-01-01 07:00:00,2.61,80.7,2.16,52.5,NORMAL,NORMAL\n");
  r.flags[0]|=Historian::WAS_WARNING;Historian::exportCsv(r,row,sizeof row);assert(strstr(row,",WASPADA,NORMAL\n"));
  r.causes[0]=2;Historian::exportCsv(r,row,sizeof row);assert(strstr(row,"ALARM: Suhu tinggi"));
  r.causes[0]=r.causes[1]=31;Historian::exportCsv(r,row,sizeof row);
  assert(strstr(row,"Tekanan tinggi; Suhu tinggi; Gangguan sensor tekanan; Gangguan sensor suhu; Sumber simulasi digunakan saat mode LIVE"));
  assert(strlen(row)<sizeof row-1);int commas=0;for(char*p=row;*p;p++)commas+=*p==',';assert(commas==6);
  assert(!Historian::exportCsv(r,row,16));
  r=record();r.connected=0;Historian::exportCsv(r,row,sizeof row);assert(strstr(row,",,,,,TERPUTUS,TERPUTUS\n"));
  r=record();r.flags[0]|=4;r.causes[0]=4;r.p[0]=-1;Historian::exportCsv(r,row,sizeof row);assert(strstr(row,",,80.7,")&&strstr(row,"ALARM: Gangguan sensor tekanan"));
  r=record();r.flags[1]|=8;r.causes[1]=8;Historian::exportCsv(r,row,sizeof row);assert(strstr(row,",2.16,,")&&strstr(row,"ALARM: Gangguan sensor suhu"));
  r=record();r.flags[0]|=256;Historian::exportCsv(r,row,sizeof row);assert(strstr(row,"ALARM: Trip tertahan"));
  r=record();r.flags[0]|=2048;Historian::exportCsv(r,row,sizeof row);assert(strstr(row,"MENUNGGU SENSOR"));
  r=record();r.flags[0]=3;r.epoch=0;Historian::exportCsv(r,row,sizeof row);assert(strstr(row,"Belum sinkron,")&&strstr(row,"STATUS LAMA TIDAK TERSEDIA"));
  // Exercise the same variable-width projection used by USB, HTTP and serial.
  std::string raw=Historian::CSV_HEADER,expected;Historian::ExportMap map;
  for(int i=0;i<1800;i++){
    r=record();r.seconds+=i*5;r.epoch+=i*5;r.flags[0]|=(i%3==0?Historian::WAS_WARNING:0);r.causes[1]=i%7==0?31:0;
    assert(map.add(r,raw.size()));char internal[192];size_t count=Historian::csv(r,internal,sizeof internal);assert(count);raw.append(internal,count);
    n=Historian::exportCsv(r,row,sizeof row);expected.append(row,n);
  }
  assert(map.count>1&&map.bytes==expected.size());MemorySource source(raw);
  const size_t sizes[]={1,17,128,512,4096,8192};
  for(size_t size:sizes)for(size_t offset=0;offset<expected.size();offset+=997){
    std::vector<uint8_t> out(size);int got=Historian::projectSlice(source,map,raw.size(),offset,out.data(),size);
    assert(got>=0&&std::string((char*)out.data(),got)==expected.substr(offset,size));
  }
  std::vector<uint8_t> out(512);assert(Historian::projectSlice(source,map,raw.size(),expected.size(),out.data(),512)==0);
  Historian::ExportCache cache;unsigned refills=0;
  auto read=[&](uint32_t pos,uint8_t*data,size_t bytes){refills++;return Historian::projectSlice(source,map,raw.size(),pos,data,bytes);};
  for(uint32_t pos=0;pos<8192;pos+=128){int got=cache.read(pos,out.data(),128,expected.size(),read);assert(got==128&&std::string((char*)out.data(),got)==expected.substr(pos,got));}
  assert(refills==2); // Serial/USB chunks reuse 4 KiB windows instead of rescanning each sector.
  for(uint32_t pos=13;pos<expected.size();pos+=7919){int got=cache.read(pos,out.data(),512,expected.size(),read);assert(std::string((char*)out.data(),got)==expected.substr(pos,got));}
  cache.clear();assert(cache.read(0,out.data(),512,expected.size(),read)==512);assert(!memcmp(out.data(),expected.data(),512));
  assert(cache.read(expected.size(),out.data(),512,expected.size(),read)==0);
  std::string corrupt=raw;corrupt[strlen(Historian::CSV_HEADER)]='9';MemorySource damaged(corrupt);
  assert(Historian::projectSlice(damaged,map,corrupt.size(),0,out.data(),512)==-1);
  // Unterminated tails and overlong lines must be rejected, without stalling.
  std::string broken="good\npartial";MemorySource truncated(broken);Historian::CsvLines<MemorySource> lines(truncated,0,broken.size());char line[16];uint32_t start;
  assert(lines.next(line,sizeof line,start)==1&&start==0&&!strcmp(line,"good"));assert(lines.next(line,sizeof line,start)==-1);assert(lines.next(line,sizeof line,start)==0);
  std::string large(200,'x');large+="\nok\n";MemorySource tooLong(large);Historian::CsvLines<MemorySource> longLines(tooLong,0,large.size());
  assert(longLines.next(line,sizeof line,start)==-1);assert(longLines.next(line,sizeof line,start)==1&&!strcmp(line,"ok"));
  uint8_t boot[512];UsbExport::sector(0,boot,expected.size(),nullptr);
  assert(boot[19]==0&&boot[20]==0);assert((uint32_t(boot[32])|(uint32_t(boot[33])<<8)|(uint32_t(boot[34])<<16)|(uint32_t(boot[35])<<24))==UsbExport::SECTORS);
  assert(UsbExport::SECTORS==131072&&(UsbExport::SECTORS-UsbExport::DATA_START)/4<65525);
  assert(!UsbExport::sector(UsbExport::DATA_START,boot,512,[](uint32_t,uint8_t*,size_t)->int{return -1;}));
  if(argc==2){FILE*f=fopen(argv[1],"wb");assert(f);fputs(Historian::EXPORT_HEADER,f);r=record();n=Historian::exportCsv(r,row,sizeof row);fwrite(row,1,n,f);
    r.p[0]=460;r.t[1]=1650;r.flags[0]|=Historian::WAS_WARNING;r.causes[1]=2;n=Historian::exportCsv(r,row,sizeof row);fwrite(row,1,n,f);fclose(f);}
  puts("Readable history: seven columns, real units, all alarm causes, missing readings, historical status, indexed random USB/HTTP reads and FAT16 geometry passed.");
}
