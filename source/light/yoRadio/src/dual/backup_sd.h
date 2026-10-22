#pragma once
#include <stdint.h>
namespace backupsd {
enum class Operation : uint8_t { SavePlaylist, RestorePlaylist, SaveWifi, RestoreWifi };
enum class Result : uint8_t { None, Pass, Busy, NoCard, NoSource, NoBackup, Full, Invalid, WriteFailed, RestoreFailed };
Result run(Operation operation);
Result lastResult();
Operation lastOperation();
}
