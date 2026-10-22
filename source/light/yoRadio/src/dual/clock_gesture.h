#pragma once
#include <stdint.h>
namespace clockgesture {
constexpr bool secondPress(bool pending,uint32_t now,uint32_t shortAt){return pending&&uint32_t(now-shortAt)<=2500;}
constexpr bool menu(bool pending,bool down,bool allowedPage,bool accessAllowed){return pending&&down&&allowedPage&&accessAllowed;}
}
