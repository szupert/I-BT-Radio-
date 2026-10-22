#pragma once
#include "../dual/core.h"
namespace front {
bool begin();
#if defined(BOARD_CYD28)
bool bootFrame(bool connected,bool setup,bool offline,unsigned focus);
void btClockTick(); // UI owner, throttled; remains active while menus are open.
void bootRestart(uint8_t choice,uint8_t vintage,uint8_t skin);
bool cassetteReady();
void skinPreview(int index);
bool skinPreviewActive();
bool skinPreviewInput(dual::Action action);
bool menuViewport(unsigned page,unsigned &first);
bool renderPending();
#endif
dual::Size nativeSize();
void render(const dual::Snapshot &s,unsigned page,unsigned selected,bool force);
void invalidate();
unsigned stationListFirst();
#if defined(BOARD_CYD28)
void renderBattery(const dual::Snapshot &s);
void artworkTestFast(bool on);
void artworkTestRows(unsigned y,unsigned n,const uint16_t *pixels);
#endif
void service();
#if IBT_NO_PSRAM
void liteMeterTick(bool allowed);
void transportAck(); // latch only; LCD feedback remains on the guarded UI owner
#endif
void setRotation(uint8_t rotation);
void calibrationTarget(int x,int y,unsigned step);
void calibrationDone(bool ok);
bool artworkBegin(int width,int height);
void artworkTile(int x,int y,int width,int height,int stride,const uint16_t *pixels);
void artworkEnd();
void artworkClear();
}
