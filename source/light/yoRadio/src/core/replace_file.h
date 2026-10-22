#pragma once
#include <string.h>

namespace ibtfile {
template<class FS> bool recover(FS& fs, const char* target, const char* previous) {
    if (fs.exists(target) || !fs.exists(previous)) return true;
    return fs.rename(previous, target);
}

template<class FS> bool replace(FS& fs, const char* staged, const char* target, const char* previous) {
    if (!staged || !target || !previous || !strcmp(staged, target) || !fs.exists(staged)) return false;
    // SPIFFS refuses rename onto an existing name. Keep the old file until
    // the staged file is in place; recover it at boot after a power loss.
    if (fs.exists(previous)) {
        if (!recover(fs, target, previous)) return false;
        if (fs.exists(previous) && !fs.remove(previous)) return false;
    }
    bool hadPrevious = fs.exists(target);
    if (hadPrevious && !fs.rename(target, previous)) return false;
    if (!fs.rename(staged, target)) {
        if (hadPrevious) fs.rename(previous, target);
        return false;
    }
    if (hadPrevious) fs.remove(previous);
    return true;
}

// Keep the previous copy until a dependent file (the playlist index) has
// also been committed. On reset, recover() restores it if target is absent.
template<class FS> bool replaceKeepPrevious(FS& fs, const char* staged, const char* target, const char* previous) {
    if (!staged || !target || !previous || !strcmp(staged, target) || !fs.exists(staged)) return false;
    if (fs.exists(previous)) {
        if (!recover(fs, target, previous)) return false;
        if (fs.exists(previous) && !fs.remove(previous)) return false;
    }
    bool hadPrevious = fs.exists(target);
    if (hadPrevious && !fs.rename(target, previous)) return false;
    if (!fs.rename(staged, target)) {
        if (hadPrevious) fs.rename(previous, target);
        return false;
    }
    return true;
}
}
