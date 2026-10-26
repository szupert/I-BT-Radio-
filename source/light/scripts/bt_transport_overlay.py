"""Light-only AVDT submission result observation, without changing buffer ownership."""
def generate(source,output,control):
    assert source.read_bytes()==control.read_bytes(),'AV transport SDK version changed'
    s=source.read_text();needle='void bta_av_data_path (tBTA_AV_SCB *p_scb, tBTA_AV_DATA *p_data)'
    assert s.count(needle)==1
    s=s.replace(needle,'''#include "dual/bt_transport_probe.h"
extern void ibt_sbc_transport_note(unsigned failed);
/* Single immutable first-session SBC frame. No heap, printf, or callback wait.
 * Capture after one second of RTP time, beyond initial prefill/fade silence.
 * This is pre-AVDT payload evidence, never claimed as over-air delivery. */
static uint32_t ibt_tx_fields[IBT_TX_FIELDS];
uint32_t ibt_tx_get(unsigned f){return f<IBT_TX_FIELDS?__atomic_load_n(&ibt_tx_fields[f],__ATOMIC_ACQUIRE):0;}
static void ibt_tx_add(unsigned f,unsigned n){__atomic_fetch_add(&ibt_tx_fields[f],n,__ATOMIC_RELAXED);}
void ibt_tx_l2_result(unsigned r){ibt_tx_add(r==1?IBT_L2_OK:r==2?IBT_L2_CONGESTED:IBT_L2_FAILED,1);}
unsigned ibt_tx_hci_before(const uint8_t *d,unsigned n){
 ibt_tx_add(IBT_ACL_TX,1);const ibt_acl_view v=ibt_inspect_acl(d,n);
 if(v.kind==2){ibt_tx_add(IBT_BAD_FRAME,1);return 0;}
 if(v.kind!=1&&v.kind!=3)return 0;
 ibt_tx_add(IBT_MEDIA_TX,1);if(v.kind==3)ibt_tx_add(IBT_MEDIA_FRAGMENT,1);
 __atomic_store_n(&ibt_tx_fields[IBT_MEDIA_HANDLE],v.handle,__ATOMIC_RELAXED);
 __atomic_store_n(&ibt_tx_fields[IBT_MEDIA_CID],v.cid,__ATOMIC_RELAXED);
 if(!ibt_tx_get(IBT_FIRST_BYTES)&&v.timestamp>=16000u&&v.first_bytes){
  uint32_t hash=2166136261u;for(unsigned i=0;i<v.first_bytes;++i)hash=(hash^v.first[i])*16777619u;
  __atomic_store_n(&ibt_tx_fields[IBT_FIRST_HASH],hash,__ATOMIC_RELAXED);
  __atomic_store_n(&ibt_tx_fields[IBT_FIRST_TIME],v.timestamp,__ATOMIC_RELAXED);
  __atomic_store_n(&ibt_tx_fields[IBT_FIRST_BYTES],v.first_bytes,__ATOMIC_RELEASE);
 }
 return v.kind;
}
void ibt_tx_hci_after(unsigned kind,unsigned written,unsigned expected){
 if(kind==1||kind==3)ibt_tx_add(written==expected?IBT_MEDIA_SUBMITTED:IBT_MEDIA_SHORT,1);
}
void ibt_tx_completed(const uint8_t *d,unsigned n){
 if(!ibt_tx_get(IBT_MEDIA_TX)||!d||n<3||d[0]!=0x13||d[1]+2u!=n||3u+4u*d[2]>n)return;
 for(unsigned i=0;i<d[2];++i){const uint8_t *p=d+3+4*i;
  if(((p[0]|((unsigned)p[1]<<8))&4095)==ibt_tx_get(IBT_MEDIA_HANDLE))ibt_tx_add(IBT_ACL_COMPLETED,p[2]|((unsigned)p[3]<<8));
 }
}
static UINT8 ibt_wire_frame[128];
static unsigned ibt_wire_length;
unsigned ibt_sbc_wire_byte(unsigned index) {
    const unsigned length=__atomic_load_n(&ibt_wire_length,__ATOMIC_ACQUIRE);
    return index==128 ? length : index<length ? ibt_wire_frame[index] : 0;
}
static void ibt_wire_capture(const BT_HDR *packet, UINT32 timestamp) {
    if (__atomic_load_n(&ibt_wire_length,__ATOMIC_RELAXED)||timestamp<16000u||packet->len<5)return;
    const UINT8 *payload=(const UINT8 *)(packet+1)+packet->offset;
    /* No SCMS-T in this pinned build. MPF1 frame count then standard SBC. */
    if ((payload[0]&0xf0u)||!(payload[0]&15u)||payload[1]!=0x9c)return;
    const UINT8 *f=payload+1;
    const unsigned bands=(f[1]&1u)?8u:4u, blocks=4u*(((f[1]>>4)&3u)+1u);
    const unsigned mode=(f[1]>>2)&3u, channels=mode?2u:1u;
    const unsigned bits=blocks*f[2]*(mode==1?2u:1u)+(mode==3?bands:0u);
    const unsigned length=4u+bands*channels/2u+(bits+7u)/8u;
    if(length>sizeof(ibt_wire_frame)||length+1u>packet->len)return;
    memcpy(ibt_wire_frame,f,length);
    __atomic_store_n(&ibt_wire_length,length,__ATOMIC_RELEASE);
}
'''+needle,1)
    needle='            AVDT_WriteReqOpt(p_scb->avdt_handle, p_buf, timestamp, m_pt, opt);'
    assert s.count(needle)==1
    s=s.replace(needle,'''            ibt_wire_capture(p_buf, timestamp);
            const UINT16 ibt_result = AVDT_WriteReqOpt(p_scb->avdt_handle, p_buf, timestamp, m_pt, opt);
            /* API-queued result only: not a peer ACK or an air-delivery proof. */
            ibt_sbc_transport_note(ibt_result != AVDT_SUCCESS);''',1)
    if not output.exists() or output.read_text()!=s:output.write_text(s,encoding='utf8')
