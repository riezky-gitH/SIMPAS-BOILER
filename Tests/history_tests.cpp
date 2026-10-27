#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../Firmware/Shared/HistoryRecord.h"

int main(){
  HistoryTiming::Schedule schedule;
  assert(schedule.periodic(100));assert(!schedule.periodic(2100));
  assert(schedule.observe(0,64)); // Threshold crossed 2 seconds into the interval.
  assert(!schedule.observe(0,64)); // Sustained high readings do not flood the log.
  assert(!schedule.periodic(5099));assert(schedule.periodic(5100));
  assert(schedule.observe(0,192)); // Temperature can independently trigger a row.
  assert(!schedule.observe(0,0));assert(schedule.observe(0,64)); // Re-crossing.
  assert(schedule.observe(1,64)); // Second boiler is independent.
  assert(schedule.periodic(15100));assert(!schedule.periodic(15101));
  HistoryTiming::Schedule wrap;assert(wrap.periodic(UINT32_MAX-2000));
  assert(!wrap.periodic(2998));assert(wrap.periodic(2999));
  char stamp[24];HistoryTiming::timestamp(0,stamp,sizeof stamp);assert(!strcmp(stamp,"UNSYNCED"));
  HistoryTiming::timestamp(1704067200,stamp,sizeof stamp);assert(!strcmp(stamp,"2024-01-01 07:00:00"));
  HistoryTiming::timestamp(1704043800,stamp,sizeof stamp);assert(!strcmp(stamp,"2024-01-01 00:30:00"));
  assert(HistoryTiming::inferredEpoch(0,42,10,42,1704067200,20)==1704067190);
  assert(HistoryTiming::inferredEpoch(0,41,10,42,1704067200,20)==0);
  assert(HistoryTiming::inferredEpoch(1700000001,42,10,42,1704067200,20)==1700000001);
  assert(HistoryTiming::inferredEpoch(0,42,30,42,1704067200,20)==0);
  Historian::Record row;row.epoch=1704067200;row.boot=42;row.seconds=5;
  row.p[0]=450;row.t[0]=1750;row.flags[0]=19;row.connected=1;row.causes[0]=3;
  char encoded[192];size_t size=Historian::csv(row,encoded,sizeof encoded);
  assert(size>0&&strstr(encoded,",2024-01-01 07:00:00,"));encoded[size-1]=0;
  char copy[192];strcpy(copy,encoded);Historian::Record decoded;
  assert(Historian::parseCsv(copy,decoded));assert(decoded.epoch==row.epoch&&decoded.connected==1&&decoded.p[0]==450&&decoded.causes[0]==3);
  strcpy(copy,encoded);copy[0]='2';assert(!Historian::parseCsv(copy,decoded));
  char legacy[192]="0,41,5,200,1000,32771,0,0,0,200,1000,32771,0,0,0";
  size_t length=strlen(legacy);snprintf(legacy+length,sizeof legacy-length,",%04X",Link::crc16(legacy,length));
  assert(Historian::parseCsv(legacy,decoded));size=Historian::csv(decoded,encoded,sizeof encoded);
  assert(size&&strstr(encoded,",UNSYNCED,")); // Old boot has no trustworthy clock anchor.
  assert(!Historian::csv(row,encoded,32));
  puts("History: fixed 5-second cadence, immediate independent crossings, rollover, WIB, clock anchor, legacy conversion and CRC passed.");
}
