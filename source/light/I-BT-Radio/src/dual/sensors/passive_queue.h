#pragma once
#include "passive_frame.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace radiosensor {
// Initialized by the owner before worker/GAP registration. No callback allocation.
// Seven owned packets; zero-tick SDK send/receive, full queue drops newest.
class PassiveQueue {
 StaticQueue_t control{};alignas(4) uint8_t storage[7*sizeof(Packet)]{};QueueHandle_t handle=nullptr;
public:
 bool begin(){if(!handle)handle=xQueueCreateStatic(7,sizeof(Packet),storage,&control);return handle!=nullptr;}
 bool push(const Packet&p){return handle&&p.length>0&&p.length<=64&&xQueueSend(handle,&p,0)==pdTRUE;}
 bool pop(Packet&p){return handle&&xQueueReceive(handle,&p,0)==pdTRUE;}
 // Configuration change: bump producer epoch; worker rejects old epochs. Never reset indices concurrently.
};
}
