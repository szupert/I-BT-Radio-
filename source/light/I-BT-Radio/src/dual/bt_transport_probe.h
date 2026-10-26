#pragma once
#include <stdint.h>
#include <stddef.h>
enum { IBT_L2_OK,IBT_L2_CONGESTED,IBT_L2_FAILED,IBT_ACL_TX,IBT_MEDIA_TX,
 IBT_MEDIA_SUBMITTED,IBT_MEDIA_SHORT,IBT_BAD_FRAME,IBT_ACL_COMPLETED,
 IBT_FIRST_HASH,IBT_FIRST_BYTES,IBT_FIRST_TIME,IBT_MEDIA_HANDLE,IBT_MEDIA_CID,
 IBT_MEDIA_FRAGMENT,IBT_TX_FIELDS };
typedef struct {unsigned kind,handle,cid,first_bytes;uint32_t timestamp;const uint8_t *first;} ibt_acl_view;
static inline unsigned ibt_sbc_bytes(const uint8_t *f,size_t n){
 if(n<4||f[0]!=0x9c)return 0;
 const unsigned bands=(f[1]&1)?8:4,blocks=4*(((f[1]>>4)&3)+1),mode=(f[1]>>2)&3,channels=mode?2:1;
 if(f[2]>(mode<=1?16:32)*bands)return 0;
 return 4+bands*channels/2+(blocks*f[2]*(mode==1?2:1)+(mode==3?bands:0)+7)/8;
}
// 0 other ACL, 1 complete SBC RTP, 2 malformed candidate, 3 first fragment.
// Observation only: no ownership, content, offset, or flow-credit changes.
static inline ibt_acl_view ibt_inspect_acl(const uint8_t *d,size_t n){
 ibt_acl_view v={0,0,0,0,0,NULL};
 if(!d||n<4){v.kind=2;return v;}
 v.handle=(d[0]|((unsigned)d[1]<<8))&4095;
 if((unsigned)(d[2]|((unsigned)d[3]<<8))!=n-4){v.kind=2;return v;}
 if(((d[1]>>4)&3)==1||n<8)return v;
 v.cid=d[6]|((unsigned)d[7]<<8);
 if(v.cid<64||n<10||d[8]!=0x80||(d[9]&127)!=96)return v;
 v.kind=2;if(n<25)return v;
 const unsigned l2=d[4]|((unsigned)d[5]<<8),frames=d[20]&15;
 if(l2<n-8||(d[20]&0xf0)||!frames)return v;
 v.timestamp=((uint32_t)d[12]<<24)|((uint32_t)d[13]<<16)|((uint32_t)d[14]<<8)|d[15];
 v.first=d+21;v.first_bytes=ibt_sbc_bytes(v.first,n-21);
 if(!v.first_bytes)return v;
 if(l2>n-8){v.kind=3;if(v.first_bytes>n-21)v.first_bytes=0;return v;}
 size_t offset=21;
 for(unsigned i=0;i<frames;++i){unsigned bytes=ibt_sbc_bytes(d+offset,n-offset);if(!bytes||bytes>n-offset)return v;offset+=bytes;}
 if(offset!=n)return v;
 v.kind=1;return v;
}
#ifdef __cplusplus
extern "C" {
#endif
uint32_t ibt_tx_get(unsigned field);
void ibt_tx_l2_result(unsigned result);
unsigned ibt_tx_hci_before(const uint8_t *data,unsigned length);
void ibt_tx_hci_after(unsigned kind,unsigned written,unsigned expected);
void ibt_tx_completed(const uint8_t *data,unsigned length);
#ifdef __cplusplus
}
#endif
