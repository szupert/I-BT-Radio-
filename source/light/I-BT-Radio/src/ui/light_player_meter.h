#pragma once
#include <stdint.h>
namespace lightmeter {
constexpr unsigned steps=24;
constexpr uint16_t panel=0x0862,border=0x2146,well=0x10c4,white=0xffff;
inline uint16_t color(unsigned step){
 unsigned r,g,b;
 if(step<14){r=20;g=180+step*4;b=190-step*7;}
 else if(step<20){r=80+(step-14)*30;g=230;b=40;}
 else{r=255;g=170-(step-20)*38;b=35;}
 return uint16_t(((uint32_t(r)>>3)<<11)|((uint32_t(g)>>2)<<5)|(uint32_t(b)>>3));
}
template<class LCD> void segment(LCD& lcd,unsigned style,unsigned ch,unsigned j,bool lit){
 if(style==1)lcd.fillRect(13+int(ch)*40,157-int(j)*3,30,2,lit?color(j):well);
 else lcd.fillRect(12+int(j)*3,91+int(ch)*44,2,22,lit?color(j):well);
}
template<class LCD> void cap(LCD& lcd,unsigned style,unsigned ch,unsigned peak){
 if(!peak)return;
 if(style==1)lcd.fillRect(13+int(ch)*40,157-int(peak-1)*3,30,1,white);
 else lcd.fillRect(12+int(peak-1)*3,91+int(ch)*44,1,22,white);
}
template<class LCD> void channel(LCD& lcd,unsigned style,unsigned ch,unsigned level,unsigned peak,bool layout){
 // One shared frame and symmetric channels: the stereo layout needs no B/J labels.
 if(layout&&ch==0){lcd.fillRoundRect(8,80,80,88,6,panel);lcd.drawRoundRect(8,80,80,88,6,border);if(style==1)lcd.fillRect(47,87,1,74,border);else lcd.fillRect(14,123,68,1,border);}
 for(unsigned j=0;j<steps;++j)segment(lcd,style,ch,j,j<level);
 cap(lcd,style,ch,peak);
}
// Previous level/peak come from the existing UI owner cache, never a framebuffer.
template<class LCD> void channelDelta(LCD& lcd,unsigned style,unsigned ch,unsigned level,unsigned peak,unsigned previousLevel,unsigned previousPeak,bool layout){
 if(layout){channel(lcd,style,ch,level,peak,true);return;}
 const unsigned lo=level<previousLevel?level:previousLevel,hi=level>previousLevel?level:previousLevel;
 for(unsigned j=lo;j<hi;++j)segment(lcd,style,ch,j,j<level);
 if(previousPeak&&previousPeak!=peak){const unsigned j=previousPeak-1;if(j<lo||j>=hi)segment(lcd,style,ch,j,j<level);}
 if(peak&&(peak!=previousPeak||(peak-1>=lo&&peak-1<hi)))cap(lcd,style,ch,peak);
}
template<class LCD> void compact(LCD& lcd,unsigned left,unsigned right,bool layout){
 if(layout){lcd.fillRoundRect(8,99,144,42,5,panel);lcd.drawRoundRect(8,99,144,42,5,border);}
 const unsigned values[]={left,right};
 for(unsigned ch=0;ch<2;++ch)for(unsigned j=0;j<steps;++j)lcd.fillRect(14+int(j)*5,105+int(ch)*19,4,11,j<values[ch]?color(j):well);
}
template<class LCD> void compactDelta(LCD& lcd,unsigned left,unsigned right,unsigned previousLeft,unsigned previousRight,bool layout){
 if(layout){compact(lcd,left,right,true);return;}
 const unsigned now[]={left,right},old[]={previousLeft,previousRight};
 for(unsigned ch=0;ch<2;++ch){const unsigned lo=now[ch]<old[ch]?now[ch]:old[ch],hi=now[ch]>old[ch]?now[ch]:old[ch];
  for(unsigned j=lo;j<hi;++j)lcd.fillRect(14+int(j)*5,105+int(ch)*19,4,11,j<now[ch]?color(j):well);
 }
}
}
