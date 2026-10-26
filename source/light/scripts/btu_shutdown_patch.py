"""R225 version-locked SDK fix: stop/join BTU worker before freeing its state.
Original Apache-2.0 sources retained in sdk_control/r225_*; no serial operations.
"""
from pathlib import Path
import hashlib,json
FILES={'thread.c':'common/osi/thread.c','thread.h':'common/osi/include/osi/thread.h','btu_init.c':'host/bluedroid/stack/btu/btu_init.c'}
def transform(name,s):
 if name=='thread.c':
  old='static void osi_thread_stop(osi_thread_t *thread)\n{';assert s.count(old)==1
  s=s.replace(old,'void osi_thread_stop(osi_thread_t *thread)\n{')
  old='    //stop the thread\n';assert s.count(old)==1
  s=s.replace(old,'    // Repeated stop after an acknowledged join must not wait again.\n    if (thread->stop && thread->thread_handle == NULL) return;\n\n'+old)
 elif name=='thread.h':
  old='void osi_thread_free(osi_thread_t *thread);';assert s.count(old)==1
  s=s.replace(old,'// Stop/join worker, retaining queues until timer and core cleanup finishes.\nvoid osi_thread_stop(osi_thread_t *thread);\n'+old)
 else:
  old='void BTU_ShutDown(void)\n{';assert s.count(old)==1
  s=s.replace(old,old+'\n    // R225: queued BTA/HCI events must finish before their control blocks die.\n    // Keep queue objects alive while btu_task_shut_down cancels alarms.\n    if (btu_thread) osi_thread_stop(btu_thread);\n')
 return s
def apply(sdk,control):
 proof={}
 for name,relative in FILES.items():
  original=(control/('r225_'+name)).read_text();patched=transform(name,original);path=sdk/relative;actual=path.read_text()
  if actual not in (original,patched):raise RuntimeError('Unexpected SDK source; refusing patch '+str(path))
  if actual!=patched:path.write_text(patched)
  proof[relative]=hashlib.sha256(path.read_bytes()).hexdigest();print('[IBT-BTU-QUIESCE] '+relative+' sha256='+proof[relative])
 return proof
