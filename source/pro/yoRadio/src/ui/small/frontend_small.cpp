#include "clock_design.h"
#include "../alarm_layout.h"
#include "../setup_layout.h"
#include "../wifi_password_layout.h"
#include "../../dual/v026_probe.h"
#include "../../dual/bt_audio.h"
#include "../../dual/menu_render_policy.h"
#include "../../dual/bt_signal.h"
#include "../../dual/bt_media.h"
#include "../../dual/track_text.h"
#include "../../dual/vu_probe.h"
#include "../../dual/artwork_ab.h"
#include "bt_thin_fonts.h"
#include "../../dual/load_probe.h"
#include <Preferences.h>
#include <math.h>
#include <cstring>
#include "../../core/options.h"
#if defined(BOARD_CYD28)
#include "../frontend.h"
#include "../content.h"
#include "../fonts.h"
#include "../../dual/artwork.h"
#include "../../dual/online_artwork.h"
#include "../../core/config.h"
#include "../../core/player.h"
#include "../../core/network.h"
#if FEATURE_SD
#include <SD.h>
#include "../../core/sdmanager.h"
#endif
#include "../../dual/v027_himem.h"
namespace board {
int rotaryDebugClk(); int rotaryDebugDt(); int rotaryDebugSw();
uint32_t rotaryDebugClkChanges(); uint32_t rotaryDebugDtChanges(); uint32_t rotaryDebugSwChanges();
uint32_t rotaryDebugClkRise(); uint32_t rotaryDebugClkFall();
uint32_t rotaryDebugDtRise(); uint32_t rotaryDebugDtFall();
uint32_t rotaryDebugCW(); uint32_t rotaryDebugCCW();
int rotaryDebugDirection();
}
#include <Adafruit_ST7789.h>
#include "bootlogo_rle.h"
#include "cassette_photo.h"
#include "skin_assets.h"
#include "retro_palette.h"
#include "neon_asset.h"
#include "videoton_face.h"
#include "service_photo.h"
#include "bt_speaker_photo.h"
#include "videoton_boot.h"
#include "../../dual/bt_play_clock.h"
#include "weather_face.h"
#include "../interaction_policy.h"
#include "player_skin_assets.h"
#include "media_player_asset.h"


#include "boot_rgba32.h"
#include "os_boot.h"
#include "../../dual/boot_sequence.h"
#include "rgba_pixel.h"
#include "skin_gallery.h"
#include <time.h>
#include "../cassette_layout.h"
#include "../../dual/resource_limiter.h"
#include "../../dual/backup_sd.h"
#include <SPIFFS.h>
#include <esp_heap_caps.h>
namespace front {
static SPIClass lcdSPI(HSPI);
static Adafruit_ST7789 lcd(&lcdSPI,TFT_CS,TFT_DC,TFT_RST);
static uint16_t bg=0x0841,fg=0xffff,accent=0x05da,muted=0x7bef;
static bool previousDark=true,valid=false;
static bool overloadDotPainted=false;
static bool previousCassette=true,cassetteRequested=true;
static bool previousPlayer=false,playerRequested=false;
static bool previousMediaPlayer=false,previousKajon=false;
static uint8_t previousVintage=0,previousRetroColor=0;
static unsigned previousPage=99;
static bool previousSetup=false,artDirty=true;
static bool connectionScreen=false;
static bool previousNightClock=false;
static dual::Language previousLanguage=dual::Language::HU;
static uint32_t previousGeneration=0;
static int artX=8,artY=62,artW=0,artH=0;
static uint16_t *coverPixels=nullptr;
static bool coverVisible=false,cassetteDirty=false;
static uint32_t vintageCoverRevision=0;
// Optional redraw cache: external RAM only, owned by the UI thread.
struct UiCache { uint16_t glyphScratch[32*24]{}; char trackLine[192]{}; char rows[16][192]{}; int widths[16]{},rowX[16]{},rowY[16]{}; uint16_t rowColor[16]{};
 unsigned menuFirst=0,scrollSelected=~0u,scrollCount=0,scrollVisible=0;
 char oldStation[352]{},oldTime[20]{},oldBitrate[30]{},oldRate[12]{},servicePrevious[160]{},serviceStatus[100]{};float coefficients[48]{};uint8_t previousBands[96]{};};
static UiCache* uiCache=nullptr;
static char (*rows)[192]=nullptr;
static bool ensureRows(){
    if(!uiCache){void* p=heap_caps_malloc(sizeof(UiCache),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(p){uiCache=new(p)UiCache();rows=uiCache->rows;}}
    return rows!=nullptr;
}

// One UI-owner scratch, PSRAM only. SPI bitmap writes finish before reuse.
// No recursive rendering, internal-RAM fallback or per-frame allocation.
static uint16_t* pixelScratch(){return ensureRows()?uiCache->glyphScratch:nullptr;}
// Isolate the helper frame from the shared renderer; no new buffer/allocation.
static __attribute__((noinline)) void clockMenuText(int x,int y,int limit,const char*s,unsigned size,uint16_t color=clockart::fg,uint16_t back=clockart::bg,bool center=false){
    static_assert(sizeof(UiCache::glyphScratch)/sizeof(uint16_t)>=480,"Menu glyph scratch capacity");
    if(!clockart::textBatch(lcd,pixelScratch(),sizeof(UiCache::glyphScratch)/sizeof(uint16_t),x,y,limit,s,size,color,back,center))
        clockart::text(lcd,x,y,limit,s,size,color,back,center);
}
// Keep every clock scanline restore outside the shared render frame. A single
// isolated call lets the compiler inline a different call to the same template.
static __attribute__((noinline)) void clockRestore(int x,int y,int width,int height){
    clockart::restore(lcd,x,y,width,height);
}


static menurender::Slices menuSlices;
bool renderPending(){return menuSlices.active;}
static SkinGallery gallery;
static void drawSkin(unsigned index,uint8_t retroChoice=0){
    if(index>=7)return;
    uint16_t* scan=pixelScratch();if(!scan)return;
    const int w=lcd.width(),h=lcd.height();
    if(w>320||h>320||w<1||h<1)return;
    const uint16_t* pixels=index==0?packedpixels::pixels(bootPanelAsset):skinPixels(index);
    if(!pixels)return;
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            const unsigned offset=(y*240/h)*320+x*320/w;
            scan[x]=index==4?retrocolor::pixel(pixels[offset],retroChoice):pixels[offset];
        }
        lcd.drawRGBBitmap(0,y,scan,w,1);
        if((y&7)==7)vTaskDelay(1);
    }
}
static void drawPlayer(const uint16_t *pixels){
    if(!pixels)return;
    uint16_t* scan=pixelScratch();if(!scan)return;
    const int w=lcd.width(),h=lcd.height();
    if(w>320||h>320||w<1||h<1)return;
    for(int y=0;y<h;y++){
        for(int x=0;x<w;x++)scan[x]=pixels[(y*240/h)*320+x*320/w];
        lcd.drawRGBBitmap(0,y,scan,w,1);
        if((y&15)==15)vTaskDelay(1);
    }
}
// Match dynamic fields to the offline crop of the static player art.
static constexpr int winX(int x){return ((x-4)*320+157)/314;}
static constexpr int winY(int y){return ((y-2)*240+116)/233;}
static constexpr int winW(int x,int w){return winX(x+w)-winX(x);}
static constexpr int winH(int y,int h){return winY(y+h)-winY(y);}
static bool menuTypography=false;
// Touch must use the viewport actually painted, even if rotary/scroll input
// is still waiting behind the audio gate. A stale page cannot activate rows.
bool menuViewport(unsigned page,unsigned &first){
    if(!valid||!uiCache||menuSlices.active||previousPage!=page||!menuTypography||connectionScreen||gallery.active())return false;
    first=uiCache->menuFirst;return true;
}
static bool previousBatteryEnabled=false,previousWifiEnabled=false;
static uint16_t* menuRowCanvas=nullptr;
static int16_t menuCanvasX=0,menuCanvasY=0,menuCanvasWidth=0;
static int menuTextWidth(const char* p){int width=0;while(*p)width+=fonts::find(fonts::font16,dual::nextUtf8(p)).advance;return width;}
void skinPreview(int index){gallery.select(index);valid=false;}
bool skinPreviewActive(){return gallery.active();}
bool skinPreviewInput(dual::Action a){bool used=gallery.input(a);if(used)valid=false;return used;}

