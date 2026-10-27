#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "../Firmware/Shared/IndexedFrame.h"

int main(){
  std::vector<uint8_t> canvas(Screen::BYTES+2,0xCD),shadow(Screen::BYTES+2,0xEF);
  uint8_t*next=canvas.data()+1,*previous=shadow.data()+1;
  std::vector<uint16_t> scratch(Screen::WIDTH*Screen::TILE_H),lcd(Screen::WIDTH*Screen::HEIGHT);
  int lastX=-1,lastY=-1,lastW=0,lastH=0;
  auto sink=[&](int x,int y,int w,int h,uint16_t*rgb){
    assert(x>=0&&y>=0&&x+w<=Screen::WIDTH&&y+h<=Screen::HEIGHT);
    assert(w*h<=int(scratch.size()));lastX=x;lastY=y;lastW=w;lastH=h;
    for(int row=0;row<h;row++)for(int col=0;col<w;col++)lcd[(y+row)*Screen::WIDTH+x+col]=rgb[row*w+col];
  };
  Screen::fill(next,Screen::index(Screen::BG));
  assert(Screen::present(next,previous,true,scratch.data(),sink)==22);
  assert(lastY==168&&lastH==4&&lastW==320);
  assert(Screen::present(next,previous,false,scratch.data(),sink)==0); // no SPI writes for an unchanged frame
  Screen::pixel(next,31,171,Screen::index(Screen::ACCENT));
  assert(Screen::present(next,previous,false,scratch.data(),sink)==1);
  assert(lastX==16&&lastY==168&&lastW==16&&lastH==4);
  assert(lcd[171*320+31]==Screen::ACCENT&&lcd[171*320+30]==Screen::BG);
  Screen::pixel(next,31,171,Screen::index(Screen::BG));
  assert(Screen::present(next,previous,false,scratch.data(),sink)==1); // old pixels are erased in the same tile update
  assert(lcd[171*320+31]==Screen::BG);
  Screen::pixel(next,16,20,1);Screen::pixel(next,32,20,1);
  assert(Screen::present(next,previous,false,scratch.data(),sink)==1);
  assert(lastX==16&&lastY==16&&lastW==32&&lastH==8); // adjacent tiles coalesce
  Screen::fill(next,0);Screen::rect(next,1,1,3,2,1);
  for(int y=1;y<3;y++){assert(Screen::pixel((const uint8_t*)next,0,y)==0);assert(Screen::pixel((const uint8_t*)next,4,y)==0);for(int x=1;x<4;x++)assert(Screen::pixel((const uint8_t*)next,x,y)==1);}
  Screen::rect(next,-2,-2,3,3,4);assert(Screen::pixel((const uint8_t*)next,0,0)==4);
  Screen::rect(next,319,171,20,20,5);assert(Screen::pixel((const uint8_t*)next,319,171)==5);
  Screen::pixel(next,-1,0,6);Screen::pixel(next,320,0,6);Screen::pixel(next,0,172,6);
  Screen::present(next,previous,false,scratch.data(),sink);
  assert(!memcmp(next,previous,Screen::BYTES));
  assert(canvas.front()==0xCD&&canvas.back()==0xCD&&shadow.front()==0xEF&&shadow.back()==0xEF);
  assert(Screen::PALETTE[Screen::index(Screen::BLACK)]==0&&Screen::PALETTE[Screen::index(Screen::WHITE)]==0xFFFF);
  puts("Indexed framebuffer: unchanged frames, dirty rectangles, erasure, clipping, palette and guard bytes passed.");
}
