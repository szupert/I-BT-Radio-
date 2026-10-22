#include "../core/options.h"
#include "hal.h"
#include "../dual/core.h"
#include <Arduino.h>
#if defined(BOARD_CYD28) && !FEATURE_ADC_BUTTONS
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_heap_caps.h>
#endif
#if defined(BOARD_CYD28) && FEATURE_ENCODER
#include "../dual/rotary_pending.h"
#include "../yoEncoder/yoEncoder.h"
#include "../core/config.h"
#endif
#include <Preferences.h>
#if defined(BOARD_8048S070)
#include <Wire.h>
#endif
namespace board {
using dual::Action;
static dual::Quadrature encoder;
static dual::Debounce keys[11];
static int encoderLastClk=HIGH;
static int debugLastDt=HIGH, debugLastSw=HIGH;
static uint32_t encoderLastEdgeMs=0;
static uint32_t debugClkChanges=0, debugDtChanges=0, debugSwChanges=0;
static uint32_t debugClkRise=0, debugClkFall=0, debugDtRise=0, debugDtFall=0;
static uint32_t debugCW=0, debugCCW=0;
static int debugDirection=0; // +1 CW, -1 CCW, 0 none
static uint32_t debugLastHeartbeat=0;
static bool touchOK=true, mcpOK=false;
static uint32_t pushAt=0;static bool longSent=false;
static uint32_t lastPoll=0,lastTouch=0;
static dual::Point lastPoint;static bool lastDown=false;
static void button(unsigned index,bool pressed,uint32_t now,Action a,void(*emit)(Action)) {
    bool changed=keys[index].update(pressed,now);
    if(index==0) {
        if(changed&&keys[0].value()) {pushAt=now;longSent=false;emit(Action::EncoderDown);}
        if(keys[0].value()&&!longSent&&uint32_t(now-pushAt)>=800) {longSent=true;emit(Action::EncoderLong);}
        if(changed&&!keys[0].value()&&!longSent)emit(Action::EncoderShort);
    } else if(changed&&keys[index].value())emit(a);
}
void brightness(uint8_t p) {
    const uint8_t duty=(uint8_t)map(p,0,100,0,255);
    if(!ledcWrite(BRIGHTNESS_PIN,duty)){
        // The SMALL frontend can request light before the legacy config path.
        if(ledcAttach(BRIGHTNESS_PIN,20000,8))ledcWrite(BRIGHTNESS_PIN,duty);
        else digitalWrite(BRIGHTNESS_PIN,duty?HIGH:LOW);
    }
}
// This input is shared physically with the CYD LDR. The menu changes only
// interpretation; the LDR branch MUST be disconnected for battery wiring.
bool sensorSample(uint8_t mode,uint32_t now,uint16_t &mv) {
#if defined(BOARD_CYD28) && !FEATURE_ADC_BUTTONS
    static uint32_t last=0; static uint16_t filtered=0;
    static uint8_t previous=255; static bool valid=false;
    static esp_adc_cal_characteristics_t *cal=nullptr;
    static_assert(DUAL_ADC_PIN==34,"SMALL sensor must remain GPIO34/ADC1_CH6");
    if(mode!=previous){previous=mode;valid=false;last=now-2000;}
    if(mode==0||mode>2){mv=0;return false;}
    if(uint32_t(now-last)>=2000){
        last=now;
        // Audio's legacy I2S brings legacy ADC into the image. Never link
        // Arduino's new oneshot ADC beside it: the IDF constructor aborts at boot.
        if(!cal){
            auto *candidate=(esp_adc_cal_characteristics_t*)heap_caps_calloc(1,sizeof(esp_adc_cal_characteristics_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(!candidate){mv=0;valid=false;return false;}
            if(adc1_config_width(ADC_WIDTH_BIT_12)!=ESP_OK||adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_DB_12)!=ESP_OK){free(candidate);mv=0;valid=false;return false;}
            const auto kind=esp_adc_cal_characterize(ADC_UNIT_1,ADC_ATTEN_DB_12,ADC_WIDTH_BIT_12,1100,candidate);
            cal=candidate;Serial.printf("[ADC1-SENSOR] driver=LEGACY pin=34 channel=6 calibration=%u default_vref=%u\n",unsigned(kind),kind==ESP_ADC_CAL_VAL_DEFAULT_VREF?1:0);
        }
        int ra=adc1_get_raw(ADC1_CHANNEL_6),rb=adc1_get_raw(ADC1_CHANNEL_6),rc=adc1_get_raw(ADC1_CHANNEL_6);
        if(ra<0||rb<0||rc<0){mv=0;filtered=0;valid=false;return false;}
        uint32_t a=esp_adc_cal_raw_to_voltage(ra,cal), b=esp_adc_cal_raw_to_voltage(rb,cal), c=esp_adc_cal_raw_to_voltage(rc,cal);
        if(a>b){uint32_t t=a;a=b;b=t;}if(b>c){uint32_t t=b;b=c;c=t;}if(a>b){uint32_t t=a;a=b;b=t;}
        // Reject out-of-range voltage before filtering; do not hide it with EMA.
        if(b>2450 || (mode==2 && b<150)){valid=false;filtered=0;}
        else {filtered=valid?uint16_t((uint32_t(filtered)*3+b+2)/4):uint16_t(b);valid=true;}
    }
    mv=filtered;return valid;
#else
    (void)mode;(void)now;mv=0;return false;
#endif
}
bool touchAvailable(){return touchOK;}
bool expanderAvailable(){return mcpOK;}
#if defined(BOARD_CYD28)
#if FEATURE_ENCODER
// Original yoRadio decoder: both phase interrupts, quadrature and acceleration.
static yoEncoder nativeEncoder(DUAL_ENCODER_A,DUAL_ENCODER_B,4,false);
static dual::RotaryPending rotaryPending;
static void IRAM_ATTR rotaryEdge(){nativeEncoder.readEncoder_ISR();}
#endif
static uint8_t transfer(uint8_t value) {
    uint8_t in=0;
    for(int bit=7;bit>=0;--bit) {
        digitalWrite(DUAL_TOUCH_MOSI,(value>>bit)&1);
        digitalWrite(DUAL_TOUCH_SCK,HIGH);delayMicroseconds(1);
        in=(in<<1)|digitalRead(DUAL_TOUCH_MISO);
        digitalWrite(DUAL_TOUCH_SCK,LOW);delayMicroseconds(1);
    }
    return in;
}
static int sample(uint8_t cmd) {
    digitalWrite(DUAL_TOUCH_CS,LOW); transfer(cmd);
    uint16_t a=transfer(0);a=(a<<8)|transfer(0);
    digitalWrite(DUAL_TOUCH_CS,HIGH);return a>>3;
}
static int median(int a,int b,int c) {return a>b?(b>c?b:(a>c?c:a)):(a>c?a:(b>c?c:b));}
static int median3(int a,int b,int c) {return median(a,b,c);}
static TouchCalibration touchCal={DUAL_TOUCH_XMIN,DUAL_TOUCH_XMAX,DUAL_TOUCH_YMIN,DUAL_TOUCH_YMAX,DUAL_TOUCH_SWAP_XY,DUAL_TOUCH_INVERT_X,DUAL_TOUCH_INVERT_Y};
static bool touchCalValid=false;
static bool rawTouch(dual::Point &p) {
    int pressure=sample(0xb0)+4095-sample(0xc0);if(pressure<=400)return false;
    p.x=median(sample(0xd0),sample(0xd0),sample(0xd0));p.y=median(sample(0x90),sample(0x90),sample(0x90));return true;
}
bool touchRaw(dual::Point &p){return rawTouch(p);}
bool touchCalibrationNeeded(){return !touchCalValid;}
bool touchSaveCalibration(const TouchCalibration &c){
    if(c.xMax-c.xMin<500||c.yMax-c.yMin<500)return false;touchCal=c;Preferences p;if(!p.begin("ibt-touch",false))return false;
    p.putUChar("schema",1);p.putInt("xmin",c.xMin);p.putInt("xmax",c.xMax);p.putInt("ymin",c.yMin);p.putInt("ymax",c.yMax);p.putBool("swap",c.swapXY);p.putBool("invx",c.invertX);p.putBool("invy",c.invertY);p.end();touchCalValid=true;return true;
}
void begin() {
    pinMode(DUAL_TOUCH_CS,OUTPUT);digitalWrite(DUAL_TOUCH_CS,HIGH);
    pinMode(DUAL_TOUCH_SCK,OUTPUT);digitalWrite(DUAL_TOUCH_SCK,LOW);
    pinMode(DUAL_TOUCH_MOSI,OUTPUT);pinMode(DUAL_TOUCH_MISO,INPUT);
    Preferences tp;if(tp.begin("ibt-touch",true)){if(tp.getUChar("schema",0)==1){touchCal.xMin=tp.getInt("xmin",DUAL_TOUCH_XMIN);touchCal.xMax=tp.getInt("xmax",DUAL_TOUCH_XMAX);touchCal.yMin=tp.getInt("ymin",DUAL_TOUCH_YMIN);touchCal.yMax=tp.getInt("ymax",DUAL_TOUCH_YMAX);touchCal.swapXY=tp.getBool("swap",DUAL_TOUCH_SWAP_XY);touchCal.invertX=tp.getBool("invx",DUAL_TOUCH_INVERT_X);touchCal.invertY=tp.getBool("invy",DUAL_TOUCH_INVERT_Y);touchCalValid=(touchCal.xMax-touchCal.xMin>=500&&touchCal.yMax-touchCal.yMin>=500);}tp.end();}
    if(dual::debugEnabled())Serial.printf("[TOUCH] calibration=%s\n",touchCalValid?"NVS":"required");
#if FEATURE_ENCODER
    pinMode(DUAL_ENCODER_A,INPUT_PULLUP); // GPIO26: dedicated rotary CLK, internal pull-up enabled
    pinMode(DUAL_ENCODER_B,INPUT); // GPIO35: no internal pull-up; external module pull-up
    pinMode(DUAL_ENCODER_SW,INPUT_PULLUP);
    encoderLastClk=digitalRead(DUAL_ENCODER_A);
    debugLastDt=digitalRead(DUAL_ENCODER_B);
    debugLastSw=digitalRead(DUAL_ENCODER_SW);
    encoderLastEdgeMs=millis();
    nativeEncoder.begin();nativeEncoder.setBoundaries(-16384,16383,true);
    nativeEncoder.setAcceleration(config.store.encacc);nativeEncoder.readEncoder_ISR();
    nativeEncoder.setup(rotaryEdge);
#endif
#if FEATURE_ADC_BUTTONS
    pinMode(DUAL_ADC_PIN,INPUT);analogSetPinAttenuation(DUAL_ADC_PIN,ADC_11db);
#endif
}
void poll(void(*emit)(Action)) {
    uint32_t now=millis();if(uint32_t(now-lastPoll)<1)return;lastPoll=now;
#if FEATURE_ENCODER
    const int clk=digitalRead(DUAL_ENCODER_A);
    const int dt=digitalRead(DUAL_ENCODER_B);
    const int sw=digitalRead(DUAL_ENCODER_SW);

    if(clk!=encoderLastClk) {
        debugClkChanges++;
        if(clk) debugClkRise++; else debugClkFall++;
    }
    if(dt!=debugLastDt) {
        debugDtChanges++;
        if(dt) debugDtRise++; else debugDtFall++;
    }
    if(sw!=debugLastSw) debugSwChanges++;

    static unsigned lastPage=0;
    static uint32_t lastMenuStep=0;
    const unsigned currentPage=dual::r10DiagPage();
    if(currentPage!=lastPage){rotaryPending.pending=0;lastMenuStep=0;lastPage=currentPage;}
    nativeEncoder.setAcceleration(currentPage==0?config.store.encacc:0);
    rotaryPending.add(nativeEncoder.encoderChanged());
    // Keep volume responsive, but pace menu rows and retain a few detents.
    // BT-IN player page 39 uses the knob for volume, so it stays unthrottled.
    const bool menuSteps=currentPage!=0&&currentPage!=39;
    if(menuSteps){
        if(rotaryPending.pending>3)rotaryPending.pending=3;
        if(rotaryPending.pending<-3)rotaryPending.pending=-3;
    }
    const unsigned budget=menuSteps?(uint32_t(now-lastMenuStep)>=75?1:0):2;
    for(unsigned i=0;i<budget;i++) {
        int move=rotaryPending.take();if(!move)break;
        if(menuSteps)lastMenuStep=now;
        if(move>0){++debugCW;debugDirection=1;}else{++debugCCW;debugDirection=-1;}
        emit(move>0?Action::RotCW:Action::RotCCW);
    }

    if(uint32_t(now-debugLastHeartbeat)>=1000) {
        debugLastHeartbeat=now;
        if(dual::debugEnabled())Serial.printf("[ROTARY-RAW] CLK=%d DT=%d SW=%d clkchg=%lu dtchg=%lu swchg=%lu cw=%lu ccw=%lu last=%s\n",
          clk,dt,sw,
          (unsigned long)debugClkChanges,(unsigned long)debugDtChanges,(unsigned long)debugSwChanges,
          (unsigned long)debugCW,(unsigned long)debugCCW,
          debugDirection>0?"CW":debugDirection<0?"CCW":"NONE");
    }

    encoderLastClk=clk;
    debugLastDt=dt;
    debugLastSw=sw;
    button(0,!sw,now,Action::Select,emit);
#endif
#if FEATURE_ADC_BUTTONS
    static dual::Debounce adc;
    static Action held=Action::None;
    static uint32_t pressedAt=0,repeatedAt=0,lastAdcPoll=0;
    if(uint32_t(now-lastAdcPoll)>=10) {
        lastAdcPoll=now;
        int mv=median3(analogReadMilliVolts(DUAL_ADC_PIN),
                       analogReadMilliVolts(DUAL_ADC_PIN),
                       analogReadMilliVolts(DUAL_ADC_PIN));
        Action raw=dual::adcKey(mv);
        if(adc.update(int(raw),now)) {
            Action stable=Action(adc.value());
            if(stable!=Action::None) {
                emit(stable);
                held=stable;
                pressedAt=repeatedAt=now;
            } else held=Action::None;
        }
        if(dual::repeatable(held)
           &&uint32_t(now-pressedAt)>=450
           &&uint32_t(now-repeatedAt)>=120) {
            repeatedAt=now;
            emit(held);
        }
    }
#endif
}
int rotaryDebugClk(){return digitalRead(DUAL_ENCODER_A);}
int rotaryDebugDt(){return digitalRead(DUAL_ENCODER_B);}
int rotaryDebugSw(){return digitalRead(DUAL_ENCODER_SW);}
uint32_t rotaryDebugClkChanges(){return debugClkChanges;}
uint32_t rotaryDebugDtChanges(){return debugDtChanges;}
uint32_t rotaryDebugSwChanges(){return debugSwChanges;}
uint32_t rotaryDebugClkRise(){return debugClkRise;}
uint32_t rotaryDebugClkFall(){return debugClkFall;}
uint32_t rotaryDebugDtRise(){return debugDtRise;}
uint32_t rotaryDebugDtFall(){return debugDtFall;}
uint32_t rotaryDebugCW(){return debugCW;}
uint32_t rotaryDebugCCW(){return debugCCW;}
int rotaryDebugDirection(){return debugDirection;}

bool touch(dual::Point &p) {
    uint32_t now=millis();if(uint32_t(now-lastTouch)<20){p=lastPoint;return lastDown;}lastTouch=now;dual::Point raw;lastDown=rawTouch(raw);
    if(lastDown){int x=raw.x,y=raw.y;if(touchCal.swapXY){int z=x;x=y;y=z;}x=constrain(map(x,touchCal.xMin,touchCal.xMax,0,319),0,319);y=constrain(map(y,touchCal.yMin,touchCal.yMax,0,239),0,239);if(touchCal.invertX)x=319-x;if(touchCal.invertY)y=239-y;lastPoint={x,y};}
    p=lastPoint;return lastDown;
}
#else
static uint8_t gtAddress=0;
static bool readReg(uint8_t address,uint16_t reg,uint8_t *out,size_t n,bool wide) {
    Wire.beginTransmission(address);if(wide)Wire.write(uint8_t(reg>>8));Wire.write(uint8_t(reg));
    if(Wire.endTransmission(false)!=0)return false;
    size_t count=Wire.requestFrom(address,uint8_t(n));
    if(count!=n){while(Wire.available())Wire.read();return false;}
    for(size_t i=0;i<n;i++)out[i]=Wire.read();return true;
}
static bool writeReg(uint8_t addr,uint16_t reg,uint8_t val,bool wide) {
    Wire.beginTransmission(addr);if(wide)Wire.write(uint8_t(reg>>8));Wire.write(uint8_t(reg));Wire.write(val);return Wire.endTransmission()==0;
}
void begin() {
    Wire.begin(DUAL_I2C_SDA,DUAL_I2C_SCL,400000);Wire.setTimeOut(5);
    pinMode(DUAL_TOUCH_RESET,OUTPUT);digitalWrite(DUAL_TOUCH_RESET,LOW);delay(10);
    digitalWrite(DUAL_TOUCH_RESET,HIGH);delay(60);
    for(uint8_t a:{uint8_t(0x5d),uint8_t(0x14)}) {
        uint8_t id[4];if(readReg(a,0x8140,id,4,true)){gtAddress=a;break;}
    }
    touchOK=gtAddress!=0;
#if FEATURE_MCP23017
    // MCP23017 power-on BANK=0, sequential access. All pins inputs with pull-ups.
    mcpOK=writeReg(DUAL_MCP_ADDRESS,0x00,0xff,false)&&writeReg(DUAL_MCP_ADDRESS,0x01,0xff,false)
       &&writeReg(DUAL_MCP_ADDRESS,0x0c,0xff,false)&&writeReg(DUAL_MCP_ADDRESS,0x0d,0xff,false);
#endif
    if(dual::debugEnabled())Serial.printf("[DUAL] GT911=%02x MCP23017=%s\n",gtAddress,mcpOK?"OK":"absent");
}
void poll(void(*emit)(Action)) {
#if FEATURE_MCP23017
    uint32_t now=millis();if(!mcpOK||uint32_t(now-lastPoll)<5)return;lastPoll=now;
    uint8_t value[2];if(!readReg(DUAL_MCP_ADDRESS,0x12,value,2,false))return;
#if FEATURE_ENCODER
    int move=encoder.update(value[0]&3);if(move)emit(move>0?Action::VolUp:Action::VolDown);
    button(0,!(value[0]&4),now,Action::Select,emit);
#endif
    const Action a[]={Action::Prev,Action::Next,Action::Toggle,Action::VolDown,Action::VolUp};
    for(unsigned i=0;i<5;i++)button(i+1,!(value[0]&(1<<(i+3))),now,a[i],emit);
    button(6,!(value[1]&1),now,Action::Source,emit);
    // GPB1 favourite is reserved; GPB2 menu/back.
    button(7,!(value[1]&4),now,Action::Menu,emit);
#else
    (void)emit;
#endif
}
bool touchRaw(dual::Point &p){return touch(p);}
bool touchCalibrationNeeded(){return false;}
bool touchSaveCalibration(const TouchCalibration &){return false;}
bool touch(dual::Point &p) {
    uint32_t now=millis();if(!touchOK)return false;
    if(uint32_t(now-lastTouch)<20){p=lastPoint;return lastDown;}lastTouch=now;
    uint8_t status=0;
    if(!readReg(gtAddress,0x814e,&status,1,true)){lastDown=false;return false;}
    if(status&0x80) {
        lastDown=(status&15)>0;
        if(lastDown) {
            uint8_t xy[5];if(readReg(gtAddress,0x814f,xy,5,true))lastPoint={constrain(int(xy[1]|xy[2]<<8),0,799),constrain(int(xy[3]|xy[4]<<8),0,479)};
            else lastDown=false;
        }
        writeReg(gtAddress,0x814e,0,true);
    }
    p=lastPoint;return lastDown;
}
#endif
}