dual::Size nativeSize(){return {320,240};}
static void text(int x,int y,int maxWidth,const char *value,uint16_t color) {
    const fonts::Font &font=fonts::font16;const char *p=value;
    uint16_t* glyphPixels=pixelScratch();if(!glyphPixels)return;int start=x;
    while(*p) {
        const auto &g=fonts::find(font,dual::nextUtf8(p));
        if(x-start+g.w>maxWidth||g.w>32||g.h>24)break;
        const uint8_t *bitmap=font.data+g.offset;
        for(unsigned j=0;j<unsigned(g.w)*g.h;j++){
            bool ink=bitmap[j/8]&(0x80>>(j&7));
            if(menuTypography&&j%g.w){unsigned left=j-1;ink=ink||(bitmap[left/8]&(0x80>>(left&7)));}
            glyphPixels[j]=ink?color:bg;
        }
        if(menuCanvasWidth){
            for(int yy=0;yy<g.h;++yy)for(int xx=0;xx<g.w;++xx){
                const int dx=x+g.x+xx-menuCanvasX,dy=y+yy-menuCanvasY;
                if(dx>=0&&dx<menuCanvasWidth&&dy>=0&&dy<22)menuRowCanvas[dy*menuCanvasWidth+dx]=glyphPixels[yy*g.w+xx];
            }
        }else lcd.drawRGBBitmap(x+g.x,y,glyphPixels,g.w,g.h);
        x+=g.advance;
    }
}
// Reuse the existing Unicode glyphs at half size for finer station text.
static void textSmall(int x,int y,int maxWidth,const char *value,uint16_t color,uint16_t background){
    const fonts::Font &font=fonts::font16;const char *p=value;
    uint16_t* glyphPixels=pixelScratch();if(!glyphPixels)return;const int start=x;
    while(*p){
        const auto &g=fonts::find(font,dual::nextUtf8(p));
        const int w=(int(g.w)+1)/2,h=(int(g.h)+1)/2;
        if(x-start+w>maxWidth||w>16||h>12)break;
        const uint8_t *bitmap=font.data+g.offset;
        for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){
            bool ink=false;
            for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
                const int sx=xx*2+dx,sy=yy*2+dy;
                if(sx<int(g.w)&&sy<int(g.h)){
                    const int bit=sy*int(g.w)+sx;
                    ink|=(bitmap[bit/8]&(0x80>>(bit&7)))!=0;
                }
            }
            glyphPixels[yy*w+xx]=ink?color:background;
        }
        lcd.drawRGBBitmap(x+g.x/2,y,glyphPixels,w,h);
        x+=(int(g.advance)+1)/2;
    }
}
// Offline one/two-pixel Unicode glyphs; no blur, bold, resize, heap or new task.
static void textCompactHeight(int x,int y,int maxWidth,const char* value,uint16_t color,uint16_t background,unsigned nominal){
    const auto& font=nominal==10?btfonts::font10:nominal==11?btfonts::font11:btfonts::font8;
    const char* p=value;const int start=x;uint16_t* pixels=pixelScratch();if(!pixels)return;
    while(*p){
        const auto& g=fonts::find(font,dual::nextUtf8(p));
        if(g.w>32||g.h>24||x-start+int(g.w)>maxWidth)break;
        for(unsigned j=0;j<unsigned(g.w)*g.h;++j)pixels[j]=(font.data[g.offset+j/8]&(0x80>>(j&7)))?color:background;
        lcd.drawRGBBitmap(x+g.x,y,pixels,g.w,g.h);x+=g.advance;
    }
}
// Eight rows using native font16; no bitmap thickening or scaling.
// The home view keeps its existing typography.
static void playerListText(int x,int y,int maxWidth,const char *value,uint16_t color,uint16_t background){
    const fonts::Font &font=fonts::font16;const char *p=value;
    // This page is optional UI work; keep the enlarged glyph scratch out of
    // scarce internal DRAM and leave the audio path untouched.
    static uint16_t *glyphPixels=nullptr;
    if(!glyphPixels)glyphPixels=(uint16_t*)heap_caps_malloc(24*21*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!glyphPixels)return;
    const int start=x;
    while(*p){
        const auto &g=fonts::find(font,dual::nextUtf8(p));
        const int w=int(g.w),h=int(g.h);
        if(w<1||h<1||w>24||h>21||x-start+w>maxWidth)break;
        const uint8_t *bitmap=font.data+g.offset;
        for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){
            const int bit=yy*w+xx;
            const bool ink=(bitmap[bit/8]&(0x80>>(bit&7)))!=0;
            glyphPixels[yy*w+xx]=ink?color:background;
        }
        lcd.drawRGBBitmap(x+g.x,y,glyphPixels,w,h);
        x+=g.advance;
    }
}
static bool line(unsigned id,int x,int y,int w,const char *s,uint16_t color,bool force) {
    if(id>=16||w<1)return false;
    if(force||uiCache->widths[id]!=w||uiCache->rowX[id]!=x||uiCache->rowY[id]!=y||uiCache->rowColor[id]!=color||strcmp(rows[id],s)) {
        const bool compose=menuTypography&&id>=2&&w<=320;
        if(compose&&!menuRowCanvas)menuRowCanvas=(uint16_t*)heap_caps_malloc(320*22*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(compose&&menuRowCanvas){menuCanvasX=x;menuCanvasY=y;menuCanvasWidth=w;std::fill_n(menuRowCanvas,w*22,bg);}
        else lcd.fillRect(x,y,w,22,bg);
        const char* split=menuTypography&&id>=2?strstr(s,": "):nullptr;
        if(split){
            char label[170];size_t n=std::min<size_t>(size_t(split-s),sizeof(label)-1);memcpy(label,s,n);label[n]=0;
            const char* value=split+2;const int vw=std::min(w/2,menuTextWidth(value));
            text(x,y,w-vw-12,label,color);text(x+w-vw,y,vw,value,color);
        }else text(x,y,w,s,color);
        if(menuCanvasWidth){lcd.drawRGBBitmap(x,y,menuRowCanvas,w,22);menuCanvasWidth=0;}
        dual::textCopy(rows[id],sizeof(rows[id]),s);uiCache->widths[id]=w;uiCache->rowX[id]=x;uiCache->rowY[id]=y;uiCache->rowColor[id]=color;
        return true;
    }
    return false;
}
static void playerField(char *cache,size_t cap,int x,int y,int w,int h,const char *value,uint16_t color,bool force){
    if(!force&&!strcmp(cache,value))return;
    // The baked example glyphs are absent; clear the whole black well.
    lcd.fillRect(x,y,w,h,0x0000);
    textSmall(x+1,y+1,w-2,value,color,0x0000);
    dual::textCopy(cache,cap,value);
}
// Compact, fixed-width numerals stay inside their own bitmap well.
static void playerNumber(char *cache,size_t cap,int x,int y,int w,int h,const char *value,int scale,bool force){
    if(!force&&!strcmp(cache,value))return;
    static const uint8_t digits[10][7]={
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
    static constexpr int kPixels=winW(44,75)*winH(25,19);
    static uint16_t* pixels=nullptr;
    if(!pixels)pixels=(uint16_t*)heap_caps_malloc(kPixels*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!pixels)return;
    if(w<1||h<1||w*h>kPixels)return;
    if(x<0||y<0||x+w>320||y+h>240)return;
    for(int row=0;row<h;++row)
        for(int col=0;col<w;++col)pixels[row*w+col]=playerSkinEq[(y+row)*320+x+col];
    int pen=1,top=(h-7*scale)/2;
    for(const char *c=value;*c&&pen+5*scale<=w;c++){
        for(int row=0;row<7;row++){
            uint8_t bits=(*c>='0'&&*c<='9')?digits[*c-'0'][row]:
                (*c==':'&&(row==2||row==5)?4:0);
            for(int col=0;col<5;col++)if(bits&(16>>col))
                for(int sy=0;sy<scale;sy++)for(int sx=0;sx<scale;sx++)
                    pixels[(top+row*scale+sy)*w+pen+col*scale+sx]=0x07e0;
        }
        pen+=5*scale+1;
    }
    lcd.drawRGBBitmap(x,y,pixels,w,h);
    dual::textCopy(cache,cap,value);
}
static void restorePlayerVolume(int volume){
    // One hue for the whole track, interpolated across the volume marks.
    // Match the inverse floor mapping used by the offline bitmap crop, not
    // rounded forward bounds: source row 68 occupies TWO display rows.
    // Keep the lower border neutral; the old bitmap edge had a green cast.
    static const uint8_t colors[5][3]={{0,132,0},{0,210,0},{230,210,0},
                                       {255,130,0},{235,20,12}};
    const int section=volume>=100?3:volume/25;
    const int offset=volume>=100?25:volume%25;
    uint8_t tint[3];
    for(int c=0;c<3;c++)tint[c]=uint8_t((int(colors[section][c])*(25-offset)+
                                          int(colors[section+1][c])*offset+12)/25);
    uint16_t row[82];
    const int x0=winX(126),x1=winX(200);
    for(int y=winY(60);y<winY(76);y++){
        const int sourceY=2+(y*233)/240;
        for(int x=x0;x<x1;x++){
            const uint16_t base=playerSkinEq[y*320+x];
            const unsigned r=(base>>11)&31,g=(base>>5)&63,b=base&31;
            const bool greenTrack=g>24&&g>r*2&&g>b*2;
            if(sourceY>=65&&sourceY<=68&&greenTrack){
                const unsigned red=std::min(31u,(unsigned(tint[0])*31*g+5355u)/(255u*42u));
                const unsigned green=std::min(63u,(unsigned(tint[1])*63*g+5355u)/(255u*42u));
                const unsigned blue=std::min(31u,(unsigned(tint[2])*31*g+5355u)/(255u*42u));
                row[x-x0]=uint16_t((red<<11)|(green<<5)|blue);
            }else if(sourceY==69&&greenTrack){
                const unsigned gray=(g*31u+31u)/63u;
                row[x-x0]=uint16_t((gray<<11)|(g<<5)|gray);
            }else row[x-x0]=base;
        }
        lcd.drawRGBBitmap(x0,y,row,x1-x0,1);
    }
}
static void drawPlayerVolumeThumb(int x){
    // Apply the same inverse crop mapping as the baked balance thumb.
    // Display rows 68/69 both use source row 68; otherwise the bottom is 1 px high.
    for(int y=winY(60);y<winY(76);++y){
        const int sourceY=2+(y*233)/240;
        lcd.drawRGBBitmap(x,y,playerSkinSliderThumb+(sourceY-60)*14,14,1);
    }
}
// Twelve measured frequency bands, computed on the UI core at <=10 Hz.
// The audio core only publishes one in eight DAC PCM samples and never waits.
static void playerSpectrum(uint8_t* bars,unsigned count=12){
    if(count!=12&&count!=24&&count!=48)return;
    const uint32_t end=vuprobe::dacPcmWrite.load(std::memory_order_acquire);
    if(end<128)return;
    int16_t samples[128];
    for(unsigned i=0;i<128;i++)
        samples[i]=vuprobe::dacPcm[(end-128+i)&127u].load(std::memory_order_relaxed);
    static const uint8_t bins[12]={3,4,5,7,9,12,16,21,27,35,45,58};
    static const uint8_t denseBins[24]={2,3,4,5,6,7,8,9,10,12,14,16,18,20,23,26,29,33,37,41,46,51,56,62};
    auto& coefficients=uiCache->coefficients;
    static unsigned coefficientCount=0;
    if(coefficientCount!=count){
        for(unsigned band=0;band<count;band++)
            coefficients[band]=2.0f*cosf(6.2831853f*float(count==48?(2+band*60/47):count==24?denseBins[band]:bins[band])/128.0f);
        coefficientCount=count;
    }
    for(unsigned band=0;band<count;band++){
        const float coeff=coefficients[band];
        float q1=0,q2=0;
        for(unsigned i=0;i<128;i++){
            const float q0=float(samples[i])+coeff*q1-q2;
            q2=q1;q1=q0;
        }
        const float power=q1*q1+q2*q2-coeff*q1*q2;
        const float magnitude=sqrtf(power>0?power:0)/128.0f;
        float threshold=45.0f;
        while(bars[band]<16&&magnitude>threshold){bars[band]++;threshold*=1.35f;}
    }
}
static void playerMetalButton(int x,int y,int w,int h,const char* label){
    // Reuse the actual skin pixels: bevel, metallic shading and indicator.
    for(int row=0;row<h;++row)
        lcd.drawRGBBitmap(x,y+row,playerSkinEq+(y+row)*320+x,w,1);
    if(strcmp(label,"BT-OUT")==0)return; // original lettering already matches
    // Replace only the old EQ OFF lettering, using its blank face column.
    for(int row=3;row<h-3;++row)
        lcd.drawFastHLine(x+15,y+row,w-20,playerSkinEq[(y+row)*320+x+14]);
    // Compact transparent pixel lettering preserves the metal shading.
    static const char letters[]="BT-OUMEN";
    static const uint8_t glyphs[][5]={
        {127,73,73,73,54},{1,1,127,1,1},{8,8,8,8,8},
        {62,65,65,65,62},{63,64,64,64,63},{127,2,12,2,127},
        {127,73,73,73,65},{127,4,8,16,127}};
    int len=strlen(label), px=x+15+(w-20-(len*6-1))/2;
    for(const char* p=label;*p;++p,px+=6){
        const char* found=strchr(letters,*p);if(!found)continue;
        for(int col=0;col<5;++col)for(int row=0;row<7;++row)
            if(glyphs[found-letters][col]&(1<<row))lcd.fillRect(px+col,y+(h-7)/2+row,1,1,0x2104);
    }
}
static void playerEqPanel(const dual::Snapshot& s,bool layout){
    static eq10::Settings previous{};
    static bool known=false;
    const auto& eq=s.settings.graphicEq;
    if(!layout&&known&&memcmp(&previous,&eq,sizeof(eq))==0)return;
    previous=eq;known=true;
    // Restore the real Winamp metal panel and its printed scale/labels.
    // The previous flat overlay discarded the original finish and placed
    // several faders progressively farther away from their gold tracks.
    for(int y=155;y<225;++y)
        lcd.drawRGBBitmap(76,y,playerSkinEq+y*320+76,231,1);
    static constexpr uint16_t bandX[eq10::Bands]={88,111,133,155,179,201,225,247,271,294};
    for(unsigned i=0;i<eq10::Bands;++i){
        const int x=bandX[i];
        // The baked example knobs are at different heights. Rebuild each
        // gold shaft from an unobstructed shaft in the SAME source image.
        // At row 185 all example knobs overlap, so use the adjacent clean
        // row; this is below the visible fader and avoids a horizontal scar.
        uint16_t row[17];
        for(int y=159;y<218;++y){
            const int donorX=y<=185?179:88;
            const int donorY=y==185?184:y;
            for(int dx=-8;dx<=8;++dx)
                row[dx+8]=playerSkinEq[donorY*320+donorX+dx];
            lcd.drawRGBBitmap(x-8,y,row,17,1);
        }
        const int top=200-(constrain(int(eq.db[i]),-12,6)+12)*35/18;
        // Reuse the original 15x13 silver slider, including its bevel and
        // shadow, exactly as the volume control reuses its source pixels.
        for(int dy=0;dy<13;++dy)
            lcd.drawRGBBitmap(x-7,top+dy,playerSkinEq+(173+dy)*320+81,15,1);
    }
    // The curve is the only newly drawn part; use the original yellow and
    // dark panel palette, with a complete opaque well to avoid old traces.
    lcd.fillRect(94,137,153,16,0x3186);
    for(unsigned i=0;i<eq10::Bands;++i)
        lcd.drawFastVLine(97+16*int(i),138,14,0x630c);
    lcd.drawFastHLine(94,145,153,0x4208);
    for(unsigned i=1;i<eq10::Bands;++i){
        const int x0=97+16*int(i-1),x1=97+16*int(i);
        const int y0=constrain(145-eq.db[i-1]*5/12,139,151);
        const int y1=constrain(145-eq.db[i]*5/12,139,151);
        lcd.drawLine(x0,y0+1,x1,y1+1,0x7bc0);
        lcd.drawLine(x0,y0,x1,y1,eq.enabled?0xffc0:0x8410);
    }
    // Keep the original button bezel. Only its inset lettering changes.
    for(int y=138;y<151;++y)
        lcd.drawRGBBitmap(10,y,playerSkinEq+y*320+10,28,1);
    // Restore the full lettering well, retaining the source bevel/gradient.
    for(int y=139;y<150;++y){
        const uint16_t face=y<143?0xce79:y<148?0xad75:0x8410;
        lcd.drawFastHLine(11,y,26,face);
    }
    static const uint8_t glyph[][5]={{62,65,65,65,62},{127,2,4,8,127},{127,9,9,9,1}}; // O N F
    const int letters[3]={0,eq.enabled?1:2,2};
    for(int i=0;i<(eq.enabled?2:3);++i)for(int x=0;x<5;++x)for(int y=0;y<7;++y)
        if(glyph[letters[i]][x]&(1<<y))lcd.drawPixel(20+i*6+x,141+y,0x1082);
    lcd.fillRect(13,141,4,5,eq.enabled?0x07e0:0x4208);
}
// Each changing field owns a complete rectangle: no overdraw trails and no
// full-screen framebuffer. The static RGB565 background lives in flash.
// Same Unicode font as the menus, with transparent ink on native skin faces.
// Metadata uses the native 16px glyphs; compact labels use the same family.
static void mediaInk(int x,int y,int width,int height,const char* value,uint16_t color,int numerator=3){
    if(numerator==2){
        // Native small glyphs: decimating font16 discarded entire thin strokes.
        const auto& small=btfonts::font8;const char* cursor=value;const int left=x;
        while(*cursor){
            const auto& g=fonts::find(small,dual::nextUtf8(cursor));
            if(x+g.advance>left+width||g.h>height)break;
            for(int yy=0;yy<g.h;++yy)for(int xx=0;xx<g.w;++xx){
                const int bit=yy*g.w+xx,px=x+g.x+xx;
                if(px>=left&&px<left+width&&(small.data[g.offset+bit/8]&(0x80>>(bit&7))))lcd.drawPixel(px,y+yy,color);
            }
            x+=g.advance;
        }
        return;
    }
    const auto& font=fonts::font16;const char* p=value;
    const int left=x,top=y,lineHeight=numerator==4?19:numerator==3?13:10;
    while(*p){
        const auto& g=fonts::find(font,dual::nextUtf8(p));
        const int w=(g.w*numerator+3)/4,h=(min(int(g.h),19)*numerator+3)/4;
        const int advance=max(1,(g.advance*numerator+3)/4);
        if(x+advance>left+width){x=left;y+=lineHeight;}
        if(y+h>top+height)break;
        const uint8_t* bits=font.data+g.offset;
        for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx){
            const int sx=xx*4/numerator,sy=yy*4/numerator;
            const int bit=sy*g.w+sx;
            const int px=x+g.x*numerator/4+xx;
            if(sx<g.w&&sy<g.h&&px>=left&&px<left+width&&(bits[bit/8]&(0x80>>(bit&7))))
                lcd.drawPixel(px,y+yy,color);
        }
        x+=advance;
    }
}
static void mediaRestore(int x,int y,int w,int h){
    for(int row=y;row<y+h;++row)lcd.drawRGBBitmap(x,row,mediaPlayerBackground+row*320+x,w,1);
}
static void mediaField(unsigned id,int x,int y,int w,int h,const char* value,bool force,uint16_t color=0xffff){
    // Reuse the existing row cache instead of consuming scarce internal RAM.
    if(!force&&!strncmp(rows[id],value,sizeof(rows[id])-1))return;
    strlcpy(rows[id],value,sizeof(rows[id]));
    lcd.fillRect(x,y,w,h,0x0024);
    if(id<2)mediaInk(x+1,y+1,w-2,h-2,value,color,4);
    else textSmall(x+1,y+2,w-2,value,color,0x0024);
}
static void skinCover(int x,int y,int side){
    if(!coverVisible||!coverPixels||artW<1||artH<1)return;
    uint16_t row[88];side=std::min(side,88);
    for(int yy=0;yy<side;++yy){
        for(int xx=0;xx<side;++xx)row[xx]=coverPixels[(yy*artH/side)*112+xx*artW/side];
        lcd.drawRGBBitmap(x,y+yy,row,side,1);
        if((yy&15)==15)vTaskDelay(1);
    }
}
// Rebuild the lens inside the original bezel: never stretch a bitmap edge
// across the button (that repeats shadows and leaves a flat, blurred face).
static uint16_t mediaBlend(uint16_t a,uint16_t b,int n,int d){
    const int r=((a>>11)*(d-n)+(b>>11)*n)/d;
    const int g=(((a>>5)&63)*(d-n)+((b>>5)&63)*n)/d;
    const int bl=((a&31)*(d-n)+(b&31)*n)/d;
    return uint16_t((r<<11)|(g<<5)|bl);
}
static void mediaLens(int cx,int cy,int radius=12){
    mediaRestore(cx-radius-1,cy-radius-1,2*radius+3,2*radius+3);
    for(int dy=-radius;dy<=radius;++dy){
        const int half=int(sqrtf(float(radius*radius-dy*dy)));
        // Small top reflection, deep centre, brighter lower blue rim.
        const uint16_t c=dy<0?mediaBlend(0x65ff,0x0194,dy+radius,radius)
                              :mediaBlend(0x0194,0x03bf,dy,radius);
        lcd.drawFastHLine(cx-half,cy+dy,half*2+1,c);
    }
}
static void mediaPlate(int x,int width,const char* label){
    // One shared silver face for BT, output status and Menu.
    lcd.fillRoundRect(x,217,width,18,5,0x3a70);
    for(int row=1;row<17;++row){
        int inset=row<4?4-row:row>14?row-14:1;
        uint16_t c=row<8?mediaBlend(0xffff,0xc679,row-1,7)
                         :mediaBlend(0xc679,0x94d3,row-8,8);
        lcd.drawFastHLine(x+inset,217+row,width-2*inset,c);
    }
    lcd.drawRoundRect(x,217,width,18,5,0x4aef);
    const char* cursor=label;int labelWidth=0;
    while(*cursor)labelWidth+=max(1,int(fonts::find(btfonts::font8,dual::nextUtf8(cursor)).advance));
    const int inset=max(7,(width-labelWidth)/2);
    mediaInk(x+inset,221,width-inset-7,12,label,0x1082,2);
}
static void mediaPlayerHome(const dual::Snapshot& s,bool layout){
    if(!mediaPlayerBackground)return;
    static int lastVolume=-1,lastFill=-1,lastPlaying=-1,lastSource=-1;
    static int lastBt=-1;
    static uint32_t lastCover=UINT32_MAX;
    static bool coverEnabled=false;
    static uint32_t updatedAt=0;
    const bool bt=btaudio::wanted();
    if(!layout&&bt&&uint32_t(millis()-updatedAt)<1000)return;
    updatedAt=millis();
    if(layout){
        drawPlayer(mediaPlayerBackground);
        lastVolume=lastFill=lastPlaying=lastSource=lastBt=-1;
        // Demo artwork/text are never presented as real track metadata.
        lcd.fillRect(14,47,74,78,0x0024);
        lcd.drawRoundRect(23,56,56,56,6,0x039f);
        lcd.fillCircle(40,92,6,0x7dff);lcd.fillCircle(62,87,6,0x7dff);
        lcd.fillRect(44,68,3,25,0x7dff);lcd.fillRect(66,63,3,25,0x7dff);
        lcd.fillRect(45,64,23,4,0x7dff);
        lcd.fillRect(97,48,210,77,0x0024);
        // USB is a disabled hardware-dependent source, not a working tab.
        // Preserve the original tab bevel and lettering; mute only its dark ink.
        uint16_t usbRow[81];
        for(int y=26;y<38;++y){
            for(int x=228;x<309;++x){uint16_t c=mediaPlayerBackground[y*320+x];
                usbRow[x-228]=((c>>11)<=16&&((c>>5)&63)<=32&&(c&31)<=16)?0x8410:c;}
            lcd.drawRGBBitmap(228,y,usbRow,81,1);
        }
        // Keep the supplied button bevel; replace only its original legend.
        mediaPlate(5,102,"BT IN/OUT");
        mediaPlate(215,99,content::pick(s.settings.language,"Menü","Menu","Menü"));
        // The fifth control opens the actual playlist (no fake loop state).
        mediaLens(274,170);
        for(int y=164;y<=176;y+=6){lcd.fillRect(266,y,16,2,0xffff);}
    }
    if(layout||lastCover!=vintageCoverRevision||coverEnabled!=s.settings.artworkEnabled){
        coverEnabled=s.settings.artworkEnabled;
        // Neutral placeholder, never the baked example guitar photograph.
        lcd.fillRect(14,47,74,78,0x0024);
        lcd.drawRoundRect(23,56,56,56,6,0x039f);
        lcd.fillCircle(40,92,6,0x7dff);lcd.fillCircle(62,87,6,0x7dff);
        lcd.fillRect(44,68,3,25,0x7dff);lcd.fillRect(66,63,3,25,0x7dff);
        lcd.fillRect(45,64,23,4,0x7dff);
        if(s.settings.artworkEnabled&&coverVisible)skinCover(18,49,68);
        lastCover=vintageCoverRevision;
    }
    mediaField(0,98,47,208,21,s.station[0]?s.station:(s.sd?"SD":"I-BT-Radio"),layout);
    // Keep title static until metadata changes. Truncate, never scroll in BT.
    mediaField(1,98,68,208,40,uiCache->trackLine,layout,0xbddf);
    char info[70];
    snprintf(info,sizeof(info),"%u kbps  %u.%u kHz",unsigned(s.bitrate),unsigned(s.sampleRate/1000),unsigned((s.sampleRate%1000)/100));
    mediaField(2,98,110,151,14,info,layout,0xbddf);
    // A BT mode change must erase the previous DAC clock as well.
    if(bt)info[0]=0;
    else snprintf(info,sizeof(info),"%lu:%02lu",(unsigned long)(s.elapsed/60),(unsigned long)(s.elapsed%60));
    mediaField(3,264,110,43,14,info,layout,0xbddf);
    btaudio::Device device={};
    const char* peer=btaudio::connectedDevice(device)?device.name:(bt?"BT...":"DAC");
    if(layout||strncmp(rows[4],peer,sizeof(rows[4])-1)){
        strlcpy(rows[4],peer,sizeof(rows[4]));mediaPlate(111,99,peer);
    }
    const int volume=constrain((int(s.volume)*100+127)/254,0,100);
    if(layout||volume!=lastVolume){
        // Reuse the untouched right-hand panel gradient on each scanline.
        for(int y=194;y<211;++y)lcd.drawFastHLine(36,y,199,mediaPlayerBackground[y*320+240]);
        lcd.fillRoundRect(40,198,136,8,3,0x10a5);
        lcd.fillRoundRect(40,198,136*volume/100,8,3,0x039f);
        const int x=40+130*volume/100;
        lcd.fillRoundRect(x,195,6,14,2,0xef7d);lcd.drawRoundRect(x,195,6,14,2,0x8410);
        snprintf(info,sizeof(info),"%d%%",volume);mediaInk(184,198,46,12,info,0x1084,2);
        lastVolume=volume;
    }
    const int fill=s.bufferCapacity?constrain(int(uint64_t(s.buffer)*100/s.bufferCapacity),0,100):0;
    if(!bt&&(layout||fill!=lastFill||lastBt!=0)){
        lcd.fillRoundRect(12,133,296,9,3,0x10a5);
        if(fill)lcd.fillRoundRect(13,134,294*fill/100,7,2,0x039f);
        lastFill=fill;
    }else if(bt&&(layout||lastBt!=1)){lcd.fillRoundRect(12,133,296,9,3,0x10a5);}
    lastBt=int(bt);
    if(layout||int(s.playing)!=lastPlaying){
        mediaLens(105,170,16);
        if(s.playing){lcd.fillRect(98,163,4,15,0xffff);lcd.fillRect(108,163,4,15,0xffff);}
        else lcd.fillTriangle(100,162,100,179,114,170,0xffff);
        lastPlaying=int(s.playing);
    }
    if(layout||int(s.sd)!=lastSource){
        // All three tabs share one geometry/font; no baked USB label remains.
        mediaRestore(5,23,310,18);
        for(int tab=0;tab<3;++tab){
            const int x=8+tab*102,width=100;
            const bool active=tab<2&&bool(tab)==s.sd;
            lcd.fillRoundRect(x,23,width,18,5,active?0x0194:0x94d3);
            for(int row=1;row<17;++row){
                const int inset=row<4?4-row:1;
                const uint16_t c=active?mediaBlend(0x4dff,0x0194,row,17):mediaBlend(0xef7d,0x94d3,row,17);
                lcd.drawFastHLine(x+inset,23+row,width-2*inset,c);
            }
            lcd.drawRoundRect(x,23,width,18,5,0x4aef);
            const char* label=tab==0?content::pick(s.settings.language,"Rádió","Radio","Radio"):tab==1?"SD":"USB";
            const char* cursor=label;int length=0;
            while(*cursor)length+=fonts::find(btfonts::font8,dual::nextUtf8(cursor)).advance;
            mediaInk(x+(width-length)/2,26,width-8,11,label,active?0xffff:tab==2?0x630c:0x1082,2);
        }
        lastSource=int(s.sd);
    }
}
static void playerHome(const dual::Snapshot& s,bool layout){
    auto& oldStation=uiCache->oldStation;auto& oldTime=uiCache->oldTime;auto& oldBitrate=uiCache->oldBitrate;auto& oldRate=uiCache->oldRate;
    static int oldVolume=-1,oldBuffer=-1,oldPlaying=-1;
    static int oldSignalMode=-1,oldSignalBars=-1,oldSignalEnabled=-1;
    if(layout){drawPlayer(playerSkinEq);oldStation[0]=oldTime[0]=oldBitrate[0]=oldRate[0]=0;
        oldVolume=oldBuffer=oldPlaying=-1;
        // The former BT-OUT face now opens the common IN/OUT chooser.
        // The disabled-EQ badge is now a direct menu shortcut. Paint once
        // after the bitmap so this does not add redraw work during playback.
        // These are final bitmap coordinates, matching the touch rectangle.
        playerMetalButton(185,89,62,20,"BT");
        playerMetalButton(259,136,51,16,"MENU");
    }
    // The player art is cropped offline; dynamic fields use the same transform.
    const char* stationTitle=uiCache->trackLine;
    playerField(oldStation,sizeof(oldStation),winX(129),winY(25),winW(129,176),winH(25,14),stationTitle,0x07e0,layout);
    char value[30];
    snprintf(value,sizeof(value),"%lu:%02lu",(unsigned long)(s.elapsed/60),(unsigned long)(s.elapsed%60));
    const bool counterChanged=layout||strcmp(oldTime,value)!=0;
    // Signal now owns the former Bluetooth logo below PL, not the clock.
    const bool btMode=btaudio::inputSelected()||btaudio::outputSelected();
    const unsigned signal=btMode?btaudio::signalBars():ibtsignal::wifiBars(s.connected,s.rssi);
    const int signalMode=btMode?1:0,signalEnabled=int(s.settings.wifiIndicator);
    const bool signalChanged=oldSignalMode!=signalMode||oldSignalBars!=int(signal)||oldSignalEnabled!=signalEnabled;
    // Stop before the original analyzer border. Restore skin pixels instead
    // of painting a black rectangle across the separator after the counter.
    if(counterChanged){
        playerNumber(oldTime,sizeof(oldTime),winX(44),winY(25),winW(44,75),winH(25,19),
                     value,2,true);
    }
    oldSignalMode=signalMode;oldSignalBars=int(signal);oldSignalEnabled=signalEnabled;
    // Keep the original white kbps/kHz labels; only numbers are dynamic.
    snprintf(value,sizeof(value),"%u",unsigned(s.bitrate));
    playerNumber(oldBitrate,sizeof(oldBitrate),winX(129),winY(44),winW(129,21),winH(44,11),value,1,layout);
    if(s.sampleRate)snprintf(value,sizeof(value),"%u",unsigned((s.sampleRate+500)/1000));
    else value[0]=0;
    playerNumber(oldRate,sizeof(oldRate),winX(183),winY(44),winW(183,17),winH(44,11),value,1,layout);
    const int volume=(int(s.volume)*100+127)/254;
    if(layout||volume!=oldVolume){
        restorePlayerVolume(volume);
        const int thumb=winX(126+60*volume/100);
        drawPlayerVolumeThumb(thumb);
        oldVolume=volume;
    }
    const int fill=s.bufferCapacity?int((uint64_t(s.buffer)*100)/s.bufferCapacity):0;
    const int bounded=fill<0?0:fill>100?100:fill;
    if(layout||bounded!=oldBuffer){
        // The long separator is a live compressed-input buffer, not a timer.
        lcd.fillRect(winX(13),winY(77),winW(13,294),winH(77,4),0x2144);
        lcd.fillRect(winX(13),winY(77),winW(13,294)*bounded/100,3,0xc5e7);
        oldBuffer=bounded;
    }
    if(layout||int(s.playing)!=oldPlaying){
        lcd.fillRect(winX(24),winY(28),winW(24,13),winH(28,12),0x0020);
        if(s.playing)lcd.fillTriangle(winX(26),winY(29),winX(26),winY(38),winX(34),winY(33),0x07e0);
        else lcd.fillRect(winX(27),winY(31),winW(27,7),winH(31,7),0xa244);
        oldPlaying=int(s.playing);
    }
    // Visual spectrum: do not analyze a hidden status well or repaint all bands.
    static uint8_t oldBands[12]={};
    const bool statusWell=s.settings.batteryIndicator;
    if(!statusWell){
        uint8_t bands[12]={};
        if(s.playing&&vuprobe::dacEnabled.load(std::memory_order_relaxed))playerSpectrum(bands);
        if(layout)lcd.fillRect(winX(15),winY(45),winW(15,99),winH(45,25),0x0020);
        for(unsigned band=0;band<12;band++){
            if(!layout&&oldBands[band]==bands[band])continue;
            const int x=winX(15+band*8),w=winW(15+band*8,8);
            lcd.fillRect(x,winY(45),w,winH(45,25),0x0020);
            for(unsigned h=0;h<bands[band];h+=3){
                const uint16_t color=h<10?0x07e0:h<14?0xffe0:0xf800;
                lcd.fillRect(x,winY(66-h),winW(15+band*8,6),2,color);
            }
            oldBands[band]=bands[band];
        }
    }
    static int oldWifi=-1;
    if(layout||signalChanged||oldWifi!=int(signal)){
        // Dedicated former Bluetooth-logo slot below PL, in final bitmap
        // coordinates. Restore every row before drawing fewer/off bars.
        // Column 289 is the adjacent plain panel texture, outside the logo.
        for(int y=87;y<110;++y)
            lcd.drawFastHLine(290,y,23,playerSkinEq[y*320+289]);
        if(signalEnabled){
            const uint16_t color=signal<=1?0xf800:signal<=2?0xffe0:0x07e0;
            for(unsigned b=0;b<4;++b){
                const int height=3+int(b)*4;
                lcd.fillRect(292+b*5,106-height,3,height,b<signal?color:0x4208);
            }
        }
        oldWifi=int(signal);
    }
    playerEqPanel(s,layout);
}


static uint16_t vintageAccent(uint8_t style,uint8_t retroChoice){return style==6?videotono::green:style==5?0x9eff:style==4?0x07ff:style==1?retrocolor::accent(retroChoice):style==2?0xfd20:0xfda8;}
static uint32_t vintageTextHash(const char* value){
    uint32_t hash=2166136261u;
    for(const unsigned char* p=(const unsigned char*)value;*p;++p)hash=(hash^*p)*16777619u;
    return hash;
}
static uint16_t vintagePixel(uint8_t style,uint8_t color,int x,int y){
    if(style==6){const auto* p=packedpixels::pixels(videotono::playerAsset);return p?p[y*320+x]:0;}
    if(style==5)return weatherface::background(x,y);
    const uint16_t* plate=style==4?neonBackground:skinPixels(3+style);
    if(!plate)return 0;
    const uint16_t pixel=plate[y*320+x];
    return style==1?retrocolor::pixel(pixel,color):pixel;
}
static void vintageRestore(uint8_t style,uint8_t color,int x,int y,int w,int h){
    x=constrain(x,0,319);y=constrain(y,0,239);
    w=std::min(w,320-x);h=std::min(h,240-y);
    uint16_t* row=pixelScratch();if(!row)return;
    for(int yy=0;yy<h;++yy){
        for(int xx=0;xx<w;++xx)row[xx]=vintagePixel(style,color,x+xx,y+yy);
        lcd.drawRGBBitmap(x,y+yy,row,w,1);
        if((yy&15)==15)vTaskDelay(1);
    }
}
// Draw glyphs over the original textured pixels, without rectangular color blocks.
static void vintageLabel(uint8_t style,uint8_t choice,int x,int y,int maxWidth,
                         const char* value,uint16_t ink,bool small){
    const fonts::Font& font=fonts::font16;
    const char* p=value;const int start=x;
    uint16_t* glyph=pixelScratch();if(!glyph)return;
    while(*p){
        const auto& g=fonts::find(font,dual::nextUtf8(p));
        const int w=small?(int(g.w)+1)/2:int(g.w),h=small?(int(g.h)+1)/2:int(g.h);
        if(w<1||h<1||w>32||h>24||x-start+w>maxWidth||x+w>320||y+h>240)break;
        const uint8_t* bits=font.data+g.offset;
        const int gx=x+(small?int(g.x)/2:int(g.x));
        for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx){
            bool on=false;
            const int scale=small?2:1;
            for(int dy=0;dy<scale;++dy)for(int dx=0;dx<scale;++dx){
                const int sx=xx*scale+dx,sy=yy*scale+dy;
                if(sx<int(g.w)&&sy<int(g.h)){
                    const int bit=sy*int(g.w)+sx;
                    on|=(bits[bit/8]&(0x80>>(bit&7)))!=0;
                }
            }
            glyph[yy*w+xx]=on?ink:vintagePixel(style,choice,gx+xx,y+yy);
        }
        lcd.drawRGBBitmap(gx,y,glyph,w,h);
        x+=small?(int(g.advance)+1)/2:int(g.advance);
    }
}
static void vintageMenuFrame(const char* title,uint8_t style,uint8_t retroChoice){
    if(style==6){
        bg=videotono::glass;fg=accent=videotono::green;muted=0x430c;
        lcd.fillScreen(0xbdf7);lcd.fillRect(6,30,308,205,bg);lcd.drawRect(5,29,310,207,0x39c7);
        textSmall(12,34,290,title,fg,bg);return;
    }

    bg=style==5?0x0863:style==4?0x0021:style==1?0x0040:style==2?0x1082:0x3104;
    accent=vintageAccent(style,retroChoice);
    fg=style==1?accent:0xeed8;
    muted=style==1?retrocolor::dim(accent):0x9ccb;
    // The menu has one clean frame. Drawing the player bitmap first and
    // covering it with an opaque panel created seams and needless SPI work.
    
    lcd.drawRoundRect(5,5,310,230,6,accent);
    if(style==4)lcd.drawRoundRect(2,2,316,236,7,0xf81f);
    lcd.drawFastHLine(10,49,300,muted);
    textSmall(15,9,217,title,fg,bg);
}

