#pragma once
#include <stdint.h>
#include <esp_avrc_api.h>
namespace btmedia {
struct View { char title[128],artist[96]; uint32_t generation; bool image; };
void event(esp_avrc_ct_cb_event_t event,esp_avrc_ct_cb_param_t* param);
void service(bool active);
View view();
bool row(uint32_t generation,unsigned y,uint16_t* pixels,unsigned width=320,unsigned height=240);
constexpr unsigned side=200;
}
