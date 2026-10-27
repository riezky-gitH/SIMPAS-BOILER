#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Two exact 4-bit frames use 55,040 bytes instead of 220,160 bytes for RGB565.
// All screen drawing happens off-screen; only changed 16x8 tiles reach SPI.
namespace Screen {
constexpr int WIDTH=320,HEIGHT=172,STRIDE=WIDTH/2,TILE_W=16,TILE_H=8;
constexpr size_t BYTES=STRIDE*HEIGHT;
constexpr uint16_t rgb(uint8_t r,uint8_t g,uint8_t b){return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);}
constexpr uint16_t BG=rgb(233,233,231),PAPER=rgb(250,250,248),INK=rgb(41,42,45),MUTED=rgb(114,114,116);
constexpr uint16_t ACCENT=rgb(128,0,32),RED=rgb(154,22,50),GREEN=rgb(65,100,81),AMBER=rgb(145,96,24);
constexpr uint16_t LINE=rgb(212,212,209),FOOTER=rgb(52,52,54),WHITE=0xFFFF,BLACK=0;
constexpr uint16_t WARN_BG=rgb(247,240,228),RED_BG=rgb(248,241,243);
constexpr uint16_t WARNING_EDGE=rgb(255,221,0),ALARM_EDGE=rgb(235,35,45);
constexpr uint16_t PALETTE[16]={BG,PAPER,INK,MUTED,ACCENT,RED,GREEN,AMBER,LINE,FOOTER,WHITE,BLACK,WARN_BG,RED_BG,WARNING_EDGE,ALARM_EDGE};
inline uint8_t index(uint16_t color){for(uint8_t i=0;i<16;i++)if(PALETTE[i]==color)return i;return 2;}
inline void pixel(uint8_t*data,int x,int y,uint8_t value){
  if(!data||x<0||x>=WIDTH||y<0||y>=HEIGHT)return;
  uint8_t&b=data[y*STRIDE+x/2];value&=15;
  if(x&1)b=(b&0xF0)|value;else b=(b&0x0F)|(value<<4);
}
inline uint8_t pixel(const uint8_t*data,int x,int y){return (data[y*STRIDE+x/2]>>((x&1)?0:4))&15;}
inline void fill(uint8_t*data,uint8_t value){if(data)memset(data,(value<<4)|value,BYTES);}
inline void rect(uint8_t*data,int x,int y,int w,int h,uint8_t value){
  if(!data||w<=0||h<=0)return;
  int right=x+w,bottom=y+h;if(x<0)x=0;if(y<0)y=0;
  if(right>WIDTH)right=WIDTH;if(bottom>HEIGHT)bottom=HEIGHT;
  if(x>=right||y>=bottom)return;
  for(int row=y;row<bottom;row++){
    int first=x;if(first&1){pixel(data,first,row,value);first++;}
    int pairs=(right-first)/2;if(pairs)memset(data+row*STRIDE+first/2,(value<<4)|value,pairs);
    if((right-first)&1)pixel(data,right-1,row,value);
  }
}
inline bool tileChanged(const uint8_t*next,const uint8_t*previous,int x,int y,int rows){
  for(int row=0;row<rows;row++)if(memcmp(next+(y+row)*STRIDE+x/2,previous+(y+row)*STRIDE+x/2,TILE_W/2))return true;
  return false;
}
template<class Sink> size_t present(const uint8_t*next,uint8_t*previous,bool force,uint16_t*scratch,Sink sink){
  if(!next||!previous||!scratch)return 0;size_t blocks=0;
  for(int y=0;y<HEIGHT;y+=TILE_H){int rows=HEIGHT-y<TILE_H?HEIGHT-y:TILE_H;
    for(int x=0;x<WIDTH;){
      if(!force&&!tileChanged(next,previous,x,y,rows)){x+=TILE_W;continue;}
      int first=x;x+=TILE_W;
      while(x<WIDTH&&(force||tileChanged(next,previous,x,y,rows)))x+=TILE_W;
      int width=x-first;
      for(int row=0;row<rows;row++)for(int col=0;col<width;col++)scratch[row*width+col]=PALETTE[pixel(next,first+col,y+row)];
      sink(first,y,width,rows,scratch);blocks++;
      for(int row=0;row<rows;row++)memcpy(previous+(y+row)*STRIDE+first/2,next+(y+row)*STRIDE+first/2,width/2);
    }
  }
  return blocks;
}
}