// Neon uses the generated static plate, with live fields restored independently.
static __attribute__((noinline)) void neonHome(const dual::Snapshot& s,bool layout){
    if(!neonBackground)return;
    static bool artworkOn=false;
    static uint32_t station=0,title=0,cover=UINT32_MAX;static int vol=-1,fill=-1,playing=-1,minute=-2;
    auto& previousBands=uiCache->previousBands;static uint32_t meterAt=0;
    if(layout){drawPlayer(neonBackground);station=title=0;vol=fill=playing=-1;minute=-2;memset(previousBands,255,sizeof(previousBands));}
    const uint32_t sh=vintageTextHash(s.station),th=vintageTextHash(uiCache->trackLine);
    if(layout||sh!=station||th!=title||cover!=vintageCoverRevision||artworkOn!=s.settings.artworkEnabled){
        artworkOn=s.settings.artworkEnabled;
        vintageRestore(4,0,22,52,275,58);
        const bool hasCover=s.settings.artworkEnabled&&coverVisible;
        if(hasCover)skinCover(26,54,52);
        const int tx=hasCover?85:27,tw=hasCover?210:265;
        vintageLabel(4,0,tx,58,tw,s.station[0]?s.station:(s.sd?"SD":"I-BT RADIO"),0x07ff,hasCover);
        vintageLabel(4,0,tx,84,tw,uiCache->trackLine,0xf81f,true);
        cover=vintageCoverRevision;
        station=sh;title=th;
    }
    time_t stamp=time(nullptr);tm t{};const bool validTime=stamp>1704067200&&localtime_r(&stamp,&t);
    int m=validTime?t.tm_hour*60+t.tm_min:-1;
    if(layout||m!=minute){
        char value[12];if(validTime)snprintf(value,sizeof(value),"%02d:%02d",t.tm_hour,t.tm_min);else snprintf(value,sizeof(value),"--:--");
        vintageRestore(4,0,263,23,33,9);vintageLabel(4,0,264,23,32,value,0x07ff,true);minute=m;
    }
    // Same measured PCM frequency analysis as Winamp, on the UI task only.
    //48 measured bins interpolated to96 display columns; no doubled analysis cost.
    if(layout||uint32_t(millis()-meterAt)>=50){
        meterAt=millis();uint8_t measured[48]={},bands[96]={};
        if(s.playing&&vuprobe::dacEnabled.load(std::memory_order_relaxed))playerSpectrum(measured,48);
        for(unsigned i=0;i<96;++i)bands[i]=uiinteraction::spectrum(i,measured);
        for(int band=0;band<96;++band){
            if(!layout&&bands[band]==previousBands[band])continue;
            const int x=27+band*264/96;
            const int cellWidth=(band+1)*264/96-band*264/96;
            const int height=int(bands[band])*53/16;
            const int oldHeight=previousBands[band]==255?0:int(previousBands[band])*53/16;
            // Compose the changed vertical span, preserving grid and one-pixel gap.
            // One synchronous bitmap transfer instead of 44 row restores plus bars.
            const int first=layout?0:std::min(height,oldHeight);
            const int last=layout?54:std::max(height,oldHeight);
            const int top=162-last, rows=last-first;
            if(rows>0){
                uint16_t pixels[3*54];
                for(int yy=0;yy<rows;++yy){
                    const int screenY=top+yy, level=161-screenY;
                    for(int xx=0;xx<cellWidth;++xx){
                        uint16_t color=neonBackground[screenY*320+x+xx];
                        if(xx<cellWidth-1&&level>=0&&level<height&&level%4<2)
                            color=level<27?0x07ff:0xf81f;
                        pixels[yy*cellWidth+xx]=color;
                    }
                }
                lcd.drawRGBBitmap(x,top,pixels,cellWidth,rows);
            }
            previousBands[band]=bands[band];
        }
    }
    const int b=s.bufferCapacity?constrain(int(uint64_t(s.buffer)*100/s.bufferCapacity),0,100):0;
    if(layout||b!=fill){vintageRestore(4,0,25,170,270,5);if(b)lcd.fillRect(26,172,268*b/100,1,0xf81f);fill=b;}
    const int v=(int(s.volume)*100+127)/254;
    if(layout||v!=vol){
        vintageRestore(4,0,240,180,55,30);char value[12];snprintf(value,sizeof(value),"%d%%",v);
        vintageLabel(4,0,250,182,40,value,0x07ff,true);
        // Native neon segmented volume: restore the complete old footprint first.
        lcd.drawRect(240,193,55,15,0x8010);
        const int lit=constrain((v*13+99)/100,0,13);
        for(int i=0;i<13;++i){
            lcd.fillRect(242+i*4,195,3,11,i<lit?0x0410:0x0841);
            if(i<lit)lcd.fillRect(242+i*4,195,2,11,0x07ff);
        }
        vol=v;
    }
    if(layout||playing!=int(s.playing)){
        // Inner key face has a dark grid; restore its neutral neighbouring row
        // before drawing either transport state, so the baked pause cannot leak.
        for(int y=192;y<210;++y)for(int x=137;x<154;++x)lcd.drawPixel(x,y,neonBackground[188*320+x]);
        if(s.playing){lcd.fillRect(141,195,3,11,0x07ff);lcd.fillRect(148,195,3,11,0x07ff);}
        else lcd.fillTriangle(141,195,141,206,151,200,0x07ff);
        playing=int(s.playing);
    }
    if(layout){vintageLabel(4,0,25,199,73,"BT IN/OUT",0x07ff,true);}
}

static __attribute__((noinline)) void weatherHome(const dual::Snapshot& s,bool layout){
    const auto profile=weather::configuration();const auto outside=weather::reading();
    const time_t now=time(nullptr);tm local{};const bool clockValid=now>=1600000000&&localtime_r(&now,&local);
    const bool valid=profile.enabled&&weather::fresh(outside,millis());
    layout=weatherface::prepare(now,profile,outside.code,valid)||layout;
    if(layout){
        weatherface::restore(lcd,0,0,320,240);
        weatherface::text(lcd,9,3,240,"I-BT-Radio",16);
        for(int y=7;y<=17;y+=5)lcd.fillRoundRect(300,y,13,2,1,0xffff);
        lcd.drawFastHLine(0,166,320,0x738e);
        // All controls share an optical baseline; captions do not change icon placement.
    }
    auto field=[&](unsigned id,int x,int y,int width,int height,const char*value,unsigned size,uint16_t color=0xffff,bool center=false){
        if(layout||strcmp(rows[id],value)){weatherface::restore(lcd,x,y,width,height);weatherface::text(lcd,x,y,width,value,size,color,center);dual::textCopy(rows[id],sizeof(rows[id]),value);}
    };
    char value[192];
    if(clockValid)snprintf(value,sizeof(value),"%02u:%02u",unsigned(local.tm_hour),unsigned(local.tm_min));else strlcpy(value,"--:--",sizeof(value));
    field(0,8,27,145,55,value,48);
    if(clockValid)snprintf(value,sizeof(value),"%04u.%02u.%02u.",unsigned(local.tm_year+1900),unsigned(local.tm_mon+1),unsigned(local.tm_mday));else strlcpy(value,"Időszinkronra vár",sizeof(value));
    field(1,9,83,176,17,value,14);
    static unsigned oldSymbol=UINT32_MAX;const unsigned symbol=(valid?outside.code:255)+unsigned(weatherface::phase)*256;
    if(layout||oldSymbol!=symbol){weatherface::symbol(lcd,outside.code,valid);oldSymbol=symbol;}
    if(valid)snprintf(value,sizeof(value),"%s%d",outside.temperatureTenths<0?"-":"",abs(int(outside.temperatureTenths))/10);else strlcpy(value,"--",sizeof(value));
    if(layout||strcmp(rows[2],value)){
        weatherface::restore(lcd,240,27,77,40);
        const unsigned size=clockart::width(value,32)+15<=77?32:20;
        const int digits=clockart::width(value,size),left=240+(77-digits-15)/2;
        weatherface::text(lcd,left,29,digits,value,size);
        lcd.drawCircle(left+digits+4,33,2,0xffff);
        weatherface::text(lcd,left+digits+8,29,9,"C",14);
        strlcpy(rows[2],value,sizeof(rows[2]));
    }
    weatherface::fit(value,sizeof(value),profile.location,76,14);field(3,240,68,76,17,value,14,0xffff,true);
    field(4,244,85,72,16,valid?weatherface::condition(outside.code):profile.enabled?"Adatra vár":"API: ki",14,0xdedb);
    if(valid)snprintf(value,sizeof(value),"%u%%",outside.humidity);else strlcpy(value,"--%",sizeof(value));
    field(5,27,105,49,18,value,14);
    if(valid)snprintf(value,sizeof(value),"%u.%u km/h",outside.windTenths/10,outside.windTenths%10);else strlcpy(value,"-- km/h",sizeof(value));
    field(6,103,105,92,18,value,14);
    if(valid)snprintf(value,sizeof(value),"%u hPa",outside.pressure);else strlcpy(value,"-- hPa",sizeof(value));
    field(7,223,105,92,18,value,14);
    if(layout){
        lcd.fillCircle(15,115,4,0xd7ff);lcd.fillTriangle(11,114,19,114,15,106,0xd7ff);
        lcd.drawFastHLine(85,111,13,0xffff);lcd.drawFastHLine(82,116,15,0xffff);lcd.drawCircle(95,109,2,0xffff);
        lcd.drawCircle(210,113,7,0xffff);lcd.drawLine(210,113,214,108,0xffff);
    }
    radiosensor::Reading indoor{};
    const bool haveIndoor=sensors::enabled()&&sensors::reading(indoor);
    const bool tempValid=haveIndoor&&indoor.temperatureValid,humValid=haveIndoor&&indoor.humidityValid;
    const bool indoorFresh=tempValid&&humValid&&radiosensor::hasFreshPair(indoor,millis());
    const uint16_t indoorInk=indoorFresh?0xffff:0x9cf3;
    static uint16_t oldIndoorInk=0;if(oldIndoorInk!=indoorInk){rows[14][0]=rows[15][0]=0;oldIndoorInk=indoorInk;}
    // Separate fields, color pictograms, no age counter in the idle face.
    field(8,9,132,34,17,"Bent",14,0xc6db);
    if(tempValid)snprintf(value,sizeof(value),"%s%d.%u C",indoor.temperatureCenti<0?"-":"",abs(int(indoor.temperatureCenti))/100,unsigned(abs(int(indoor.temperatureCenti))/10%10));else strlcpy(value,"-- C",sizeof(value));
    field(14,63,132,86,18,value,14,indoorInk);
    if(humValid)snprintf(value,sizeof(value),"%u%%",unsigned(indoor.humidityCenti/100));else strlcpy(value,"--%",sizeof(value));
    field(15,185,132,60,18,value,14,indoorInk);
    if(layout){weatherface::thermometer(lcd,46,130);weatherface::droplet(lcd,167,132);}
    field(9,9,171,248,23,s.station[0]?s.station:s.sd?"SD-lejátszás":"Rádió",20);
    weatherface::fit(value,sizeof(value),uiCache->trackLine,302,14);field(10,9,196,302,17,value,14,0xe71c);
    field(13,267,173,49,15,btaudio::inputSelected()?"BT IN":btaudio::outputSelected()?"BT OUT":"DAC",14);
    snprintf(value,sizeof(value),"%u/%u",s.playing?1:0,unsigned(s.volume));
    if(layout||strcmp(rows[11],value)){
        weatherface::restore(lcd,0,216,320,24);
        weatherface::text(lcd,6,224,39,"BT",14,0x9eff);
        weatherface::transport(lcd,55,220,0);weatherface::transport(lcd,99,220,s.playing?1:3);weatherface::transport(lcd,143,220,2);
        lcd.drawFastVLine(180,218,20,0x9cf3);
        lcd.fillRect(191,225,3,8,0xffff);lcd.fillTriangle(194,225,200,220,200,237,0xffff);
        const unsigned volume=(unsigned(s.volume)*100u+127u)/254u;
        lcd.fillRoundRect(207,229,74,4,2,0x4a69);if(volume)lcd.fillRoundRect(207,229,74*volume/100,4,2,0x459f);
        char percent[8];snprintf(percent,sizeof(percent),"%u%%",volume);weatherface::text(lcd,285,224,34,percent,14);
        strlcpy(rows[11],value,sizeof(rows[11]));
    }
}

static btplayclock::Clock videotonBtClock;
void btClockTick(){
    static uint32_t checked=0;const uint32_t now=millis();
    if(uint32_t(now-checked)<200)return;checked=now;
    const bool selected=btaudio::inputSelected();
    if(!selected){videotonBtClock.tick(now,false,false,0);return;}
    const auto media=btmedia::view();
    videotonBtClock.tick(now,btaudio::connected(),btaudio::inputPlaying(),btplayclock::trackKey(media.title,media.artist));
}
static __attribute__((noinline)) void videotonHome(const dual::Snapshot& s,bool layout){
    const auto* plate=packedpixels::pixels(videotono::playerAsset);
    if(!plate){if(layout){lcd.fillScreen(0x0020);text(12,80,296,"Videoton: PSRAM cache",0x47f5);}return;}
    if(layout)drawPlayer(plate);
    auto field=[&](unsigned id,videotono::Rect r,const char* value,const videotono::Font& font,uint16_t color=videotono::green,bool center=false){
        if(layout||strcmp(rows[id],value)){videotono::field(lcd,plate,r,value,font,color,center);dual::textCopy(rows[id],sizeof(rows[id]),value);}
    };
    auto fit=[&](unsigned id,videotono::Rect r,const char* value,bool large){
        if(layout||strcmp(rows[id],value)){videotono::textFit(lcd,plate,r,value,large);dual::textCopy(rows[id],sizeof(rows[id]),value);}
    };
    const bool btIn=btaudio::inputSelected(),btOut=btaudio::outputSelected();
    field(0,{18,36,80,12},btIn?"BT IN":btOut?"BT OUT":"STEREO",videotono::font9);
    const char* title=s.title;const char* artist=s.artist;
    const bool btLinked=btIn&&btaudio::connected();
    const uint32_t btElapsed=videotonBtClock.seconds;
    fit(1,{14,52,256,26},btIn?(btLinked?"BLUETOOTH IN":btaudio::linkFailed()?"BT IN unavailable":"Bluetooth connecting"):s.station[0]?s.station:s.sd?"SD":"INTERNET RADIO",true);
    fit(2,{14,80,256,15},artist,false);fit(3,{14,96,256,23},title,false);
    char value[32];time_t now=time(nullptr);tm local{};
    if(now>1704067200&&localtime_r(&now,&local))snprintf(value,sizeof(value),"%02d:%02d",local.tm_hour,local.tm_min);else strlcpy(value,"--:--",sizeof(value));
    field(4,{267,35,38,14},value,videotono::infoFont(value,38));
    if(btIn)strlcpy(value,"--",sizeof(value));else snprintf(value,sizeof(value),"%u",unsigned(config.lastStation()));
    field(5,{272,69,34,28},value,videotono::presetFont(value),videotono::red,true);
    if(btIn)strlcpy(value,"BT IN",sizeof(value));else if(s.bitrate)snprintf(value,sizeof(value),"%u kbps",unsigned(s.bitrate));else strlcpy(value,"-- kbps",sizeof(value));
    field(6,{162,125,72,13},value,videotono::font9);
    const uint32_t elapsed=btIn?btElapsed:s.elapsed;
    snprintf(value,sizeof(value),"%lu:%02lu",(unsigned long)(elapsed/60),(unsigned long)(elapsed%60));field(7,{257,125,45,13},value,videotono::infoFont(value,45));
    const unsigned signal=s.settings.wifiIndicator?(btIn||btOut?btaudio::signalBars():s.sd?0:ibtsignal::wifiBars(s.connected,s.rssi)):0;
    snprintf(value,sizeof(value),"%u",signal);if(layout||strcmp(rows[8],value)){videotono::restore(lcd,plate,{61,125,77,13});for(unsigned i=0;i<signal*2;++i)lcd.fillRect(63+i*7,127,5,7,videotono::green);strlcpy(rows[8],value,sizeof(rows[8]));}
    const unsigned volume=(unsigned(s.volume)*100+127)/254;snprintf(value,sizeof(value),"%u%%",volume);
    if(layout||strcmp(rows[9],value)){videotono::field(lcd,plate,{246,202,42,8},value,videotono::font7,0x1082,true);videotono::level(lcd,plate,(volume+9)/10);strlcpy(rows[9],value,sizeof(rows[9]));}
    const bool running=btIn?btaudio::inputPlaying():s.playing;snprintf(value,sizeof(value),"%u",running);
    if(layout||strcmp(rows[10],value)){videotono::play(lcd,plate,running);strlcpy(rows[10],value,sizeof(rows[10]));}
    snprintf(value,sizeof(value),"%u",btIn||btOut?2:s.sd?1:0);
    if(layout||strcmp(rows[11],value)){videotono::sourceLeds(lcd,plate,unsigned(value[0]-'0'));strlcpy(rows[11],value,sizeof(rows[11]));}
}

