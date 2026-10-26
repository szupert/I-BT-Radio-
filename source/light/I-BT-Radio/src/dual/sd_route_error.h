#pragma once
#include <stdint.h>
namespace sdroute {
enum class Error:uint8_t {None,Empty,Unavailable,ReadIndex,Cancelled,Timeout};
inline Error catalogFailure(bool complete,uint32_t tracks,bool emptyFiles){return complete&&!tracks&&emptyFiles?Error::Empty:Error::ReadIndex;}
inline const char* message(Error error){switch(error){
 case Error::Empty:return "Az SD-lista üres: nincs lejátszható fájl.";
 case Error::Unavailable:return "Az SD-kártya nem érhető el.";
 case Error::Cancelled:return "Az SD-beolvasás megszakítva.";
 case Error::Timeout:return "Az SD-BT előkészítés időtúllépés miatt leállt.";
 case Error::ReadIndex:return "Nem sikerült beolvasni az SD-listát.";
 default:return "";
}}
}
