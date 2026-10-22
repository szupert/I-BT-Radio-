"""IDF 5.5.5: count the recursion guard instead of flooding UART.
The existing early return and all other warnings/errors are unchanged.
"""
from pathlib import Path
import hashlib
ORIGINAL='85a3b5889ba92c758a129e7389ce4a2e2a19c2b3c89814f8f33abbd680f839fe'
def transform(text):
 old='        L2CAP_TRACE_ERROR("l2cab is_cong_cback_context");'
 if text.count(old)!=1:raise RuntimeError('L2CAP guard context mismatch')
 return text.replace(old,'        /* I-BT: preserve guard, count without blocking/log interleaving. */\n        extern void ibt_l2cap_guard_note(void);\n        ibt_l2cap_guard_note();',1)
def apply(sdk,control):
 original=control.read_bytes()
 if hashlib.sha256(original).hexdigest()!=ORIGINAL:raise RuntimeError('L2CAP original hash mismatch')
 source=original.decode('utf8').replace('\r\n','\n');patched=transform(source)
 actual=sdk.read_text(encoding='utf8')
 if actual not in (source,patched):raise RuntimeError('Unknown L2CAP SDK source; build refused')
 if actual!=patched:sdk.write_text(patched,encoding='utf8')
 print('[IBT-L2CAP-GUARD] '+hashlib.sha256(sdk.read_bytes()).hexdigest())
