#include <assert.h>
#include <stdio.h>
#include "../Firmware/Shared/UsbSession.h"

int main(){
  UsbExport::Session s;
  // CDC already mounted USB before the application's event handler was installed.
  s.observe(true,1000);assert(s.connected&&s.requested);
  assert(!s.due(1499,true,true));assert(s.due(1500,true,true));
  // RAM-ready is not filesystem-ready; never advertise a nonexistent history.
  assert(!s.due(1600,true,false));assert(!s.due(1600,false,true));
  assert(s.due(1700,true,true));
  // A temporary snapshot failure retries without requiring replug/reset.
  s.failed(1700);assert(s.requested);assert(!s.due(2699,true,true));assert(s.due(2700,true,true));
  s.inserted();assert(!s.due(3000,true,true));
  s.observe(true,3100);assert(!s.requested); // polling the host does not repeatedly remount a ready drive
  s.eject();assert(s.ejected&&!s.requested);s.observe(true,3200);assert(!s.due(9000,true,true));
  s.refresh(9000);assert(!s.ejected);assert(!s.due(9599,true,true));assert(s.due(9600,true,true));
  s.inserted();s.eject();s.observe(false,10000);assert(!s.connected&&!s.requested);
  s.observe(true,11000);assert(!s.ejected&&s.due(11500,true,true));
  // Filesystem initialization may finish later with no new USB callback.
  assert(!s.due(12000,true,false));assert(s.due(12001,true,true));
  UsbExport::Session wrap;wrap.observe(true,UINT32_MAX-100);
  assert(!wrap.due(398,true,true));assert(wrap.due(399,true,true));
  puts("USB mount lifecycle: missed startup event, filesystem/recovery gating, retry, eject/refresh, reconnect and rollover passed.");
}