static __attribute__((noinline)) void vintageHome(const dual::Snapshot& s,bool layout){
    const uint8_t style=s.settings.vintageSkin;
    // Videoton dispatches directly from render(), outside this large vintage frame.
    if(style==6)return;
    if(style<1||style>3)return;
    // Reuse the single UI-owner PSRAM scratch only between synchronous writes.
    // Optional-cache failure must leave the previous pixels and cache state intact.
    static_assert(sizeof(UiCache::glyphScratch)/sizeof(uint16_t)>=246*3,
                  "Vintage stripe must fit the existing PSRAM scratch");
    uint16_t* const pixels=pixelScratch();if(!pixels)return;
    static uint32_t oldStationHash=0,oldTitleHash=0,oldTimeHash=0;
    static int oldBuffer=-1,oldVolume=-1,oldPlaying=-1,oldSource=-1,oldMeterL=-1,oldMeterR=-1;
    static uint32_t oldCoverRevision=UINT32_MAX;
    struct Geometry {int textX,textY,textW,textH,coverX,coverY,coverW,coverH,meterY,playY;};
    static constexpr Geometry faces[]={
        {0,0,0,0,0,0,0,0,0,0},
        {79,100,227,53,14,100,59,53,162,214},
        {121,39,182,72,24,31,88,80,123,190},
        {132,39,120,62,70,39,56,62,123,198}
    };
    const Geometry& face=faces[style];
    const uint16_t ink=vintageAccent(style,s.settings.retroColor);
    fg=style==1?ink:0xeed8;
    if(layout){
        drawSkin(3+style,s.settings.retroColor);
        oldStationHash=oldTitleHash=oldTimeHash=0;
        oldBuffer=oldVolume=oldPlaying=oldSource=oldMeterL=oldMeterR=-1;
        // The optional battery readout owns this header slot. Do not draw
        // the brand beneath an opaque status badge.
        if(!s.settings.batteryIndicator)
            vintageLabel(style,s.settings.retroColor,12,8,71,"I-BT-Radio",ink,true);
        vintageLabel(style,s.settings.retroColor,259,8,51,"BT IN/OUT",ink,true);
        if(style==1){
            vintageLabel(style,s.settings.retroColor,31,210,91,"ELŐZŐ",ink,true);
            vintageLabel(style,s.settings.retroColor,198,210,64,"KÖVETK.",ink,true);
            vintageLabel(style,s.settings.retroColor,267,210,38,"MENÜ",ink,true);
        }
    }
    const bool redrawCover=layout||oldCoverRevision!=vintageCoverRevision;
    const uint32_t stationHash=vintageTextHash(s.station),titleHash=vintageTextHash(uiCache->trackLine);
    if(layout||oldStationHash!=stationHash||oldTitleHash!=titleHash||oldSource!=int(s.sd)||redrawCover){
        vintageRestore(style,s.settings.retroColor,face.textX,face.textY,face.textW,face.textH);
        vintageLabel(style,s.settings.retroColor,face.textX+5,face.textY+5,face.textW-10,
                     s.station[0]?s.station:(s.sd?"SD":"I-BT-Radio"),ink,style==3);
        vintageLabel(style,s.settings.retroColor,face.textX+5,face.textY+(style==1?29:31),face.textW-10,
                     uiCache->trackLine,fg,true);
        {
            vintageRestore(style,s.settings.retroColor,face.coverX,face.coverY,face.coverW,face.coverH);
            if(coverVisible&&coverPixels&&artW>0&&artH>0){
                const int side=std::min(face.coverW-4,face.coverH-4);
                for(int y=0;y<side;y++){
                    uint16_t* const row=pixels;
                    for(int x=0;x<side;x++)row[x]=coverPixels[(y*artH/side)*112+(x*artW/side)];
                    lcd.drawRGBBitmap(face.coverX+(face.coverW-side)/2,face.coverY+(face.coverH-side)/2+y,row,side,1);
                    if((y&15)==15)vTaskDelay(1);
                }
            }else vintageLabel(style,s.settings.retroColor,face.coverX+6,face.coverY+face.coverH/2-4,
                               face.coverW-12,btaudio::inputSelected()?"BT IN":s.sd?"SD":"RADIO",ink,true);
        }
        oldCoverRevision=vintageCoverRevision;
        oldStationHash=stationHash;oldTitleHash=titleHash;oldSource=int(s.sd);
    }
    char timeValue[20];snprintf(timeValue,sizeof(timeValue),"%lu:%02lu",(unsigned long)(s.elapsed/60),(unsigned long)(s.elapsed%60));
    const uint32_t timeHash=vintageTextHash(timeValue);
    if(layout||oldTimeHash!=timeHash){
        vintageRestore(style,s.settings.retroColor,176,7,67,11);
        vintageLabel(style,s.settings.retroColor,183,8,55,timeValue,ink,true);
        oldTimeHash=timeHash;
    }
    const int fill=s.bufferCapacity?constrain(int(uint64_t(s.buffer)*100/s.bufferCapacity),0,100):0;
    const int meterL=vuprobe::dacEnabled.load(std::memory_order_relaxed)?vuprobe::dacLeft.load(std::memory_order_relaxed)/3:0;
    const int meterR=vuprobe::dacEnabled.load(std::memory_order_relaxed)?vuprobe::dacRight.load(std::memory_order_relaxed)/3:0;
    if(layout){
        vintageRestore(style,s.settings.retroColor,18,face.meterY,284,style==3?35:27);
        if(style==2){lcd.fillRoundRect(21,125,278,23,3,0x0841);lcd.drawRoundRect(21,125,278,23,3,ink);}
    }
    for(int channel=0;channel<3;++channel){
        const int level=channel==2?fill:constrain(channel?meterR:meterL,0,85);
        const int previous=channel==2?oldBuffer:(channel?oldMeterR:oldMeterL);
        if(!layout&&level==previous)continue;
        const int y=channel==2?face.meterY+21:face.meterY+4+channel*8;
        for(int yy=0;yy<3;++yy)for(int xx=0;xx<246;++xx)
            pixels[yy*246+xx]=style==2?0x0841:vintagePixel(style,s.settings.retroColor,41+xx,y+yy);
        lcd.drawRGBBitmap(41,y,pixels,246,3);
        lcd.drawFastHLine(41,y+1,246,style==1?retrocolor::dim(ink):0x4208);
        if(level)lcd.fillRoundRect(41,y,246*level/(channel==2?100:85),3,1,ink);
    }
    oldBuffer=fill;oldMeterL=meterL;oldMeterR=meterR;
    const int volume=(int(s.volume)*100+127)/254;
    if(layout||oldVolume!=volume){
        vintageRestore(style,s.settings.retroColor,88,7,78,11);
        char value[20];snprintf(value,sizeof(value),"VOL %d%%",volume);
        vintageLabel(style,s.settings.retroColor,91,8,72,value,ink,true);oldVolume=volume;
    }
    if(layout||oldPlaying!=int(s.playing)){
        const int cx=159,cy=face.playY;
        // Keep the original engraved button, bezel and light intact. The
        // previous flat filled circle covered the source artwork.
        vintageRestore(style,s.settings.retroColor,cx-11,cy-12,23,25);
        if(style==1){
            if(s.playing){lcd.fillRect(cx-4,cy-6,2,12,ink);lcd.fillRect(cx+2,cy-6,2,12,ink);}
            else lcd.fillTriangle(cx-4,cy-6,cx-4,cy+6,cx+5,cy,ink);
        }else if(!s.playing){
            // The photograph already contains a pause glyph. Reconstruct
            // only its tiny inner disc from the neighbouring source pixels,
            // then engrave a play glyph; never paste a new button over it.
            uint16_t* const row=pixels;
            for(int yy=-10;yy<=10;++yy){
                const uint16_t middle=vintagePixel(style,s.settings.retroColor,cx,cy+yy);
                const uint16_t left=vintagePixel(style,s.settings.retroColor,cx-11,cy+yy);
                const uint16_t right=vintagePixel(style,s.settings.retroColor,cx+11,cy+yy);
                for(int xx=-9;xx<=9;++xx){
                    const uint16_t edge=xx<0?left:right;
                    const unsigned t=unsigned(abs(xx))*10u;
                    const unsigned r=((((middle>>11)&31u)*(100u-t)+((edge>>11)&31u)*t)+50u)/100u;
                    const unsigned g=((((middle>>5)&63u)*(100u-t)+((edge>>5)&63u)*t)+50u)/100u;
                    const unsigned b=(((middle&31u)*(100u-t)+(edge&31u)*t)+50u)/100u;
                    row[xx+9]=uint16_t((r<<11)|(g<<5)|b);
                }
                lcd.drawRGBBitmap(cx-9,cy+yy,row,19,1);
            }
            lcd.fillTriangle(cx-4,cy-7,cx-4,cy+7,cx+6,cy,ink);
        }
        oldPlaying=int(s.playing);
    }
}



static void mediaMenuFrame(const char* title){
    
    lcd.drawRoundRect(1,1,318,238,7,0xffff);
    lcd.drawRoundRect(3,3,314,234,6,0x8410);
    // The glyph helper paints an opaque background. A gradient beneath it
    // produced a separate solid rectangle around every letter.
    lcd.fillRect(6,5,308,20,0x0258);
    textSmall(12,8,220,title,0xffff,0x0258);
    lcd.drawFastHLine(7,27,306,0xffff);
}
static void skinMenuFrame(const char* title,bool cassette){
    const int w=lcd.width(),h=lcd.height();
    bg=cassette?0x2104:0x1083;fg=cassette?0xeedb:0x9fe0;
    accent=cassette?0xfcc8:0xffe0;muted=0x8410;
    
    lcd.drawRoundRect(1,1,w-2,h-2,4,cassette?0x9cd2:0xbdf7);
    lcd.drawRoundRect(3,3,w-6,h-6,3,0x4208);
    lcd.fillRect(5,5,w-10,19,cassette?0x528a:0x3186);
    textSmall(12,7,w-100,title,cassette?0xff9c:0xffff,cassette?0x528a:0x3186);
    lcd.drawFastHLine(6,h-7,w-12,accent);
}
static unsigned stationListViewportFirst=1;
unsigned stationListFirst(){return stationListViewportFirst;}
static void playerStationList(const dual::Snapshot& s,unsigned selected,bool layout){
    static unsigned oldSelected=0,oldTotal=0;
    static uint32_t oldGeneration=UINT32_MAX,paintedAt=0;
    static bool oldSd=false;
    const uint16_t total=config.playlistLength();
    const bool contentChanged=layout||total!=oldTotal||s.sd!=oldSd||s.generation!=oldGeneration;
    const bool frameChanged=layout||s.sd!=oldSd;
    if(!contentChanged&&selected==oldSelected)return;
    if(!layout&&uint32_t(millis()-paintedAt)<100)return;
    paintedAt=millis();
    const bool media=s.settings.mediaPlayerSkin;
    const bool kajonStyle=false;
    const uint8_t vintage=s.settings.vintageSkin;
    const uint16_t listBg=vintage?(vintage==6?videotono::glass:vintage==4?0x0021:vintage==1?0x0040:vintage==2?0x1082:0x3104):kajonStyle?0x0841:media?0x0024:0x0020;
    if(frameChanged){if(vintage)vintageMenuFrame(s.sd?"SD - Lista":"Rádió - Lista",vintage,s.settings.retroColor);else if(media)mediaMenuFrame(s.sd?"SD - Playlist":"Radio - Playlist");else drawPlayer(playerSkinPlaylist);}
    if(contentChanged){
        oldTotal=total;oldSd=s.sd;oldGeneration=s.generation;
        stationListViewportFirst=selected>3?selected-3:1;
        if(total>8&&stationListViewportFirst>total-7)stationListViewportFirst=total-7;
    }
    int selectedRow=-1,previousRow=-1;
    for(int row=0;row<8&&total;++row){
        unsigned n=stationListViewportFirst+unsigned(row);while(n>total)n-=total;
        if(n==selected)selectedRow=row;
        if(n==oldSelected)previousRow=row;
    }
    // Keep the viewport steady while the rotary moves inside it. A scroll or
    // source/list change still repaints the full viewport.
    const bool full=contentChanged||selectedRow<0||total<8;
    if(full){
        if(!contentChanged){
            stationListViewportFirst=selected>3?selected-3:1;
            if(total>8&&stationListViewportFirst>total-7)stationListViewportFirst=total-7;
        }
        if(vintage)lcd.fillRoundRect(14,33,289,169,4,listBg);
        else lcd.fillRect(winX(10),winY(24),winW(10,290),winH(24,178),listBg);
    }
    if(!total){
        lcd.fillRect(vintage?302:303,vintage?34:30,vintage?4:11,vintage?168:160,listBg);
        textSmall(vintage?18:winX(15),vintage?45:winY(40),vintage?275:winW(15,270),s.sd?"Nincs MP3/FLAC":"Nincs állomás",vintage?vintageAccent(vintage,s.settings.retroColor):0x9e29,listBg);
        oldSelected=selected;return;
    }
    for(int row=0;row<8&&row<int(total);row++){
        if(!full&&row!=previousRow&&row!=selectedRow)continue;
        unsigned n=stationListViewportFirst+unsigned(row);while(n>total)n-=total;
        const int y=vintage?35+row*21:25+row*22;
        if(!full){if(vintage)lcd.fillRect(16,y-2,285,20,listBg);else lcd.fillRect(winX(12),winY(y-1),winW(12,286),winH(y-1,22),listBg);}
        const uint16_t selectedBg=vintage?(vintage==1?retrocolor::dim(vintageAccent(vintage,s.settings.retroColor)):0x3965):kajonStyle?0x18c3:media?0x0258:0x4228;
        if(unsigned(n)==selected){if(vintage)lcd.fillRoundRect(16,y-2,285,20,3,selectedBg);else lcd.fillRect(winX(12),winY(y-1),winW(12,286),winH(y-1,22),selectedBg);}
        char label[190];
        const char* name=config.stationByNum(uint16_t(n));
        snprintf(label,sizeof(label),"%u. %s",unsigned(n),name?name:"");
        playerListText(vintage?19:winX(15),vintage?y:winY(y),vintage?278:winW(15,277),label,vintage?(unsigned(n)==selected?vintageAccent(vintage,s.settings.retroColor):0xdedb):kajonStyle?(unsigned(n)==selected?0x05ff:0xc638):media?(unsigned(n)==selected?0xffff:0xbddf):(unsigned(n)==selected?0xe6d6:0x9e29),
                  unsigned(n)==selected?selectedBg:listBg);
    }
    const int railX=vintage?302:303,railY=vintage?34:30,railH=vintage?168:160,railW=vintage?4:11;
    lcd.fillRect(railX,railY,railW,railH,listBg);
    const int thumbH=total>8?std::max(12,int(railH*8/total)):railH;
    const int thumbY=railY+(total>8?int((stationListViewportFirst-1)*(railH-thumbH)/(total-8)):0);
    lcd.fillRoundRect(railX,thumbY,railW,thumbH,2,vintage?vintageAccent(vintage,s.settings.retroColor):0xbdf7);
    oldSelected=selected;
}
static bool batteryDirty=true;
void invalidate(){menuSlices.cancel();valid=false;batteryDirty=true;}
static void wifiPasswordScreen(unsigned selected,bool layout,dual::Language lang,bool ntp=false){
    const auto& entry=ntp?dual::ntpServerUi():dual::wifiPasswordUi();
    const int w=lcd.width(),h=lcd.height();
    line(0,8,4,w-16,ntp?content::pick(lang,"Időszerver","Time server","Zeitserver"):content::pick(lang,"Wi-Fi jelszó","Wi-Fi password","WLAN-Passwort"),accent,layout);
    line(1,8,27,w-16,entry.ssid,fg,layout);
    const size_t count=strlen(entry.password);
    char masked[30]={};
    wifipasswordui::preview(masked,sizeof(masked),entry.password,entry.visible);
    char value[64];
    if(ntp)snprintf(value,sizeof(value),"%s%s",count>28?"...":"",entry.password+(count>28?count-28:0));
    else snprintf(value,sizeof(value),"%u: %s",unsigned(count),masked);
    line(2,8,50,ntp?w-16:w-106,value,accent,layout);
    const char* modes[]={"abc","ABC","#+="};
    if(entry.error[0])snprintf(value,sizeof(value),"%s",entry.error);
    else if(entry.submitted&&network.setupFailed())snprintf(value,sizeof(value),"Kapcsolat sikertelen: javítsd");
    else if(entry.submitted)snprintf(value,sizeof(value),"Kapcsolódás...");
    else snprintf(value,sizeof(value),"%s  %s",modes[entry.mode],ntp?content::pick(lang,"OK: mentés / X: mégse","OK: save / X: cancel","OK: speichern / X: zurück"):content::pick(lang,"OK: csatlakozás","OK: connect","OK: verbinden"));
    line(3,8,68,ntp?w-16:w-106,value,entry.error[0]||(entry.submitted&&network.setupFailed())?0xf800:muted,layout);
    static int previousKey=-1,previousMode=-1;static bool previousVisible=false;
    if(layout){previousKey=-1;previousMode=-1;}
    const int cellW=(w-16)/8,cellH=(h-99)/5;
    auto drawKey=[&](unsigned key){
        if(key==wifipasswordui::visibilityKey){
            if(ntp)return;
            const int x=w-90,y=46,kw=82,kh=36;
            const uint16_t color=selected==key?accent:muted;
            lcd.fillRect(x,y,kw,kh,bg);lcd.drawRoundRect(x,y,kw,kh,3,color);
            text(x+7,y+9,kw-14,entry.visible?"Elrejt":"Mutat",color);return;
        }
        const int x=8+int(key%8)*cellW,y=91+int(key/8)*cellH;
        const int kw=cellW-2,kh=cellH-2;
        lcd.fillRect(x,y,kw,kh,bg);
        const uint16_t color=selected==key?accent:muted;
        lcd.drawRoundRect(x,y,kw,kh,3,color);
        char label[5]={};
        if(key<36){const char c=dual::wifiPasswordKeyChar(entry.mode,key);if(c==' ')strlcpy(label,"SP",sizeof(label));else if(c)label[0]=c;}
        else strlcpy(label,key==36?"M":key==37?"<":key==38?"OK":"X",sizeof(label));
        const int labelX=x+(kw-(int(strlen(label))*9))/2;
        text(labelX< x+2?x+2:labelX,y+(kh-17)/2,kw-4,label,color);
    };
    if(layout||previousMode!=entry.mode){for(unsigned key=0;key<40;key++)drawKey(key);}
    else if(previousKey!=int(selected)){
        if(previousKey>=0)drawKey(unsigned(previousKey));
        drawKey(selected);
    }
    if(!ntp&&(layout||previousVisible!=entry.visible))drawKey(wifipasswordui::visibilityKey);
    previousVisible=entry.visible;previousKey=int(selected);previousMode=entry.mode;
}
// Static asset and decoded cover live exclusively in PSRAM; no large internal fallback.
static const uint16_t *cassettePixels=cassettePhoto;
static unsigned reelPhase=0;
// Two degrees per step; fixed-point lookup avoids trigonometry in pixel drawing.
static const int16_t reelCos[180]={
    256,256,255,255,254,252,250,248,246,243,241,237,234,230,226,
    222,217,212,207,202,196,190,184,178,171,165,158,150,143,136,
    128,120,112,104,96,88,79,71,62,53,44,36,27,18,9,
    0,-9,-18,-27,-36,-44,-53,-62,-71,-79,-88,-96,-104,-112,-120,
    -128,-136,-143,-150,-158,-165,-171,-178,-184,-190,-196,-202,-207,-212,-217,
    -222,-226,-230,-234,-237,-241,-243,-246,-248,-250,-252,-254,-255,-255,-256,
    -256,-256,-255,-255,-254,-252,-250,-248,-246,-243,-241,-237,-234,-230,-226,
    -222,-217,-212,-207,-202,-196,-190,-184,-178,-171,-165,-158,-150,-143,-136,
    -128,-120,-112,-104,-96,-88,-79,-71,-62,-53,-44,-36,-27,-18,-9,
    0,9,18,27,36,44,53,62,71,79,88,96,104,112,120,
    128,136,143,150,158,165,171,178,184,190,196,202,207,212,217,
    222,226,230,234,237,241,243,246,248,250,252,254,255,255,256
};
static const int16_t reelSin[180]={
    0,9,18,27,36,44,53,62,71,79,88,96,104,112,120,
    128,136,143,150,158,165,171,178,184,190,196,202,207,212,217,
    222,226,230,234,237,241,243,246,248,250,252,254,255,255,256,
    256,256,255,255,254,252,250,248,246,243,241,237,234,230,226,
    222,217,212,207,202,196,190,184,178,171,165,158,150,143,136,
    128,120,112,104,96,88,79,71,62,53,44,36,27,18,9,
    0,-9,-18,-27,-36,-44,-53,-62,-71,-79,-88,-96,-104,-112,-120,
    -128,-136,-143,-150,-158,-165,-171,-178,-184,-190,-196,-202,-207,-212,-217,
    -222,-226,-230,-234,-237,-241,-243,-246,-248,-250,-252,-254,-255,-255,-256,
    -256,-256,-255,-255,-254,-252,-250,-248,-246,-243,-241,-237,-234,-230,-226,
    -222,-217,-212,-207,-202,-196,-190,-184,-178,-171,-165,-158,-150,-143,-136,
    -128,-120,-112,-104,-96,-88,-79,-71,-62,-53,-44,-36,-27,-18,-9
};

