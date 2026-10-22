#pragma once
#include <cstring>
// Translate system notices only, never artist/title/device names.
namespace front {
inline const char* userMessage(const char* text) {
    if (!text) return "";
    if (std::strstr(text,"Not enough free memory"))
        return "Nincs elég memória a dekóderhez";
    if(std::strstr(text,"Nincs eleg RAM")||std::strstr(text,"not enough memory"))return "Nincs elég memória a dekóderhez";
    if(std::strstr(text,"unknown content")||std::strstr(text,"not supported"))return "Nem támogatott hangformátum";
    if(std::strstr(text,"Failed to open"))return "A hangfájl nem nyitható meg";
    if(std::strstr(text,"Connection failed")||std::strstr(text,"connect failed"))return "Sikertelen csatlakozás";
    if (!std::strcmp(text,"[connecting]")) return "[csatlakozás]";
    if (!std::strcmp(text,"[stopped]")) return "[leállítva]";
    if (!std::strcmp(text,"[ready]")) return "[kész]";
    return text;
}
}
