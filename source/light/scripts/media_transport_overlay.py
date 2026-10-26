"""Pinned Light4 media handoff correction and bounded TX observations."""
import hashlib
LOCKED={'avdt_scb_act':'cd0bde3e48c3cf6ca8e5c7fbd00967e6f47f4d71cd873547b7ceb4cc5302d41b','hci_layer':'f62bd11bde96bc0323d0f3ff1d14bb995930a798530d389e5c87d39cf81d676b'}
def transform(s,kind):
 def once(old,new):
  nonlocal s
  assert s.count(old)==1,(kind,old[:90]);s=s.replace(old,new,1)
 once('#include <string.h>','#include <string.h>\n#include "dual/bt_transport_probe.h"')
 if kind=='avdt_scb_act':
  once('''            avdt_ad_write_req(AVDT_CHAN_MEDIA, p_scb->p_ccb, p_scb, p_pkt);

            (*p_scb->cs.p_ctrl_cback)''','''            const UINT8 ibt_result=avdt_ad_write_req(AVDT_CHAN_MEDIA, p_scb->p_ccb, p_scb, p_pkt);
            /* L2CAP consumes p_pkt for ALL return codes. Never free/retry it.
             * Congested means accepted; failed means not handed to L2CAP. */
            ibt_tx_l2_result(ibt_result);
            if(ibt_result==AVDT_AD_CONGESTED)p_scb->cong=TRUE;
            else if(ibt_result!=AVDT_AD_SUCCESS)avdt_ctrl.hdr.err_code=AVDT_ERR_RESOURCE;

            (*p_scb->cs.p_ctrl_cback)''')
 else:
  once('''    hal->transmit_data(type, packet->data + packet->offset, packet->len);''','''    const unsigned ibt_kind=event==MSG_STACK_TO_HC_HCI_ACL ?
        ibt_tx_hci_before(packet->data + packet->offset,packet->len) : 0;
    const uint16_t ibt_written=hal->transmit_data(type, packet->data + packet->offset, packet->len);
    if(ibt_kind)ibt_tx_hci_after(ibt_kind,ibt_written,packet->len);''')
  once('''static void dispatch_reassembled(BT_HDR *packet)
{''','''static void dispatch_reassembled(BT_HDR *packet)
{
    /* Observe completed ACL counts; RX validation and ownership stay intact. */
    if((packet->event&MSG_EVT_MASK)==MSG_HC_TO_STACK_HCI_EVT)
        ibt_tx_completed(packet->data+packet->offset,packet->len);''')
 return s

def generate(source,output,control,kind):
 raw=source.read_bytes();assert raw==control.read_bytes() and hashlib.sha256(raw).hexdigest()==LOCKED[kind],'SDK version mismatch: '+kind
 s=transform(source.read_text(),kind)
 if not output.exists() or output.read_text()!=s:output.write_text(s,encoding='utf8')