static cassette::View cassetteView={0,13,320,213};
static uint16_t cassettePixel(int x,int y){
    if(!cassettePixels||x<0||x>=320||y<0||y>=213)return bg;
    if(coverVisible&&cassette::sticker(x,y)){
        // Cover fills the wide sticker: crop vertically, retaining the image aspect.
        int sx=(x-18)*artW/284;
        int sy=(y-21)*artW/284+(artH-(122*artW/284))/2;
        sx=constrain(sx,0,artW-1);sy=constrain(sy,0,artH-1);
        return coverPixels[sy*112+sx];
    }
    // Rotate only the hub interior; fixed body and label are never animated.
    if(reelPhase){
        for(int cx : {88,229}){int dx=x-cx,dy=y-91;
            if(dx*dx+dy*dy<=19*19){
                int sx=cx+(dx*reelCos[reelPhase]+dy*reelSin[reelPhase])/256;
                int sy=91+(-dx*reelSin[reelPhase]+dy*reelCos[reelPhase])/256;
                return cassettePixels[sy*320+sx];
            }
        }
    }
    return cassettePixels[y*320+x];
}
static void cassetteRect(int x,int y,int w,int h){
    uint16_t scan[320];const auto v=cassetteView;
    int left=v.x+x*v.w/320,right=v.x+(x+w)*v.w/320;
    int top=v.y+y*v.h/213,bottom=v.y+(y+h)*v.h/213;
    for(int dy=top;dy<bottom;dy++){
        for(int dx=left;dx<right;dx++)scan[dx-left]=cassettePixel((dx-v.x)*320/v.w,(dy-v.y)*213/v.h);
        lcd.drawRGBBitmap(left,dy,scan,right-left,1);
        if((dy&7)==0)vTaskDelay(1);
    }
}
static void cassetteLine(unsigned id,int x,int y,int w,const char* value,bool force){
    if(!force&&uiCache->widths[id]==w&&!strcmp(rows[id],value))return;
    cassetteRect(x,y,w,20);
    const char *p=value;int cursor=x;const auto& font=fonts::font16;
    while(*p){const auto& g=fonts::find(font,dual::nextUtf8(p));if(cursor+g.w>x+w)break;
        const uint8_t* bits=font.data+g.offset;
        for(int yy=0;yy<g.h;yy++)for(int xx=0;xx<g.w;xx++)if(bits[(yy*g.w+xx)/8]&(0x80>>((yy*g.w+xx)&7))){
            int px=cursor+g.x+xx,py=y+g.y+yy;
            const auto v=cassetteView;int dx=v.x+px*v.w/320,dy=v.y+py*v.h/213;
            // A shadow retains legibility on bright cover images without opaque glyph boxes.
            lcd.drawPixel(dx+1,dy+1,0x0000);lcd.drawPixel(dx,dy,coverVisible?0xffff:0x1082);
        }
        cursor+=g.advance;
    }
    dual::textCopy(rows[id],sizeof(rows[id]),value);uiCache->widths[id]=w;
}
bool cassetteReady(){return !connectionScreen&&!previousSetup&&!previousNightClock&&!gallery.active()&&cassetteRequested&&cassettePixels&&valid&&previousPage==0&&network.status!=SOFT_AP;}
static void cassetteHome(const dual::Snapshot& s,bool layout){
    if(!cassettePixels){line(1,8,40,304,s.station,fg,layout);line(2,8,66,304,uiCache->trackLine,fg,layout);line(3,8,104,304,"Kazettakep feltoltesre var",accent,layout);return;}
    cassetteView=cassette::view({lcd.width(),lcd.height()});
    static uint32_t lastReel=0;
    bool animate=s.settings.reelAnimation&&s.playing&&s.freeHeap>=32768&&s.buffer>=16384;
    bool hubChanged=false;
    if(!s.settings.reelAnimation&&reelPhase){reelPhase=0;hubChanged=true;}
    if(animate&&uint32_t(millis()-lastReel)>=50){lastReel=millis();reelPhase=(reelPhase+1)%180;hubChanged=true;}
    if(layout){
        // The cassette image is letterboxed; clear the uncovered margins on
        // entry so the previous skin/menu cannot survive below or above it.
        const auto v=cassetteView;
        if(v.y>0)lcd.fillRect(0,0,lcd.width(),v.y,bg);
        if(v.y+v.h<lcd.height())lcd.fillRect(0,v.y+v.h,lcd.width(),lcd.height()-v.y-v.h,bg);
        if(v.x>0)lcd.fillRect(0,v.y,v.x,v.h,bg);
        if(v.x+v.w<lcd.width())lcd.fillRect(v.x+v.w,v.y,lcd.width()-v.x-v.w,v.h,bg);
        cassetteRect(0,0,320,213);
    }
    else if(cassetteDirty)cassetteRect(18,21,284,122);
    if(hubChanged&&!layout){cassetteRect(68,71,40,40);cassetteRect(209,71,40,40);}
    bool labels=layout||cassetteDirty;cassetteDirty=false;
    cassetteLine(1,30,24,260,s.station,labels);
    cassetteLine(2,30,44,260,uiCache->trackLine,labels);
    char value[40];snprintf(value,sizeof(value),"%s %u%%",s.playing?"LIVE":"STOP",unsigned(s.volume)*100/254);
    cassetteLine(3,208,128,90,value,labels);
    // Only the centre window changes on pause; both mechanical reels remain untouched.
    static int previousPlaying=-1;
    if(labels||previousPlaying!=int(s.playing)){
        cassetteRect(144,80,32,32);auto v=cassetteView;
        int x=v.x+152*v.w/320,y=v.y+88*v.h/213;
        if(s.playing){lcd.fillRect(x,y,3,14,0xffff);lcd.fillRect(x+8,y,3,14,0xffff);}
        else lcd.fillTriangle(x,y,x,y+14,x+12,y+7,0xffff);
        previousPlaying=s.playing;
    }
    // Small source-buffer bar in the unused margin; update changed fill only.
    static int oldFill=-1,oldSignal=-2;static bool oldBar=false,oldWifi=false;
    auto v=cassetteView;int barX=v.x+v.w/16,barW=v.w*7/8;
    int barY=v.y+v.h+4,barH=3;
    int fill=s.bufferCapacity?int(uint64_t(s.buffer)*barW/s.bufferCapacity):0;
    fill=constrain(fill,0,barW);
    if(barY+barH<=lcd.height()&&(layout||oldBar!=s.settings.bufferIndicator||oldFill!=fill)){
        lcd.fillRect(barX,barY,barW,barH,bg);
        if(s.settings.bufferIndicator){lcd.fillRect(barX,barY,barW,barH,0x2104);if(fill)lcd.fillRect(barX,barY,fill,barH,0x6c71);}
        oldFill=fill;oldBar=s.settings.bufferIndicator;
    }
    int signal=s.connected?(s.rssi>=-55?4:s.rssi>=-67?3:s.rssi>=-75?2:s.rssi>=-85?1:0):0;
    if(labels||oldWifi!=s.settings.wifiIndicator||oldSignal!=signal){
        cassetteRect(30,131,22,11);
        if(s.settings.wifiIndicator)for(int i=0;i<4;i++){
            int x=v.x+(31+i*5)*v.w/320,y=v.y+(140-(i+1)*2)*v.h/213;
            if(i<signal)lcd.fillRect(x,y,max(1,3*v.w/320),max(1,(i+1)*2*v.h/213),0x8410);
            else lcd.drawFastHLine(x,v.y+140*v.h/213,max(1,3*v.w/320),0xad55);
        }
        oldSignal=signal;oldWifi=s.settings.wifiIndicator;
    }
    // Disconnected shortcut stays on the opposite corner of the blue link icon.
    static int oldBt=-1,oldQuick=-1;
    const bool bt=btaudio::connected();
    const bool quick=true;
    if(layout||oldQuick!=int(quick)){
        cassetteRect(8,5,24,22);
        textSmall(v.x+11*v.w/320,v.y+11*v.h/213,28,"BT",0xf800,bg);
        oldQuick=int(quick);
    }
    // Restore only this small corner when the actual A2DP link changes.
    if(layout||oldBt!=int(bt)){
        cassetteRect(297,5,12,15);
        if(bt){
            static constexpr uint8_t glyph[]={8,12,10,41,26,12,26,41,10,12,8};
            for(int y=0;y<11;y++)for(int x=0;x<6;x++)if(glyph[y]&(1<<(5-x)))
                lcd.fillRect(v.x+(300+x)*v.w/320,v.y+(7+y)*v.h/213,
                    max(1,v.w/320),max(1,v.h/213),0x249f);
        }
        oldBt=int(bt);
    }
}
static const uint8_t wifiQr[33][5]={
{0xfe,0x44,0xe7,0x3f,0x80},{0x82,0x98,0x20,0xa0,0x80},{0xba,0xfb,0x0f,0xae,0x80},{0xba,0xd2,0xab,0xae,0x80},
{0xba,0x0f,0xca,0x2e,0x80},{0x82,0x3d,0x31,0xa0,0x80},{0xfe,0xaa,0xaa,0xbf,0x80},{0x00,0xa3,0x9b,0x00,0x00},
{0x82,0xa2,0xd1,0x67,0x00},{0x80,0x25,0x77,0xf9,0x00},{0x2e,0x8f,0xca,0x5e,0x00},{0xc8,0xf4,0xe5,0xef,0x00},
{0xdb,0x9d,0x55,0xe7,0x80},{0x70,0x14,0x64,0xe2,0x00},{0x8a,0x7f,0xce,0x59,0x00},{0x0d,0x92,0x89,0xdb,0x00},
{0xb6,0xa3,0xe7,0x4d,0x00},{0x04,0xb7,0xb8,0x47,0x80},{0xb7,0xb9,0xff,0x5d,0x80},{0xc9,0x1c,0x93,0x96,0x80},
{0x3b,0x45,0x25,0x0a,0x80},{0xd9,0x6c,0x41,0xcb,0x80},{0xb2,0x7e,0x10,0x05,0x00},{0xbd,0x5f,0x99,0x93,0x80},
{0xe6,0x86,0x44,0xf9,0x00},{0x00,0xc7,0xcf,0x89,0x80},{0xfe,0x01,0xcb,0xae,0x00},{0x82,0x08,0x66,0x8a,0x80},
{0xba,0x0b,0x9e,0xfc,0x00},{0xba,0x6a,0x21,0xd6,0x80},{0xba,0x53,0x35,0xef,0x80},{0x82,0x28,0x01,0x2a,0x00},
{0xfe,0xb7,0x40,0xdd,0x00}};
static bool qrBit(int x,int y){return wifiQr[y][x>>3]&(0x80>>(x&7));}
static bool renderWifiQr(){
    const int scale=5,quiet=2,total=(33+quiet*2)*scale;
    int ox=(lcd.width()-total)/2,oy=30+(lcd.height()-70-total)/2;
    lcd.fillRect(ox,oy,total,total,0xffff);
    for(int y=0;y<33;y++)for(int x=0;x<33;x++)if(qrBit(x,y))
        lcd.fillRect(ox+(x+quiet)*scale,oy+(y+quiet)*scale,scale,scale,0x0000);
    return true;
}
static bool bootDisplayReady=false,bootUsesVideoton=false;
static unsigned bootPaintedEpoch=~0u,bootOS=0;
static bootsequence::State bootSequence;
static void bootBase(){
    bootSequence.reset();
    if(bootOS){
        if(osboot::load(bootOS)){osboot::region(lcd,0,0,320,240);osboot::animate(lcd,bootOS,0);}
        else {bootOS=0;drawSkin(0);}
    }else if(bootUsesVideoton){
        videotono::bootImage(lcd,lcd.width(),lcd.height(),[]{vTaskDelay(1);});
        lcd.setFont(nullptr);lcd.setTextSize(1);lcd.setTextWrap(false);lcd.setTextColor(0x1082);
        lcd.setCursor(89,231);lcd.print("BETA ");lcd.print(IBT_FW_VERSION);
    }else{
        drawSkin(0);lcd.setFont(nullptr);lcd.setTextSize(2);lcd.setTextWrap(false);
        lcd.setTextColor(0xf800);lcd.setCursor(136,190);lcd.print("BETA");lcd.setTextColor(0xffff);
        lcd.setCursor((lcd.width()-int(strlen(IBT_FW_VERSION))*12)/2,212);lcd.print(IBT_FW_VERSION);
    }
    bootPaintedEpoch=bootprogress::state().epoch.load();
}
void bootRestart(uint8_t choice,uint8_t vintage,uint8_t skin){
    const unsigned art=bootprogress::artwork(choice,skin);bootOS=art>=3?art:0;
    bootUsesVideoton=art==1;
    bootprogress::begin(true,true);valid=false;
}
bool bootFrame(bool connected,bool setup,bool offline,unsigned focus){
    const auto decision=bootprogress::decision(connected,setup,offline);
    if(decision==bootprogress::Decision::Released)return false;
    if(decision==bootprogress::Decision::Setup){bootprogress::release();valid=false;return false;}
    static unsigned epoch=~0u;static uint32_t painted=0,completedAt=0;
    const unsigned current=bootprogress::state().epoch.load();const uint32_t now=millis();
    if(epoch!=current){
        epoch=current;painted=0;completedAt=0;
        if(bootPaintedEpoch!=current)bootBase();
        valid=false;
    }
    if(bootOS&&bootSequence.desktop){
        if(bootSequence.expired(now)){bootprogress::release();valid=false;return false;}
        return true;
    }
    if(!bootOS&&decision==bootprogress::Decision::Complete){
        if(!completedAt){completedAt=now?now:1;painted=0;}
        else if(uint32_t(now-completedAt)>=180){bootprogress::release();valid=false;return false;}
    }else completedAt=0;
    if(!painted||uint32_t(now-painted)>=(bootOS?50u:100u)){
        painted=now;
        const unsigned stage=decision==bootprogress::Decision::Complete?7:bootprogress::state().completed.load();
        if(bootOS){
            // Only presented animation frames advance time; setup cannot skip
            // Win7's four points joining. No delay or audio/network waiting.
            bootSequence.frame(now);
            osboot::animate(lcd,bootOS,bootSequence.animationMs);
            if(bootSequence.ready(bootOS,decision==bootprogress::Decision::Complete)){
                if(osboot::load(bootOS,true)){
                    osboot::region(lcd,0,0,320,240);
                    bootSequence.shown(millis()); // two seconds AFTER the full image
                }else{bootprogress::release();valid=false;return false;}
            }
        }
        else if(lcd.width()==320&&lcd.height()==240){
            if(bootUsesVideoton)videotono::bootProgress(lcd,stage,connected,offline,now/100,focus);
            else{
                lcd.fillRect(26,231,268,6,0x2104);const unsigned fill=bootprogress::fill(stage,connected,offline)*268/7;
                if(fill)lcd.fillRect(26,231,fill,6,0x07e0);
                // Original artwork remains unchanged; mode choice lives in Wi-Fi settings.
            }
        }
    }
    return true;
}
bool earlyBoot() {
    if(bootDisplayReady)return true;
    lcdSPI.begin(DUAL_TFT_SCK,DUAL_TFT_MISO,DUAL_TFT_MOSI,TFT_CS);
    // Physical SMALL panel: ST7789 320x240, native controller memory 240x320.
    lcd.init(240,320);
    // Read the same saved orientation used by the normal UI before boot art.
    uint8_t rotation=0,bootChoice=0,savedVintage=0,savedSkin=2;Preferences bootPrefs;
    if(bootPrefs.begin("yo-dual-v1",true)){
        rotation=bootPrefs.getUChar("rotate",0)&3u;bootChoice=bootPrefs.getUChar("bootart",0);savedVintage=bootPrefs.getUChar("vinskin",0);
        const bool media=bootPrefs.getUChar("mediaskin",0),kajon=false;
        const bool enabled=bootPrefs.getUChar("winamp",1),cassette=bootPrefs.getUChar("cassette",1);
        savedSkin=savedVintage==6?15:savedVintage?uint8_t(savedVintage+5):enabled?(kajon?4:media?3:2):cassette?1:2;bootPrefs.end();
    }
    lcd.setRotation((rotation+1)&3);
    lcd.invertDisplay(false); // Physical panel: correct RGB confirmed with V076 test.
    lcd.fillScreen(bg);
    // Original yoRadio RGB565 boot asset, enlarged2x with a248-byte scanline.
    // No filesystem, JPEG decoder, PSRAM or network is required at boot.
    pinMode(BRIGHTNESS_PIN,OUTPUT);digitalWrite(BRIGHTNESS_PIN,HIGH);
#if IBT_BOOT_RGB_TEST
    bootDisplayReady=true;
    return true;
#endif
    const unsigned bootArt=bootprogress::artwork(bootChoice,savedSkin);bootOS=bootArt>=3?bootArt:0;bootUsesVideoton=bootArt==1;
    bootprogress::begin(bootprogress::hold(bootChoice,savedSkin));
    bootBase();
    bootDisplayReady=true;
    return true;
}
#if IBT_BOOT_RGB_TEST
void rgbBootTestTick(){
    static uint32_t changedAt=0;static unsigned phase=0;static bool first=true;
    uint32_t now=millis();
    if(!first&&uint32_t(now-changedAt)<3000)return;
    if(!first)phase=(phase+1)%3;
    first=false;changedAt=now;
    static const uint16_t colors[]={0xf800,0x07e0,0x001f};
    static const char* names[]={"R - PIROS","G - ZOLD","B - KEK"};
    lcd.fillScreen(colors[phase]);
    lcd.setFont(nullptr);lcd.setTextSize(3);lcd.setTextWrap(false);lcd.setTextColor(0x0000);
    int16_t x,y;uint16_t w,h;
    lcd.getTextBounds(names[phase],0,0,&x,&y,&w,&h);
    lcd.setCursor((lcd.width()-int(w))/2-x,(lcd.height()-int(h))/2-y);
    lcd.print(names[phase]);
    Serial.printf("[RGB-TEST] phase=%u rgb565=%04x size=%dx%d\n",phase,colors[phase],lcd.width(),lcd.height());
}
#endif
bool begin() { return earlyBoot(); }
void setRotation(uint8_t r) {menuSlices.cancel();lcd.setRotation((r+1)&3);valid=false;}
void calibrationTarget(int x,int y,unsigned step){lcd.fillScreen(bg);char b[48];snprintf(b,sizeof(b),"TOUCH CAL %u/4",step);text(8,8,300,b,fg);text(8,30,300,"Erintsd meg a keresztet",accent);lcd.drawCircle(x,y,10,accent);lcd.drawFastHLine(x-16,y,33,fg);lcd.drawFastVLine(x,y-16,33,fg);}
void calibrationDone(bool ok){lcd.fillScreen(bg);text(8,90,300,ok?"Touch kalibracio OK":"Touch kalibracio sikertelen",ok?accent:fg);delay(700);valid=false;}
void service(){
    if(!valid||connectionScreen||previousSetup||gallery.active()||previousPage||previousNightClock)return;
    const bool pressured=dual::uiOverloadActive();
    if(pressured&&!overloadDotPainted){
        lcd.fillCircle(lcd.width()-7,7,4,0xF800);
        overloadDotPainted=true;
    }else if(!pressured&&overloadDotPainted){
        overloadDotPainted=false;
        if(playerRequested&&!previousVintage&&!previousMediaPlayer&&!previousKajon){
            uint16_t pixels[12*12];const int x=lcd.width()-12;
            for(int y=0;y<12;++y)for(int xx=0;xx<12;++xx)pixels[y*12+xx]=playerSkinEq[y*320+x+xx];
            lcd.drawRGBBitmap(x,0,pixels,12,12);
        }else valid=false;
    }
}
void artworkClear(){coverVisible=false;artW=artH=0;artDirty=true;cassetteDirty=true;++vintageCoverRevision;}
bool artworkBegin(int w,int h){
    if(!valid||connectionScreen||previousSetup||previousNightClock||(playerRequested&&(previousVintage==5||previousVintage==6)))return false;
    if(gallery.active())return false;
    const bool vintage=playerRequested&&(previousVintage!=0||previousMediaPlayer);
    if(w<=0||h<=0||w>112||h>112||(!vintage&&(!cassetteRequested||playerRequested||!cassettePixels))||previousPage||network.status==SOFT_AP)return false;
    if(!coverPixels)coverPixels=(uint16_t*)heap_caps_malloc(112*112*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!coverPixels)return false;coverVisible=false;artW=w;artH=h;return true;
}
void artworkTile(int x,int y,int w,int h,int stride,const uint16_t *p){
    if(!coverPixels||!p||x<0||y<0||w<=0||h<=0||stride<w||x+w>artW||y+h>artH)return;
    for(int row=0;row<h;row++)memcpy(coverPixels+(y+row)*112+x,p+row*stride,w*2);
}
void artworkEnd(){coverVisible=true;cassetteDirty=true;++vintageCoverRevision;}
static __attribute__((noinline)) void renderService(bool layout,dual::Language lang){
    const auto settings=dual::serviceSettings();const auto status=player.serviceStatus();
    const bool info=dual::serviceDiagnostics();
    const uint16_t* plate=packedpixels::pixels(serviceface::asset);
    if(!plate){if(layout){lcd.fillScreen(bg);textSmall(8,60,300,"Service: PSRAM cache unavailable",fg,bg);}return;}
    auto& previous=uiCache->servicePrevious;char signature[160];
    snprintf(signature,sizeof(signature),"%u/%u/%u/%u/%lu/%d/%lu/%lu/%u/%u/%u",info,unsigned(settings.wave),unsigned(settings.route),unsigned(settings.channel),(unsigned long)settings.hz,int(settings.db),(unsigned long)settings.rate,(unsigned long)settings.step,unsigned(status.stage),unsigned(status.fault),unsigned(status.denied));
    const bool changed=layout||strcmp(previous,signature)!=0;
    int c0=-1,c1=-1;if(info)loadprobe::cached(c0,c1);
    serviceface::View view{settings.hz,settings.step,settings.rate,status.frames,0,0,unsigned(settings.wave),unsigned(settings.channel),unsigned(settings.route),unsigned(status.stage),unsigned(status.fault),unsigned(status.denied),0,c0,c1,info};
    // Only a changed status/diagnostic value redraws a small dynamic region.
    char value[100];unsigned heap=info?heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT):0;
    snprintf(value,sizeof(value),"%llu/%u/%d/%d",(unsigned long long)status.frames,heap,c0,c1);
    if(changed||strcmp(uiCache->serviceStatus,value)){
      if(changed)drawPlayer(plate); // Existing mutable scanline fast path, yields every16 rows.
      serviceface::paint(lcd,plate,view,int(settings.db),changed,false);
      if(info){
        char lineValue[70];snprintf(lineValue,sizeof(lineValue),"PCM %lu Hz  BT %u Hz",(unsigned long)status.settings.rate,btaudio::negotiatedOutputRate());textSmall(10,66,300,lineValue,0x07ff,0x0841);
        snprintf(lineValue,sizeof(lineValue),"Internal RAM: %u B",heap);textSmall(10,87,300,lineValue,0xffff,0x0841);
        if(c0<0||c1<0)snprintf(lineValue,sizeof(lineValue),"CPU: -- / --");else snprintf(lineValue,sizeof(lineValue),"CPU: %d.%d%% / %d.%d%%",c0/10,c0%10,c1/10,c1%10);
        textSmall(10,108,300,lineValue,0x07ff,0x0841);
        textSmall(10,130,300,"Audio owner: Core 1",0xffff,0x0841);textSmall(10,151,300,"Uncalibrated test signal",0xfd20,0x0841);
      }
      strlcpy(uiCache->serviceStatus,value,sizeof(uiCache->serviceStatus));
    }
    strlcpy(previous,signature,sizeof(previous));
}
static __attribute__((noinline)) void renderBtSpeaker(const dual::Snapshot& s,bool layout){
    const uint16_t* plate=packedpixels::pixels(btspeakerface::asset);
    if(!plate){if(layout){lcd.fillScreen(bg);textSmall(8,60,300,"BT speaker: PSRAM cache unavailable",fg,bg);}return;}
    if(layout){drawPlayer(plate);memset(uiCache->previousBands,255,48);}
    auto field=[&](unsigned id,videotono::Rect r,const char* value,bool large=false){
      if(layout||strcmp(rows[id],value)){btspeakerface::field(lcd,plate,r,value,large);dual::textCopy(rows[id],192,value);}
    };
    const bool link=btaudio::connected(),playing=btaudio::inputPlaying();
    field(0,{229,13,76,16},link?"CONNECTED":"PAIRING");
    field(1,{22,62,276,22},s.artist,true);field(2,{22,86,276,22},s.title,true);
    char textValue[20];snprintf(textValue,sizeof(textValue),"%lu:%02lu",(unsigned long)(videotonBtClock.seconds/60),(unsigned long)(videotonBtClock.seconds%60));field(3,{261,48,35,12},textValue);
    snprintf(textValue,sizeof(textValue),"%u",unsigned(s.volume));
    if(layout||strcmp(rows[4],textValue)){btspeakerface::volume(lcd,plate,(unsigned(s.volume)*100+127)/254);strlcpy(rows[4],textValue,192);}
    if(layout||rows[5][0]!=(playing?'1':'0')){btspeakerface::play(lcd,plate,playing);rows[5][0]=playing?'1':'0';rows[5][1]=0;}
    static uint32_t previousAt=0;const uint32_t now=millis();
    if(layout||uint32_t(now-previousAt)>=100){previousAt=now;uint8_t measured[24]{};
      if(link&&playing&&vuprobe::dacEnabled.load(std::memory_order_relaxed))playerSpectrum(measured,24);
      btspeakerface::bars(lcd,plate,measured,uiCache->previousBands,layout);
    }
}

