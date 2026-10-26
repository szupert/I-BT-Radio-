"""R233: request ACL disconnect on BTA owner, retaining completion ACKs.
Apache-2.0 SDK originals retained in sdk_control/r233_*. No COM operations.
"""
from pathlib import Path
import hashlib
FILES={n:'host/bluedroid/bta/av/'+n for n in ('bta_av_api.c','bta_av_act.c')}
def transform(name,s):
 if name=='bta_av_api.c':
  key='        p_buf->hdr.event = BTA_AV_API_DISCONNECT_EVT;';assert s.count(key)==1
  s=s.replace(key,key+'\n        p_buf->hdr.layer_specific = 0; // standard signalling close')
  a=s.index('void BTA_AvDisconnect(');b=s.index('\n}',a)+2
  extension='''

/* I-BT-Radio Light BT-OUT fallback: message only; BTA owner executes it.
 * A queued request is not completion. Profile/controller teardown still
 * waits for normal DISCONNECTED and deinit callbacks. */
int ibt_bt_disconnect_acl(BD_ADDR bd_addr)
{
    tBTA_AV_API_DISCNT *p_buf = (tBTA_AV_API_DISCNT *)osi_calloc(sizeof(tBTA_AV_API_DISCNT));
    if (p_buf == NULL) return 0;
    p_buf->hdr.event = BTA_AV_API_DISCONNECT_EVT;
    p_buf->hdr.layer_specific = 0x4933;
    bdcpy(p_buf->bd_addr, bd_addr);
    bta_sys_sendmsg(p_buf);
    return 1;
}
'''
  s=s[:b]+extension+s[b:]
 else:
  key='#include "stack/l2c_api.h"';assert s.count(key)==1
  s=s.replace(key,key+'\n#include "btm_int.h" // version-locked btm_remove_acl; BTA owner only')
  key='    AVDT_DisconnectReq(p_data->api_discnt.bd_addr, bta_av_conn_cback);';assert s.count(key)==1
  s=s.replace(key,key+'''
    /* Normal HCI disconnect after the app's 2s AVDTP close grace.
     * Never destroy profiles/control blocks or synthesize completion here. */
    if (p_data->hdr.layer_specific == 0x4933) {
        tBTM_STATUS status = btm_remove_acl(p_data->api_discnt.bd_addr, BT_TRANSPORT_BR_EDR);
        APPL_TRACE_WARNING("[IBT-LINK-CLOSE] requested status=%u completion=WAIT_HCI", (unsigned)status);
    }''')
 return s

def apply(sdk,control):
 proof={}
 for name,relative in FILES.items():
  original=(control/('r233_'+name)).read_text(encoding='utf8');patched=transform(name,original);p=sdk/relative;actual=p.read_text(encoding='utf8')
  if actual not in (original,patched):raise RuntimeError('Unexpected SDK source; refusing patch '+str(p))
  if actual!=patched:p.write_text(patched,encoding='utf8')
  proof[relative]={'original_sha256':hashlib.sha256(original.encode()).hexdigest(),'patched_sha256':hashlib.sha256(patched.encode()).hexdigest()}
 return proof

def restore(sdk,control):
    for name,relative in FILES.items():
        original=(control/('r233_'+name)).read_text(encoding='utf8')
        p=sdk/relative;actual=p.read_text(encoding='utf8')
        if actual not in (original,transform(name,original)):
            raise RuntimeError('Unexpected SDK source; refusing restore '+str(p))
        if actual!=original:p.write_text(original,encoding='utf8')

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['apply','restore']);args=parser.parse_args()
    project=Path(__file__).resolve().parents[1]
    sdk=project.parents[1]/'TOOLS/platformio/packages/framework-espidf/components/bt'
    globals()[args.action](sdk,project/'scripts/sdk_control')
    print('SDK link-close '+args.action+' complete; no COM/flash')