static bool artworkFast=false;
void artworkTestFast(bool on){artworkFast=on;}
void artworkTestRows(unsigned y,unsigned n,const uint16_t *pixels){
    if(!pixels||y>=240||n>240-y||lcd.width()!=320||lcd.height()!=240)return;
    if(!n)return;
    if(!artworkFast){lcd.drawRGBBitmap(0,int(y),pixels,320,int(n));return;}
    // Synchronous SPI, bounded four-row PSRAM buffer. writePixels reads only;
    // const overload of drawRGBBitmap selected the per-pixel GFX/PROGMEM path.
    lcd.startWrite();lcd.setAddrWindow(0,int(y),320,int(n));
    lcd.writePixels(const_cast<uint16_t*>(pixels),320*n,true,false);
    lcd.endWrite();
}
void renderBattery(const dual::Snapshot &s) {
    if((s.settings.playerEnabled&&(s.settings.vintageSkin==5||s.settings.vintageSkin==6))||connectionScreen||previousSetup||!s.settings.batteryIndicator||gallery.active()||s.settings.alarm.clockHome||previousPage!=0)return;
    int p=s.settings.sensorMode==2&&s.sensorValid?battery::percent(s.sensorMv,s.settings.batteryMv):-1;
    p=battery::quantize(p,s.settings.batteryStep);
    static int old=-2;static unsigned style=255;static uint32_t generation=0;static unsigned context=255;
    static int oldX=-1,oldY=-1;
    int x=130,y=7;uint16_t base=bg;
    if(playerRequested&&s.settings.vintageSkin){x=s.settings.vintageSkin==4?25:12;y=s.settings.vintageSkin==4?177:7;base=s.settings.vintageSkin==4?0x0021:s.settings.vintageSkin==1?0x0040:s.settings.vintageSkin==2?0x1082:0x3104;}
    else if(playerRequested&&false){x=207;y=214;base=0x0000;}
    else if(playerRequested&&s.settings.mediaPlayerSkin){x=171;y=7;base=0x0258;}
    else if(playerRequested){x=15;y=46;base=0x0020;}
    else if(cassetteRequested){x=231;y=224;base=bg;}
    // Structural draws mark batteryDirty; ordinary updates paint only a changed readout.
    if(!batteryDirty&&old==p&&style==s.settings.batteryAppearance&&generation==s.generation&&context==previousPage&&oldX==x&&oldY==y)return;
    batteryDirty=false;old=p;style=s.settings.batteryAppearance;generation=s.generation;context=previousPage;oldX=x;oldY=y;
    const uint8_t vintage=playerRequested?s.settings.vintageSkin:0;
    if(vintage)vintageRestore(vintage,s.settings.retroColor,x,y,74,13);
    else lcd.fillRect(x,y,74,13,base);
    char value[24];
    if(style==0){
        if(p<0)snprintf(value,sizeof(value),"Akku: --");else snprintf(value,sizeof(value),"Akku: ~%d%%",p);
        if(vintage)vintageLabel(vintage,s.settings.retroColor,x+2,y+2,70,value,vintageAccent(vintage,s.settings.retroColor),true);
        else textSmall(x+2,y+2,70,value,0xffff,base);
    }
    else {
        const int iconX=x+(style==1?30:45),width=style==1?40:26;
        if(style==2){
            if(p<0)snprintf(value,sizeof(value),"--");else snprintf(value,sizeof(value),"~%d%%",p);
            if(vintage)vintageLabel(vintage,s.settings.retroColor,x+2,y+2,42,value,vintageAccent(vintage,s.settings.retroColor),true);
            else textSmall(x+2,y+2,42,value,0xffff,base);
        }
        lcd.drawRect(iconX,y+1,width,10,0xffff);lcd.fillRect(iconX+width,y+4,2,4,0xffff);
        if(p>=0)lcd.fillRect(iconX+2,y+3,(width-4)*p/100,6,p<=10?0xf800:0x07e0);
        else if(vintage)vintageLabel(vintage,s.settings.retroColor,iconX+3,y+2,width-6,"?",0xffff,true);
        else textSmall(iconX+3,y+2,width-6,"?",0xffff,base);
    }
}
void render(const dual::Snapshot &s,unsigned page,unsigned selected,bool force) {
    if(!ensureRows())return;
    tracktext::line(uiCache->trackLine,sizeof(uiCache->trackLine),s.artist,s.title);
    menuTypography=content::isMenuPage(page);
    if(gallery.active()){
        menuSlices.cancel();
        if(gallery.painted!=gallery.index){drawSkin(gallery.index);gallery.painted=gallery.index;overloadDotPainted=false;}
        return;
    }
    time_t now=time(nullptr);tm local={};localtime_r(&now,&local);
    bool dark=dual::isDark(s.settings,local.tm_hour,now>1600000000);
    playerRequested=s.settings.playerEnabled&&lcd.width()==320&&lcd.height()==240;
    cassetteRequested=s.settings.cassetteEnabled&&!playerRequested;
    const bool setup=network.status==SOFT_AP;
    char connectingName[WIFI_SSID_CAP]={0};uint8_t connectingTry=0;
    const bool connecting=network.connectingInfo(connectingName,sizeof(connectingName),&connectingTry);
    const bool wasConnectionScreen=connectionScreen;
    connectionScreen=!setup&&connecting;
    if(connectionScreen||setup)menuTypography=false;
    // AP has only onboarding and QR layouts, independent of the underlying menu.
    if(setup)page=(page==17||page==28||page==42)?page:0;
    // A redraw request schedules inspection of changed rows; only structural
    // transitions invalidate the whole screen. Station/artwork changes are local.
    const bool slicedMenu=menuTypography&&!setup&&!connectionScreen;
    if(!slicedMenu)menuSlices.cancel();
    const bool nightClock=page==0&&!setup&&s.settings.alarm.clockHome;
    clockart::homeTypography=nightClock;
    static uint32_t oldClockStyle=UINT32_MAX;const uint32_t clockStyle=s.settings.alarm.clockStyle|(unsigned(s.settings.alarm.clockColor)<<8)|(unsigned(s.settings.alarm.textColor)<<16);
    const bool clockStyleChanged=nightClock&&oldClockStyle!=clockStyle;oldClockStyle=clockStyle;
    clockart::style(s.settings.alarm.clockStyle,s.settings.alarm.clockColor,s.settings.alarm.textColor);
    const bool clockPhoto=nightClock&&weather::enabled();
    const bool clockPhotoChanged=clockart::photoBackdrop!=clockPhoto;clockart::photoBackdrop=clockPhoto;
    bool layout=clockStyleChanged||clockPhotoChanged||!valid||connectionScreen!=wasConnectionScreen||dark!=previousDark||page!=previousPage||setup!=previousSetup||nightClock!=previousNightClock||s.settings.batteryIndicator!=previousBatteryEnabled||s.settings.wifiIndicator!=previousWifiEnabled||s.settings.language!=previousLanguage||cassetteRequested!=previousCassette||playerRequested!=previousPlayer||s.settings.mediaPlayerSkin!=previousMediaPlayer||false!=previousKajon||s.settings.vintageSkin!=previousVintage||s.settings.retroColor!=previousRetroColor;
    previousBatteryEnabled=s.settings.batteryIndicator;previousWifiEnabled=s.settings.wifiIndicator;
    previousNightClock=nightClock;
    previousMediaPlayer=s.settings.mediaPlayerSkin;previousKajon=false;
    previousVintage=s.settings.vintageSkin;previousRetroColor=s.settings.retroColor;
    if(s.generation!=previousGeneration){
        coverVisible=false;artW=artH=0;artDirty=true;cassetteDirty=true;
        // A new track can have the same visible title. Invalidate the cover
        // independently so the previous album image never survives it.
        ++vintageCoverRevision;
    }
    previousGeneration=s.generation;
    if(layout) {
        batteryDirty=true;
        bg=dark?0x0841:0xef7d;fg=dark?0xffff:0x1082;accent=dark?0x05da:0x0290;muted=dark?0x7bef:0x8410;
        if(!slicedMenu&&(connectionScreen||setup||(!nightClock&&!((playerRequested||cassetteRequested)&&(!page||content::isMenuPage(page))))))lcd.fillScreen(bg);
        overloadDotPainted=false;memset(rows,0,16*192);memset(uiCache->widths,0,sizeof(uiCache->widths));
        valid=true;previousDark=dark;previousPage=page;previousSetup=setup;previousLanguage=s.settings.language;previousCassette=cassetteRequested;previousPlayer=playerRequested;artDirty=true;
        if(cassetteRequested||s.settings.vintageSkin||s.settings.mediaPlayerSkin){if(s.sd)artwork::redraw(s.generation);else onlineart::redraw(s.generation);}
    }
    int w=lcd.width(),h=lcd.height();const auto lang=s.settings.language;
    if(slicedMenu){
        menuSlices.begin(page,selected,layout,force,unsigned(h));
        layout=menuSlices.layout;
        if(const unsigned stripe=menuSlices.clearRows(unsigned(h))){
            uint16_t base=bg;
            if(alarmui::menu(page))base=clockart::bg;
            else if(playerRequested&&s.settings.vintageSkin){const unsigned v=s.settings.vintageSkin;base=v==6?videotono::glass:v==5?0x0863:v==4?0x0021:v==1?0x0040:v==2?0x1082:0x3104;}
            else if(playerRequested&&false)base=0x0841;
            else if(playerRequested&&s.settings.mediaPlayerSkin)base=0xc638;
            else if(playerRequested||cassetteRequested)base=cassetteRequested?0x2104:0x1083;
            lcd.fillRect(0,int(menuSlices.clearY),w,int(stripe),base);
            menuSlices.clearY+=stripe;return;
        }
    }
    if(connectionScreen){
        char connectionText[80];
        line(0,8,25,w-16,content::pick(lang,"Csatlakozás...","Connecting...","Verbinden..."),accent,layout);
        line(1,8,75,w-16,connectingName,fg,layout);
        if(connectingTry){snprintf(connectionText,sizeof(connectionText),"%s %u/3",content::pick(lang,"Próba","Attempt","Versuch"),connectingTry);line(2,8,112,w-16,connectionText,muted,layout);}
        else line(2,8,112,w-16,content::pick(lang,"Mentett hálózat keresése","Searching saved networks","Gespeicherte Netze suchen"),muted,layout);
        return;
    }

    if(nightClock){
        if(layout)dual::clockWake();
        const bool controls=dual::clockControlsVisible();
        static bool oldControls=false;static unsigned oldHeaderFocus=99;
        const auto &alarm=s.settings.alarm;
        time_t stamp=time(nullptr);tm clockLocal{};
        const bool clockValid=stamp>0&&localtime_r(&stamp,&clockLocal)&&clockLocal.tm_year+1900>=2024;
        const time_t ending=dual::alarmEndTime();const bool ringing=ending>0;
        const time_t next=clockValid?(ringing?ending:alarmclock::nextStart(stamp,alarm,dual::alarmLastOccurrence())):0;
        const unsigned long minutesLeft=next>stamp?static_cast<unsigned long>((next-stamp+59)/60):0;
        if(layout){
            clockRestore(0,0,w,h);
            lcd.drawFastHLine(0,32,w,clockart::edge);lcd.drawFastHLine(14,194,w-28,clockart::edge);
        }
        const auto outside=weather::reading();const auto weatherConfig=weather::configuration();
        radiosensor::Reading inside{};const bool haveInside=sensors::enabled()&&sensors::reading(inside);
        const bool haveOutside=weatherConfig.enabled&&weather::fresh(outside,millis());
        char headerState[140];snprintf(headerState,sizeof(headerState),"%u/%u/%u/%d/%u/%u/%d/%u",controls?1:0,dual::clockHomeFocus(),sensors::enabled()?1:0,int(inside.temperatureCenti),haveInside?unsigned(inside.humidityCenti)+1:0,weatherConfig.enabled?1:0,int(outside.temperatureTenths),haveOutside?unsigned(outside.humidity)+1:0);
        if(layout||strcmp(rows[6],headerState)){
            clockRestore(0,0,w,33);
            if(controls){
                clockart::text(lcd,8,9,120,content::pick(lang,"Menü","Menu","Menü"),16);
                clockart::text(lcd,w-100,9,92,content::pick(lang,"Ébresztő","Alarm","Wecker"),16);
                const unsigned focus=dual::clockHomeFocus();if(focus==9)lcd.drawFastHLine(8,29,88,clockart::accent);else if(focus==10)lcd.drawFastHLine(w-100,29,92,clockart::accent);
            }else{
                const bool indoor=haveInside,outdoor=haveOutside;
                if(indoor&&outdoor){
                    clockart::sensorLine(lcd,0,160,content::pick(lang,"Bent","In","Innen"),haveInside,int(inside.temperatureCenti)/10,unsigned(inside.humidityCenti/100));
                    clockart::sensorLine(lcd,160,160,content::pick(lang,"Kint","Out","Außen"),haveOutside,int(outside.temperatureTenths),unsigned(outside.humidity));
                }else if(indoor)clockart::sensorLine(lcd,0,320,content::pick(lang,"Bent","In","Innen"),haveInside,int(inside.temperatureCenti)/10,unsigned(inside.humidityCenti/100));
                else if(outdoor)clockart::sensorLine(lcd,0,320,content::pick(lang,"Kint","Out","Außen"),true,int(outside.temperatureTenths),unsigned(outside.humidity));
                else if(sensors::enabled()||weatherConfig.enabled)clockart::sensorLine(lcd,0,320,sensors::enabled()?content::pick(lang,"Bent","In","Innen"):content::pick(lang,"Kint","Out","Außen"),false,0,0);
            }
            lcd.drawFastHLine(0,32,w,clockart::edge);
            strlcpy(rows[6],headerState,sizeof(rows[6]));oldControls=controls;oldHeaderFocus=dual::clockHomeFocus();
        }
        char digits[12];
        if(alarm.clockDisplay==1){if(next>stamp)snprintf(digits,sizeof(digits),"%02lu:%02lu",minutesLeft/60,minutesLeft%60);else strlcpy(digits,"--:--",sizeof(digits));}
        else if(clockValid)snprintf(digits,sizeof(digits),"%02u:%02u",unsigned(clockLocal.tm_hour),unsigned(clockLocal.tm_min));
        else strlcpy(digits,"--:--",sizeof(digits));
        // Keep the colon's advance fixed while blinking; never move the digits.
        static char oldDigits[12]={};static bool oldColon=false;
        bool colon=!(alarm.blinkColon&&clockValid&&(clockLocal.tm_sec%2));
        if(layout||strcmp(oldDigits,digits)||oldColon!=colon){
            const bool digitsChanged=layout||strcmp(oldDigits,digits);
            int x=(w-clockart::clockWidth(digits))/2;
            if(digitsChanged)clockRestore(0,56,w,92);
            for(const char* c=digits;*c;++c){const int advance=clockart::clockAdvance(*c);
                if(advance&&(digitsChanged||*c==':')){if(*c==':')clockRestore(x,56,advance,92);clockart::clockDigit(lcd,x,clockart::clockY(),*c,*c!=':'||colon);}
                x+=advance;
            }
            strlcpy(oldDigits,digits,sizeof(oldDigits));oldColon=colon;
        }
        auto label=[&](unsigned id,int y,const char*value,unsigned size,uint16_t color){
            if(layout||strcmp(rows[id],value)){clockRestore(8,y,w-16,size+5);while(size>14&&clockart::width(value,size)>w-20)size=size==24?20:size==20?16:14;clockart::text(lcd,8,y,w-16,value,size,color,clockart::bg,true);dual::textCopy(rows[id],sizeof(rows[id]),value);}
        };
        char detail[128]={};
        if(!clockValid)strlcpy(detail,controls?content::pick(lang,"Időszinkronra vár","Waiting for time sync","Warte auf Uhrzeit"):"",sizeof(detail));
        else if(alarm.clockDisplay==2&&next>stamp)snprintf(detail,sizeof(detail),content::pick(lang,"Ébresztésig %lu óra %lu perc","Alarm in %lu h %lu min","Wecker in %lu h %lu min"),minutesLeft/60,minutesLeft%60);
        else if(alarm.clockDisplay==1)strlcpy(detail,content::pick(lang,"Következő ébresztésig","Until next alarm","Bis zum Wecken"),sizeof(detail));
        else snprintf(detail,sizeof(detail),"%04u.%02u.%02u.",unsigned(clockLocal.tm_year+1900),unsigned(clockLocal.tm_mon+1),unsigned(clockLocal.tm_mday));
        label(2,153,detail,16,clockart::fg);
        char meteo[160]={};
        // Age and diagnostics appear only while the user has woken controls.
        if(controls){
            if(haveInside){const uint32_t age=std::max(uint32_t(millis()-inside.temperatureAt),uint32_t(millis()-inside.humidityAt))/60000;snprintf(meteo,sizeof(meteo),"Bent: %lu p",(unsigned long)age);}
            else if(sensors::enabled())snprintf(meteo,sizeof(meteo),"%s",sensors::status());
            if(weatherConfig.enabled){const size_t n=strlen(meteo);if(haveOutside)snprintf(meteo+n,sizeof(meteo)-n,"%sKint: %lu p",n?" · ":"",(unsigned long)(uint32_t(millis()-outside.updated)/60000));else snprintf(meteo+n,sizeof(meteo)-n,"%s%s",n?" · ":"",weather::status());}
        }
        label(5,174,meteo,14,clockart::fg);
        snprintf(detail,sizeof(detail),"%02u:%02u",unsigned(alarm.onHour),unsigned(alarm.onMinute));
        static uint32_t oldStrip=0xffffffff;
        const uint32_t strip=uint32_t(alarm.daysMask)|(alarm.mode?128:0)|(uint32_t(lang)<<8)|(uint32_t(controls)<<16)|(dual::clockHomeFocus()<<17);
        if(layout||force||oldStrip!=strip||strcmp(rows[3],detail)){
            clockRestore(0,194,w,46);
            lcd.drawFastHLine(14,194,w-28,clockart::edge);
            const uint16_t active=0x07e0,inactive=0x8410,icon=alarm.mode?active:inactive;
            clockart::alarmIcon(lcd,5,207,alarm.mode!=0);
            lcd.drawFastVLine(99,208,25,clockart::edge);
            const unsigned timeSize=clockart::width(detail,20)<=65?20:16;
            clockart::text(lcd,30,211,65,detail,timeSize,clockart::accent,clockart::bg,true);
            const char* hu[]={"H","K","Sze","Cs","P","Szo","V"};
            const char* en[]={"Mo","Tu","We","Th","Fr","Sa","Su"};
            const char* de[]={"Mo","Di","Mi","Do","Fr","Sa","So"};
            auto names=unsigned(lang)==0?hu:unsigned(lang)==1?en:de;
            for(unsigned i=0;i<7;++i){int x=102+i*(w-102)/7;const unsigned daySize=clockart::width(names[i],14)<=(w-102)/7?14:12;clockart::text(lcd,x,214,(w-102)/7,names[i],daySize,(alarm.daysMask&(1u<<i))?active:inactive,clockart::bg,true);}
            if(controls&&dual::clockHomeFocus()<9){unsigned f=dual::clockHomeFocus();int left=f==0?5:f==1?30:102+(f-2)*(w-102)/7;int width=f==0?23:f==1?70:(w-102)/7;lcd.drawFastHLine(left,237,width,clockart::accent);}
            strlcpy(rows[3],detail,sizeof(rows[3]));oldStrip=strip;
        }
        return;
    }
    if(page==69&&!setup){wifiPasswordScreen(selected,layout,lang,true);return;}
    if(page==67&&!setup){
        if(layout||force){
            lcd.fillScreen(clockart::bg);
            clockart::text(lcd,8,8,65,content::pick(lang,"Vissza","Back","Zurück"),14);
            clockart::text(lcd,75,8,170,content::pick(lang,dual::alarmDraftEnding()?"Befejezés":"Kezdés",dual::alarmDraftEnding()?"End":"Start",dual::alarmDraftEnding()?"Ende":"Beginn"),24,clockart::fg,clockart::bg,true);
            const int centers[]={80,240};
            for(unsigned i=0;i<2;++i){
                clockart::text(lcd,i*160,43,160,i?content::pick(lang,"Perc","Minute","Minute"):content::pick(lang,"Óra","Hour","Stunde"),16,clockart::fg,clockart::bg,true);
                lcd.drawRoundRect(centers[i]-70,62,140,65,6,selected==i?clockart::accent:clockart::edge);
                lcd.drawRoundRect(centers[i]-69,63,138,63,5,selected==i?clockart::accent:clockart::edge);
                char n[4];snprintf(n,sizeof(n),"%02u",unsigned(i?dual::alarmDraftMinute():dual::alarmDraftHour()));
                clockart::text(lcd,i*160,71,160,n,48,selected==i?clockart::accent:clockart::fg,clockart::bg,true);
            }
            for(unsigned i=0;i<2;++i){clockart::button(lcd,10+i*160,140,140,44,i?"+":"–",false);}
            clockart::button(lcd,10,190,140,44,content::pick(lang,"Mentés","Save","Speichern"),true,selected==2);
            clockart::button(lcd,170,190,140,44,content::pick(lang,"Mégse","Cancel","Abbrechen"),false,selected==3);
            if(dual::alarmDraftError()){lcd.fillRect(74,5,246,29,clockart::bg);clockart::text(lcd,78,11,238,content::pick(lang,"Azonos kezdés és vég","Start equals end","Beginn gleich Ende"),14,clockart::accent);}
        }return;
    }
    if(page==48&&!setup){renderService(layout,lang);return;}
    if(page==51&&!setup){
        char value[100];unsigned point=dual::sensorCalibrationPoint();
        line(0,8,8,w-16,content::pick(lang,"GPIO34 kalibráció","GPIO34 calibration","GPIO34 Kalibrierung"),accent,layout);
        snprintf(value,sizeof(value),"%u%% = %u mV",point*25,unsigned(s.settings.batteryMv[point]));line(1,8,70,w-16,value,fg,layout);
        snprintf(value,sizeof(value),"GPIO34: %u mV%s",unsigned(s.sensorMv),s.sensorValid?"":" --");line(2,8,110,w-16,value,muted,layout);
        line(3,8,150,w-16,"Rotary: +/- 10 mV",muted,layout);
        line(4,8,202,w-16,"-10 mV       <       +10 mV",accent,layout);return;
    }
    char buffer[200];
    uint16_t stressAction=0,stressDraw=0;uint32_t stressActions=0,stressFrames=0;
    const bool showStress=page==1&&dual::r10UiStressInfo(stressAction,stressDraw,stressActions,stressFrames);


    // Mandatory Wi-Fi onboarding screen. While setup AP is active normal menus are hidden,
    // but no feature is deleted: after successful STA connect the ordinary UI returns.
    if(network.status==SOFT_AP){
        if(page==42){wifiPasswordScreen(selected,layout,lang);return;}
        if(page==17)snprintf(buffer,sizeof(buffer),"Wi-Fi QR");
        else if(page==28)snprintf(buffer,sizeof(buffer),"Wi-Fi scan");
        else snprintf(buffer,sizeof(buffer),"%s",content::pick(lang,"Wi-Fi beállítás","Wi-Fi setup","Wi-Fi Setup"));
        if(page==17||page==28)line(0,8,4,w-16,buffer,accent,layout);
        if(page==17){
            if(layout&&!renderWifiQr())line(1,8,92,w-16,"QR generation failed",fg,true);
            line(14,8,h-30,w-16,content::pick(lang,"Vissza","Back","Zurück"),accent,layout);
            return;
        }
        if(page==28){
            if(layout){lcd.drawRoundRect(w-83,3,75,28,5,accent);text(w-73,10,65,"FRISS",accent);}
            const uint8_t phase=network.scanStatus();
            const unsigned count=network.scanCount();
            if(phase==1)line(1,8,42,w-16,content::pick(lang,"Keresés...","Scanning...","Suche..."),fg,layout);
            else if(phase==3)line(1,8,42,w-16,content::pick(lang,"Keresés sikertelen","Scan failed","Suche fehlgeschlagen"),fg,layout);
            else if(!count)line(1,8,42,w-16,content::pick(lang,"Nincs hálózat","No networks","Keine Netze"),fg,layout);
            else {
                const unsigned rows=6;
                unsigned first=selected>=rows/2?selected-rows/2:0;
                if(count>rows&&first>count-rows)first=count-rows;
                for(unsigned row=0;row<rows;row++){
                    char ssid[WIFI_SSID_CAP];int rssi=0;
                    const unsigned index=first+row;
                    if(index<count&&network.scanEntry(index,ssid,sizeof(ssid),&rssi)){
                        snprintf(buffer,sizeof(buffer),"%c %s  %d",index==selected?'>':' ',ssid,rssi);
                        line(1+row,8,39+row*26,w-16,buffer,index==selected?accent:fg,layout);
                    }else line(1+row,8,39+row*26,w-16,"",fg,layout);
                }
            }
            if(layout){lcd.drawRoundRect(8,h-38,106,30,4,accent);text(18,h-32,88,"JELSZO",accent);}
            return;
        }
        // One clean layout, generous 44px footer targets, no overlapping QR/text.
        static unsigned setupFocus=99;
        if(layout||setupFocus!=selected){
            setupFocus=selected;
            lcd.drawFastHLine(8,49,w-16,accent);
            auto key=[&](setupui::Rect r,const char* label,bool focused){
                const uint16_t oldBg=bg;bg=focused?0x1948:0x1082;
                lcd.fillRoundRect(r.x,r.y,r.w,r.h,6,bg);
                lcd.drawRoundRect(r.x,r.y,r.w,r.h,6,focused?accent:muted);
                text(r.x+8,r.y+14,r.w-16,label,0xffff);bg=oldBg;
            };
            text(12,17,w-116,content::pick(lang,"Wi-Fi beállítás","Wi-Fi setup","WLAN Setup"),accent);
            key(setupui::scan(w),"SCAN",selected==3);
            key(setupui::reboot(h),"Reboot",selected==0);
            key(setupui::player(w,h),content::pick(lang,"Lejátszó","Player","Player"),selected==1);
            key(setupui::qr(w,h),"QR",selected==2);
            text(14,61,w-28,content::pick(lang,"Telefonról vagy SCAN gombbal","Use your phone or SCAN","Mit Handy oder SCAN"),fg);
            text(14,90,w-28,network.setupSsid(),accent);
            snprintf(buffer,sizeof(buffer),"%s: %s",content::pick(lang,"Jelszó","Password","Passwort"),apPassword);
            text(14,113,w-28,buffer,fg);
            text(14,139,w-28,"http://192.168.4.1",accent);
        }
        if(connecting)snprintf(buffer,sizeof(buffer),"%.27s...",connectingName);
        else snprintf(buffer,sizeof(buffer),"%s",network.setupFailed()?content::pick(lang,"Sikertelen. Próbáld újra!","Failed. Try again!","Fehler. Erneut versuchen!"):content::pick(lang,"Lejátszó: SD / BT / DAC, Wi-Fi nélkül","Player: SD / BT / DAC, no Wi-Fi","Player: SD / BT / DAC ohne WLAN"));
        line(7,14,165,w-28,buffer,muted,layout);
        return;
    }

    if(page==42){wifiPasswordScreen(selected,layout,lang);return;}

    if(!page){
        if(playerRequested){if(s.settings.vintageSkin==6)videotonHome(s,layout);
            else if(s.settings.vintageSkin==5)weatherHome(s,layout);
            else if(s.settings.vintageSkin==4)neonHome(s,layout);else if(s.settings.vintageSkin)vintageHome(s,layout);else if(s.settings.mediaPlayerSkin)mediaPlayerHome(s,layout);else playerHome(s,layout);}
        else if(cassetteRequested)cassetteHome(s,layout);else playerHome(s,layout);
        // Clock mode is entered from the menu; no repeatedly painted skin overlay.
        return;
    }

    if(page==41){
        const bool toSd=dual::sourcePendingSd();
        const auto status=player.sdPrepareState();
        line(0,8,8,w-16,content::pick(lang,"Forrásváltás","Changing source","Quelle wechseln"),accent,layout);
        if(!toSd){
            line(1,8,62,w-16,content::pick(lang,"Internetrádióra váltás","Switching to internet radio","Wechsel zum Internetradio"),fg,layout);
            line(2,8,100,w-16,content::pick(lang,"Kérlek várj...","Please wait...","Bitte warten..."),muted,layout);
        }else if(status==SdPrepareState::Indexing){
            snprintf(buffer,sizeof(buffer),"SD: %lu %s",(unsigned long)sdman.indexedTracks.load(),
              content::pick(lang,"fájl beolvasva","files indexed","Dateien gelesen"));
            line(1,8,62,w-16,buffer,fg,layout);
            line(2,8,100,w-16,content::pick(lang,"A hangfeladat szabad","Audio task remains free","Audioaufgabe bleibt frei"),muted,layout);
        }else if(status==SdPrepareState::Failed || status==SdPrepareState::Cancelled){
            line(1,8,62,w-16,status==SdPrepareState::Failed?
              content::pick(lang,"SD hiba vagy üres lista","SD error or empty list","SD Fehler oder leere Liste"):
              content::pick(lang,"SD beolvasás megszakítva","SD indexing cancelled","SD Lesen abgebrochen"),fg,layout);
        }else line(1,8,62,w-16,content::pick(lang,"Folyamatban...","Working...","Bitte warten..."),fg,layout);
        if(toSd)line(6,8,h-32,w-16,content::pick(lang,"Vissza: megszakítás","Back: cancel","Zurück: Abbrechen"),accent,layout);
        return;
    }

    if(page==44){
        line(0,8,4,w-16,content::pageTitle(lang,page),accent,layout);
        const unsigned saved=btaudio::savedDeviceCount();
        for(unsigned i=0;i<4;++i){
            btaudio::Device d{};
            if(i<saved&&btaudio::savedDevice(i,d))
                snprintf(buffer,sizeof(buffer),"%s%u. %s",selected==i?"> ":"  ",i+1,d.name);
            else snprintf(buffer,sizeof(buffer),"  %u. --",i+1);
            line(i+1,8,36+i*30,w-16,buffer,selected==i?accent:i<saved?fg:muted,layout);
        }
        snprintf(buffer,sizeof(buffer),"%s%s",selected==4?"> ":"  ",
            content::pick(lang,"Eszköz keresése...","Search for devices...","Geräte suchen..."));
        line(5,8,36+4*30,w-16,buffer,selected==4?accent:fg,layout);
        line(7,8,h-30,w-16,btaudio::connecting()||btaudio::linkFailed()?btaudio::status():"",
            btaudio::linkFailed()?0xF800:accent,layout);
        return;
    }

    if(page==40){
        // Deliberately only the list and scan indicator; no heading or help text.
        static int shownScan=-1;
        bool scan=btaudio::scanning();
        const bool waiting=btaudio::connecting();
        const int marker=scan?1:waiting&&((millis()/400)&1)?2:0;
        if(layout||shownScan!=marker){
            lcd.fillRect(w-28,4,24,24,bg);
            if(marker==1)lcd.fillCircle(w-16,16,7,0x07E0);
            else if(marker==2)lcd.fillCircle(w-16,16,7,0xFFE0);
            else lcd.drawCircle(w-16,16,7,fg);
            shownScan=marker;
        }
        unsigned n=btaudio::deviceCount(),rows=unsigned((h-36)/26);
        unsigned first=selected>=rows/2?selected-rows/2:0;
        if(n>rows&&first>n-rows)first=n-rows;
        for(unsigned row=0;row<rows;++row){
            unsigned i=first+row;btaudio::Device d{};
            if(btaudio::device(i,d))snprintf(buffer,sizeof(buffer),"%s%u. %s",selected==i?(btaudio::linkFailed()?"! ":"> "):"  ",i+1,d.name);
            else buffer[0]=0;
            line(1+row,8,36+row*26,w-16,buffer,selected==i?(btaudio::linkFailed()?0xF800:accent):fg,layout);
        }
        return;
    }
    if(page==39){renderBtSpeaker(s,layout);return;}

    if(showStress)snprintf(buffer,sizeof(buffer),"SELFTEST %u/%u ms",unsigned(stressAction),unsigned(stressDraw));
    if(!content::isMenuPage(page))line(0,8,4,content::isMenuPage(page)?w-100:w-16,showStress?buffer:page==37&&dual::backupRestoreArmed()?content::pick(lang,"Koppints újra 5 mp-en belül","Tap again within 5 s","Binnen 5 s erneut tippen"):content::pageTitle(lang,page),accent,layout);

    if(page==38){
        const char* operation="";
        switch(backupsd::lastOperation()){
          case backupsd::Operation::SavePlaylist:operation=content::pick(lang,"Állomások mentése","Save stations","Sender sichern");break;
          case backupsd::Operation::RestorePlaylist:operation=content::pick(lang,"Állomások visszaállítása","Restore stations","Sender wiederherstellen");break;
          case backupsd::Operation::SaveWifi:operation=content::pick(lang,"Wi-Fi mentése","Save Wi-Fi","Wi-Fi sichern");break;
          case backupsd::Operation::RestoreWifi:operation=content::pick(lang,"Wi-Fi visszaállítása","Restore Wi-Fi","Wi-Fi wiederherstellen");break;
        }
        line(1,8,48,w-16,operation,accent,layout);
        const char* status="ERROR";
        switch(backupsd::lastResult()){
          case backupsd::Result::Pass:status="PASS";break;
          case backupsd::Result::Busy:status=content::pick(lang,"Stop / NetRádió mód","Stop / Web radio mode","Stopp / Webradio-Modus");break;
          case backupsd::Result::NoCard:status=content::pick(lang,"Nincs SD kártya","No SD card","Keine SD Karte");break;
          case backupsd::Result::NoSource:status=content::pick(lang,"Nincs menthető adat","No source data","Keine Quelldaten");break;
          case backupsd::Result::NoBackup:status=content::pick(lang,"Nincs mentés","No backup","Keine Sicherung");break;
          case backupsd::Result::Full:status=content::pick(lang,"Megtelt a mentési lista","Backup slots full","Sicherungsplätze voll");break;
          case backupsd::Result::Invalid:status=content::pick(lang,"Sérült mentés","Invalid backup","Ungültige Sicherung");break;
          case backupsd::Result::WriteFailed:status=content::pick(lang,"SD írási hiba","SD write failed","SD Schreibfehler");break;
          case backupsd::Result::RestoreFailed:status=content::pick(lang,"Visszaállítási hiba","Restore failed","Wiederherstellung fehlgeschlagen");break;
          default:break;
        }
        line(2,8,80,w-16,status,backupsd::lastResult()==backupsd::Result::Pass?accent:fg,layout);
        line(3,8,120,w-16,backupsd::lastOperation()==backupsd::Operation::SaveWifi?content::pick(lang,"Wi-Fi jelszó az SD-n","Password stored on SD","Passwort auf SD gespeichert"):content::pick(lang,"Adatok: állomások / Wi-Fi","Data: stations / Wi-Fi","Daten: Sender / Wi-Fi"),muted,layout);
        line(7,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }

    if(page==5) {
        snprintf(buffer,sizeof(buffer),"CLK GPIO26 RAW = %d",board::rotaryDebugClk());line(1,8,34,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"DT  GPIO35 RAW = %d",board::rotaryDebugDt());line(2,8,58,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"SW  GPIO4  RAW = %d",board::rotaryDebugSw());line(3,8,82,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"CLK chg %lu  R%lu F%lu",(unsigned long)board::rotaryDebugClkChanges(),(unsigned long)board::rotaryDebugClkRise(),(unsigned long)board::rotaryDebugClkFall());line(4,8,112,w-16,buffer,accent,layout);
        snprintf(buffer,sizeof(buffer),"DT  chg %lu  R%lu F%lu",(unsigned long)board::rotaryDebugDtChanges(),(unsigned long)board::rotaryDebugDtRise(),(unsigned long)board::rotaryDebugDtFall());line(5,8,136,w-16,buffer,accent,layout);
        snprintf(buffer,sizeof(buffer),"SW changes = %lu",(unsigned long)board::rotaryDebugSwChanges());line(6,8,160,w-16,buffer,fg,layout);
        const int d=board::rotaryDebugDirection();
        snprintf(buffer,sizeof(buffer),"CW %lu  CCW %lu  LAST %s",(unsigned long)board::rotaryDebugCW(),(unsigned long)board::rotaryDebugCCW(),d>0?"CW":d<0?"CCW":"NONE");line(7,8,190,w-16,buffer,accent,layout);
        line(8,8,216,w-16,content::pick(lang,"Nyomás: vissza","Press: back","Drücken: zurück"),fg,layout);return;
    }

    if(page==55){
        snprintf(buffer,sizeof(buffer),"%u.%u.%u.%u",dual::dnsDraftOctet(0),dual::dnsDraftOctet(1),dual::dnsDraftOctet(2),dual::dnsDraftOctet(3));
        line(1,8,56,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"%u: %u",dual::dnsEditOctet()+1,dual::dnsDraftOctet(dual::dnsEditOctet()));line(2,8,100,w-16,buffer,accent,layout);
        line(3,8,144,w-16,content::pick(lang,"Forgatás: érték / nyomás: vissza","Turn: value / press: back","Drehen: Wert / drücken: zurück"),muted,layout);
        line(4,8,192,w-16,"     -          OK          +",fg,layout);return;
    }
    if(alarmui::menu(page)&&!setup){
        if(!menuSlices.header){
        bool headerPainted=layout;
        if(layout){clockMenuText(14,9,70,content::pick(lang,"Menü","Menu","Menü"),14);clockMenuText(245,9,72,content::pick(lang,"Vissza","Back","Zurück"),14);lcd.drawFastHLine(0,31,320,clockart::edge);}
        const String ip=WiFi.localIP().toString();
        if(layout||strcmp(rows[0],ip.c_str())){headerPainted=true;lcd.fillRect(87,7,150,23,clockart::bg);clockMenuText(87,9,150,ip.c_str(),14,clockart::fg,clockart::bg,true);strlcpy(rows[0],ip.c_str(),sizeof(rows[0]));}
        if(layout){const char*title=page==59?content::pick(lang,"Ébresztő","Alarm","Wecker"):content::pageTitle(lang,page);unsigned sz=clockart::width(title,20)>300?16:20;clockMenuText(8,38,304,title,sz,clockart::fg,clockart::bg,true);}
        menuSlices.header=true;if(headerPainted)return;
        }
        const unsigned count=content::itemCount(page),first=alarmui::first(selected,count),visible=alarmui::visible(count);
        uiCache->menuFirst=first;
        for(unsigned i=menuSlices.row;i<visible;++i){
            menuSlices.row=i+1;
            unsigned item=first+i;char label[170],value[80]={};content::menuItem(label,sizeof(label),s,page,item);
            if(page==59){
                const auto&a=s.settings.alarm;
                const char* labelsHu[]={"Ébresztés","Időzítés","Hang","Fény","Hangerő","Óralap","Vissza"};const char* labelsEn[]={"Alarm","Schedule","Sound","Light","Volume","Clock face","Back"};const char* labelsDe[]={"Wecker","Zeitplan","Klang","Licht","Lautstärke","Zifferblatt","Zurück"};auto labels=unsigned(lang)==0?labelsHu:unsigned(lang)==1?labelsEn:labelsDe;strlcpy(label,labels[item],sizeof(label));
                if(item==0)strlcpy(value,a.mode==0?content::pick(lang,"Ki","Off","Aus"):content::pick(lang,"Be","On","Ein"),sizeof(value));
                if(item==1)snprintf(value,sizeof(value),"%02u:%02u–%02u:%02u",a.onHour,a.onMinute,a.offHour,a.offMinute);
                if(item==2)strlcpy(value,a.source?"SD":content::pick(lang,"Rádió","Radio","Radio"),sizeof(value));
                if(item==3)snprintf(value,sizeof(value),"%u → %u%%",a.lightStart,a.lightPeak);
                if(item==4)snprintf(value,sizeof(value),"%u → %u%%",a.volumeStart,a.volumePeak);
            }else{char*split=strstr(label,": ");if(split){*split=0;strlcpy(value,split+2,sizeof(value));}}
            char key[250];snprintf(key,sizeof(key),"%u/%u/%s/%s",item,unsigned(selected==item),label,value);
            if(!layout&&!strcmp(rows[i+2],key))continue;strlcpy(rows[i+2],key,sizeof(rows[i+2]));
            int y=alarmui::rowTop+i*alarmui::rowHeight;lcd.fillRect(8,y,300,44,clockart::bg);lcd.fillRoundRect(10,y,292,40,5,clockart::card);if(selected==item)lcd.drawRoundRect(10,y,292,40,5,clockart::accent);
            if(page==59&&item==0){clockMenuText(22,y+9,180,label,16,clockart::fg,clockart::card);lcd.fillRoundRect(220,y+5,70,26,13,s.settings.alarm.mode?clockart::accent:clockart::edge);clockMenuText(227,y+10,40,value,14,s.settings.alarm.mode?clockart::bg:clockart::fg,s.settings.alarm.mode?clockart::accent:clockart::edge);lcd.fillCircle(278,y+18,10,clockart::fg);}
            else{int vw=clockart::width(value,14);int lw=clockart::width(label,16);if(lw+vw>245){clockMenuText(20,y+2,270,label,14,clockart::fg,clockart::card);clockMenuText(20,y+20,270,value,14,clockart::fg,clockart::card);}else{clockMenuText(22,y+9,260-vw,label,16,clockart::fg,clockart::card);clockMenuText(281-vw,y+10,vw,value,14,clockart::fg,clockart::card);}if(page==59&&item>0&&item<6)clockMenuText(289,y+10,12,">",14,clockart::fg,clockart::card);}
            return;
        }
        menuSlices.cancel();
        if(count>visible){lcd.fillRoundRect(312,64,4,176,2,clockart::edge);int thumb=176*visible/count;int top=64+int(first)*(176-thumb)/int(count-visible);lcd.fillRoundRect(312,top,4,thumb,2,clockart::fg);}return;
    }
    if(content::isMenuPage(page)){
        if(playerRequested&&s.settings.vintageSkin){
            if(layout&&!menuSlices.header)vintageMenuFrame(content::pageTitle(lang,page),s.settings.vintageSkin,s.settings.retroColor);
            else {bg=s.settings.vintageSkin==6?videotono::glass:s.settings.vintageSkin==5?0x0863:s.settings.vintageSkin==4?0x0021:s.settings.vintageSkin==1?0x0040:s.settings.vintageSkin==2?0x1082:0x3104;accent=vintageAccent(s.settings.vintageSkin,s.settings.retroColor);fg=(s.settings.vintageSkin==1||s.settings.vintageSkin==6)?accent:0xeed8;muted=s.settings.vintageSkin==1?retrocolor::dim(accent):0x9ccb;}
        }else if(playerRequested&&false){
            bg=0x0841;fg=0xc638;accent=0x05ff;muted=0x8410;
            
        }else if(playerRequested&&s.settings.mediaPlayerSkin){
            bg=0xc638;fg=0x1082;accent=0x0258;muted=0x8410;
            if(layout&&!menuSlices.header)mediaMenuFrame(content::pageTitle(lang,page));
        }else if(playerRequested||cassetteRequested){
            bg=cassetteRequested?0x2104:0x1083;fg=cassetteRequested?0xeedb:0x9fe0;
            accent=cassetteRequested?0xfcc8:0xffe0;muted=0x8410;
            if(layout&&!menuSlices.header)skinMenuFrame(content::pageTitle(lang,page),cassetteRequested);
        }
        if(!menuSlices.header){
        bool headerPainted=layout;
        // One native font and baseline for Menu / IPv4 / Back. Only the
        // cached address changing redraws the header between page changes.
        static char menuHeaderIp[48]={};
        const char* headerIp=s.ip[0]?s.ip:"--";
        if(layout||strcmp(menuHeaderIp,headerIp)!=0){
            headerPainted=true;
            snprintf(menuHeaderIp,sizeof(menuHeaderIp),"%s",headerIp);
            const bool vt=playerRequested&&s.settings.vintageSkin==6;
            const uint16_t headerBg=vt?0xbdf7:playerRequested&&s.settings.mediaPlayerSkin?0x0258:bg;
            const uint16_t headerFg=vt?0x1082:playerRequested&&s.settings.mediaPlayerSkin?0xffff:fg;
            lcd.fillRect(7,5,w-14,25,headerBg);
            if(vt){lcd.fillRect(8,5,64,25,0xe71c);lcd.drawRect(8,5,64,25,0x39c7);}else lcd.drawRoundRect(8,5,64,25,3,accent);
            if(vt){lcd.fillRect(w-81,5,74,25,0xe71c);lcd.drawRect(w-81,5,74,25,0x39c7);}else lcd.drawRoundRect(w-81,5,74,25,3,accent);
            textCompactHeight(16,10,52,content::pick(lang,"Menü","Menu","Menü"),headerFg,headerBg,10);
            textCompactHeight(w-75,10,62,content::pick(lang,"Vissza","Back","Zurück"),headerFg,headerBg,10);
            int ipWidth=0;const char* ipCursor=headerIp;
            while(*ipCursor)ipWidth+=fonts::find(btfonts::font10,dual::nextUtf8(ipCursor)).advance;
            const int ipLeft=78,ipRight=w-87;
            textCompactHeight(ipLeft+(ipRight-ipLeft-ipWidth)/2,10,ipRight-ipLeft,headerIp,headerFg,headerBg,10);
        }
        if(showStress){snprintf(buffer,sizeof(buffer),"L%lu K%lu B%luK",(unsigned long)stressActions,(unsigned long)stressFrames,(unsigned long)(s.buffer/1024));headerPainted=line(1,8,30,w-16,buffer,fg,layout)||headerPainted;}
        else {
            snprintf(buffer,sizeof(buffer),"%s%s",content::pageTitle(lang,page),s.pendingSave?" *":"");
            headerPainted=line(1,8,30,w-16,buffer,fg,layout)||headerPainted;
        }
        menuSlices.header=true;if(headerPainted)return;
        }
        unsigned count=content::itemCount(page);
        const bool alarmMenu=alarmui::menu(page);
        unsigned visible=alarmMenu?alarmui::visible(count):count>8?8:count;
        const unsigned first=alarmMenu?alarmui::first(selected,count):
            menurender::viewport(selected,count,visible,uiCache->menuFirst,layout);
        uiCache->menuFirst=first;
        int rowHeight=alarmMenu?alarmui::rowHeight:count>8?22:(h-60)/int(count);
        for(unsigned i=menuSlices.row;i<visible;++i){
            menuSlices.row=i+1;
            const unsigned item=first+i;
            char label[170];bool enabled=content::menuItem(label,sizeof(label),s,page,item);
            snprintf(buffer,sizeof(buffer),"%s%s",selected==item?"> ":"  ",label);
            uint16_t color=enabled?(selected==item?accent:fg):muted;
            if(alarmMenu){
                if(!layout&&!strcmp(rows[i+2],buffer)&&uiCache->rowColor[i+2]==color)continue;
                dual::textCopy(rows[i+2],sizeof(rows[i+2]),buffer);uiCache->rowColor[i+2]=color;
                lcd.fillRect(8,53+i*rowHeight,w-24,rowHeight,bg);
                char *split=strstr(label,": ");
                if(split){*split=0;playerListText(15,56+i*rowHeight,w-37,label,color,bg);playerListText(15,75+i*rowHeight,w-37,split+2,fg,bg);}
                else playerListText(15,65+i*rowHeight,w-37,label,color,bg);
            }else if(!line(i+2,8,53+i*rowHeight,w-24,buffer,color,layout))continue;
            lcd.drawFastHLine(9,53+i*rowHeight+(alarmMenu?42:21),w-28,selected==item?accent:bg);
            return;
        }
        menuSlices.cancel();
        if(count>visible&&(layout||uiCache->scrollSelected!=selected||uiCache->scrollCount!=count||uiCache->scrollVisible!=visible)){
            lcd.fillRect(w-7,53,3,visible*rowHeight,bg);
            lcd.fillRect(w-7,53+int(selected)*(visible*rowHeight-22)/int(count-1),3,22,accent);
            uiCache->scrollSelected=selected;uiCache->scrollCount=count;uiCache->scrollVisible=visible;
        }
        return;
    }

    if(page==31){
        snprintf(buffer,sizeof(buffer),"%s: %s",content::pick(lang,"Név","Name","Name"),dual::stationSearchQuery());
        line(1,8,31,w-16,buffer,accent,layout);
        static int previousKey=-1;
        if(layout)previousKey=-1;
        auto drawKey=[&](unsigned key){
            int keyW=(w-20)/6,keyH=(h-90)/5;
            int x=10+int(key%6)*keyW,y=65+int(key/6)*keyH;
            lcd.fillRect(x,y,keyW-2,keyH-2,bg);
            uint16_t color=selected==key?accent:fg;
            lcd.drawRoundRect(x,y,keyW-2,keyH-2,4,color);
            char label[5]={};
            if(key<26){label[0]=char('A'+key);}
            else snprintf(label,sizeof(label),"%s",key==26?"SP":key==27?"DEL":key==28?"OK":"X");
            text(x+(key<26?keyW/2-7:3),y+(keyH-20)/2,keyW-5,label,color);
        };
        if(layout){for(unsigned i=0;i<30;i++)drawKey(i);}
        else if(previousKey!=int(selected)){
            if(previousKey>=0)drawKey(unsigned(previousKey));
            drawKey(selected);
        }
        previousKey=int(selected);
        const char* status="";
        switch(dual::stationSearchStatus()){
          case 1:status=content::pick(lang,"Állítsd le a rádiót","Stop radio first","Radio zuerst stoppen");break;
          case 2:status=content::pick(lang,"Lista hibás vagy hiányzik","Playlist missing/invalid","Liste fehlt/ungültig");break;
          case 3:status=content::pick(lang,"Nincs találat","No matches","Keine Treffer");break;
          case 4:status=content::pick(lang,"Válts internet rádióra","Switch to internet radio","Internetradio wählen");break;
          default:if(s.playing)status=content::pick(lang,"Kereséshez állítsd le","Stop to search","Zum Suchen stoppen");break;
        }
        line(15,8,h-24,w-16,status,muted,layout);return;
    }
    if(page==32){
        unsigned total=dual::stationSearchCount();
        snprintf(buffer,sizeof(buffer),"%s: %u  %s",content::pick(lang,"Találat","Results","Treffer"),total,dual::stationSearchQuery());
        line(1,8,30,w-16,buffer,accent,layout);
        unsigned first=selected>=2?selected-2:0;
        if(total>6&&first>total-6)first=total-6;
        for(unsigned row=0;row<6;row++){
            unsigned i=first+row;uint16_t station=dual::stationSearchResult(i);
            if(station){
                char name[170];
                dual::stationSearchResultName(i,name,sizeof(name));
                snprintf(buffer,sizeof(buffer),"%s%u %s",i==selected?"> ":"  ",station,name);
                line(2+row,8,56+row*23,w-16,buffer,i==selected?accent:fg,layout);
            }else line(2+row,8,56+row*23,w-16,"",fg,layout);
        }
        line(10,8,h-25,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==33){
        if(!s.connected)line(1,8,44,w-16,content::pick(lang,"Wi-Fi kapcsolat szükséges","Wi-Fi connection required","Wi-Fi Verbindung nötig"),fg,layout);
        else if(config.emptyFS)line(1,8,44,w-16,content::pick(lang,"Adólista hiányzik","Station list missing","Senderliste fehlt"),fg,layout);
        else {
            snprintf(buffer,sizeof(buffer),"http://%s/",s.ip);
            line(1,8,44,w-16,buffer,accent,layout);
            line(2,8,78,w-16,content::pick(lang,"Böngésző: Playlist Editor","Browser: Playlist Editor","Browser: Playlist Editor"),fg,layout);
            line(3,8,108,w-16,content::pick(lang,"Szerkesztés és mentés","Edit and save","Bearbeiten und speichern"),fg,layout);
            line(4,8,138,w-16,content::pick(lang,"Import / export: CSV","Import / export: CSV","Import / Export: CSV"),fg,layout);
        }
        line(8,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==34){
        line(1,8,42,w-16,content::pick(lang,"Csak leállított lejátszásnál","Playback must be stopped","Wiedergabe muss stoppen"),fg,layout);
        line(2,8,72,w-16,content::pick(lang,"Érintés: teljes HiMem próba","Tap: full HiMem check","Tippen: voller HiMem Test"),accent,layout);
        const auto& m=v027::memoryReport();
        const char* status="KÉSZ";
        switch(dual::memoryStressStatus()){
          case 1:status="PASS";break;
          case 2:status="FAIL / NOT_RUN";break;
          case 3:status="LIMITER: BUSY / LOW HEAP";break;
          case 4:status="STOP RADIO FIRST";break;
        }
        snprintf(buffer,sizeof(buffer),"%s: %s",content::pick(lang,"Eredmény","Result","Ergebnis"),status);
        line(3,8,110,w-16,buffer,dual::memoryStressStatus()==1?accent:fg,layout);
        snprintf(buffer,sizeof(buffer),"HiMem %u / %u KiB",unsigned(m.highTested/1024),unsigned(m.highTotal/1024));
        line(4,8,140,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"Errors: %lu",(unsigned long)m.errors);
        line(5,8,170,w-16,buffer,fg,layout);
        line(8,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }

    if(page==4&&playerRequested){playerStationList(s,selected,layout);return;}
    if((page==4||page==29)&&s.settings.vintageSkin){
        if(layout)vintageMenuFrame(s.sd?"SD - Lista":"Rádió - Lista",s.settings.vintageSkin,s.settings.retroColor);
        else {bg=s.settings.vintageSkin==6?videotono::glass:s.settings.vintageSkin==5?0x0863:s.settings.vintageSkin==4?0x0021:s.settings.vintageSkin==1?0x0040:s.settings.vintageSkin==2?0x1082:0x3104;accent=vintageAccent(s.settings.vintageSkin,s.settings.retroColor);fg=(s.settings.vintageSkin==1||s.settings.vintageSkin==6)?accent:0xeed8;}
    }else if((page==4||page==29)&&cassetteRequested){
        bg=0x2104;fg=0xeedb;accent=0xfcc8;muted=0x8410;
        if(layout)skinMenuFrame(s.sd?"SD - Playlist":"Radio - Playlist",true);
    }
    if(page==4||page==29) {
        bool favorites=page==29;uint16_t total=favorites?dual::favoritesCount():config.playlistLength();
        if(favorites)snprintf(buffer,sizeof(buffer),"%s: %u  #%u",content::pick(lang,"Kedvencek","Favorites","Favoriten"),total,selected);
        else snprintf(buffer,sizeof(buffer),"%s %u / %u",s.sd?content::pick(lang,"Fájl","File","Datei"):content::pick(lang,"Állomás","Station","Sender"),selected,total);
        line(1,8,30,w-16,buffer,accent,layout);
        if(total) {
            int first=int(selected)-3;uint16_t favStation=uint16_t(selected);
            for(int i=0;i<7&&(!favorites||i<total);i++) {
                int n;
                if(favorites){n=favStation;favStation=dual::nextFavorite(favStation,true);}
                else {n=first+i;while(n<1)n+=total;while(n>total)n-=total;}
                const char *name=config.stationByNum((uint16_t)n);
                snprintf(buffer,sizeof(buffer),"%s%u%s %s",n==(int)selected?"> ":"  ",n,!s.sd&&dual::favorite((uint16_t)n)?"*":"",name?name:"");
                line(2+i,8,56+i*23,w-16,buffer,n==(int)selected?accent:fg,layout);
            }
            lcd.fillRect(w-7,55,3,160,bg);
            lcd.fillRect(w-7,55+(total>1?int((selected-1)*138/(total-1)):0),3,22,accent);
            if(favorites)for(unsigned i=std::min<unsigned>(total,7);i<7;++i)line(2+i,8,56+i*23,w-16,"",fg,layout);
        } else {for(unsigned i=1;i<7;++i)line(2+i,8,56+i*23,w-16,"",fg,layout);line(2,8,56,w-16,s.sd?content::pick(lang,"Nincs MP3/FLAC fájl","No MP3/FLAC files","Keine MP3/FLAC-Dateien"):content::pick(lang,favorites?"Nincs kedvenc":"Nincs állomáslista",favorites?"No favorites":"No station list",favorites?"Keine Favoriten":"Keine Senderliste"),fg,layout);}
        return;
    }

    if(page==2) {
        const auto& m=v027::memoryReport();
        snprintf(buffer,sizeof(buffer),"IP %s",s.ip);line(1,8,32,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"PSRAM chip: %u KiB",unsigned(m.physical/1024));line(2,8,56,w-16,buffer,accent,layout);
        snprintf(buffer,sizeof(buffer),"Heap %u / %u KiB used",unsigned((ESP.getPsramSize()-ESP.getFreePsram())/1024),unsigned(ESP.getPsramSize()/1024));line(3,8,80,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"HiMem free %u / %u KiB",unsigned(m.highFree/1024),unsigned(m.highTotal/1024));line(4,8,104,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"Map window: %u KiB",unsigned(m.mapWindow/1024));line(5,8,128,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"HiMem test %s %u KiB",m.highResult?m.highResult:"NOT_RUN",unsigned(m.highTested/1024));line(6,8,152,w-16,buffer,accent,layout);
        snprintf(buffer,sizeof(buffer),"Internal free %lu",(unsigned long)s.freeHeap);line(7,8,176,w-16,buffer,fg,layout);
        line(8,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==20){
        line(0,8,20,w-16,"BETA RELEASE",0xf800,layout);
        line(1,8,47,w-16,IBT_FW_VERSION,accent,layout);
        line(2,8,74,w-16,"ESP32 / ST7789 320x240",fg,layout);
        line(3,8,101,w-16,"PCM5102 / I2S",fg,layout);
        const auto& m=v027::memoryReport();
        snprintf(buffer,sizeof(buffer),"PSRAM chip: %u KiB",unsigned(m.physical/1024));line(4,8,128,w-16,buffer,fg,layout);
        line(5,8,155,w-16,"Developed by Kisst\"on\"Tour",fg,layout);
        line(6,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==18){
        snprintf(buffer,sizeof(buffer),"%s: %s",content::pick(lang,"Állapot","Status","Status"),s.connected?content::pick(lang,"csatlakozva","connected","verbunden"):content::pick(lang,"offline","offline","offline"));line(1,8,44,w-16,buffer,s.connected?accent:fg,layout);
        snprintf(buffer,sizeof(buffer),"IP: %s",s.ip);line(2,8,72,w-16,buffer,fg,layout);
        if(s.connected){snprintf(buffer,sizeof(buffer),"RSSI: %d dBm",s.rssi);line(3,8,100,w-16,buffer,fg,layout);}
        snprintf(buffer,sizeof(buffer),"Setup AP: %s",network.setupSsid());line(4,8,128,w-16,buffer,muted,layout);
        line(5,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==21){
        for(unsigned i=0;i<5;i++)line(1+i,8,46+i*28,w-16,i<config.ssidsCount?config.ssids[i].ssid:"",fg,layout);
        line(7,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==28){
        if(s.playing)line(1,8,46,w-16,content::pick(lang,"Állítsd le a rádiót","Stop playback to scan","Radio zum Scannen stoppen"),fg,layout);
        else if(s.freeHeap<48000||s.largestHeap<16000)line(1,8,46,w-16,content::pick(lang,"Kevés szabad memória","Low free memory","Wenig freier Speicher"),fg,layout);
        else {
            const uint8_t phase=network.scanStatus();
            const unsigned count=network.scanCount();
            if(phase==1)line(1,8,46,w-16,content::pick(lang,"Keresés...","Scanning...","Suche..."),fg,layout);
            else if(phase==3)line(1,8,46,w-16,content::pick(lang,"Keresés sikertelen","Scan failed","Suche fehlgeschlagen"),fg,layout);
            else if(!count)line(1,8,46,w-16,content::pick(lang,"Nincs hálózat","No networks found","Keine Netzwerke"),fg,layout);
            else {
                const unsigned visible=5;
                unsigned first=selected>=visible/2?selected-visible/2:0;
                if(count>visible&&first>count-visible)first=count-visible;
                for(unsigned row=0;row<visible;row++){
                    char ssid[WIFI_SSID_CAP];int rssi=0;
                    const unsigned index=first+row;
                    if(index<count&&network.scanEntry(index,ssid,sizeof(ssid),&rssi)){
                        snprintf(buffer,sizeof(buffer),"%c %s  %d dBm",index==selected?'>':' ',ssid,rssi);
                        line(1+row,8,46+row*28,w-16,buffer,index==selected?accent:fg,layout);
                    }else line(1+row,8,46+row*28,w-16,"",fg,layout);
                }
            }
        }
        if(layout){lcd.drawRoundRect(8,h-38,106,30,4,accent);text(18,h-32,88,"JELSZO",accent);}
        return;
    }
    if(page==22||page==23){
        line(1,8,48,w-16,page==22?btaudio::status():"PCM5102A / I2S",fg,layout);
        line(2,8,84,w-16,page==22?(btaudio::inputSelected()?"Phone -> PCM5102A":"Radio -> Fej/Ful/Box"):"Fixed audio output",muted,layout);
        line(3,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==24){
        line(1,8,48,w-16,config.emptyFS?"Playlist: missing":"Playlist: present",config.emptyFS?muted:fg,layout);
        static size_t used=0,total=0;
        if(layout){used=SPIFFS.usedBytes()/1024;total=SPIFFS.totalBytes()/1024;}
        snprintf(buffer,sizeof(buffer),"SPIFFS %lu / %lu KiB",(unsigned long)used,(unsigned long)total);
        line(2,8,84,w-16,buffer,fg,layout);
        line(3,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==25){
        auto cache=onlineart::cacheStatus(s.generation);
        snprintf(buffer,sizeof(buffer),"Cover JPEG: %lu bytes",(unsigned long)cache.bytes);line(1,8,48,w-16,buffer,fg,layout);
        snprintf(buffer,sizeof(buffer),"Pending: %u  Failures: %u",cache.pending?1:0,cache.failures);line(2,8,84,w-16,buffer,fg,layout);
        line(3,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==30){
#if FEATURE_SD
        line(1,8,48,w-16,sdman.ready?content::pick(lang,"SD csatolva","SD mounted","SD eingebunden"):content::pick(lang,"SD nincs csatolva","SD not mounted","SD nicht eingebunden"),sdman.ready?accent:muted,layout);
        line(2,8,84,w-16,s.sd?content::pick(lang,"SD lejátszás aktív","SD playback active","SD Wiedergabe aktiv"):content::pick(lang,"Internet rádió aktív","Internet radio active","Internetradio aktiv"),fg,layout);
#endif
        line(3,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==26){
        if(s.connected)snprintf(buffer,sizeof(buffer),"http://%s/emergency",s.ip);
        else snprintf(buffer,sizeof(buffer),"Wi-Fi offline");
        line(1,8,48,w-16,buffer,fg,layout);
        line(2,8,84,w-16,"Upload firmware in browser",muted,layout);
        line(3,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }
    if(page==27){
        line(1,8,44,w-16,s.settings.rotaryMode==1?"Rotary: hangerő":"Rotary: állomás",fg,layout);
        line(2,8,74,w-16,"Nyomás: választ / pause",fg,layout);
        line(3,8,104,w-16,"Hosszú: vissza / lista",fg,layout);
        line(4,8,134,w-16,"Tekercsek: előző / következő",fg,layout);
        line(5,8,h-30,w-16,dual::tr(lang,dual::Text::Back),accent,layout);return;
    }

}
}
#endif
