"""Pinned Light-only SDP outbound request buffers; incoming response limit unchanged."""
import hashlib
LOCKED='c5c8b9f56b8c617546c0e880609e0fe84a79d4b0cc0f8b76d790e273ef3c9503'
def transform(s):
 helper='''/* R251: bound outbound wire builders including all transport prepend bytes.
 * UUID entries <=17B, attributes <=3B each plus sequence header; include
 * wildcard, record handle and maximum continuation. Response storage untouched. */
static size_t ibt_sdp_request_bytes(unsigned uuids, unsigned attrs)
{
    const size_t bytes = sizeof(BT_HDR) + L2CAP_MIN_OFFSET + 5u + 2u + 4u +
        2u + (size_t)uuids * 17u + (attrs ? 3u + (size_t)attrs * 3u : 7u) +
        1u + SDP_MAX_CONTINUATION_LEN;
    return bytes <= SDP_DATA_BUF_SIZE ? bytes : 0;
}
static BT_HDR *ibt_sdp_request_alloc(const tCONN_CB *ccb)
{
    if (!ccb || !ccb->p_db) return NULL;
    const size_t bytes = ibt_sdp_request_bytes(ccb->p_db->num_uuid_filters,
                                              ccb->p_db->num_attr_filters);
    return bytes ? (BT_HDR *)osi_malloc(bytes) : NULL;
}
'''
 mark='static void sdp_snd_service_search_req(tCONN_CB *p_ccb, UINT8 cont_len, UINT8 *p_cont)'
 assert s.count(mark)==1;s=s.replace(mark,helper+'\n'+mark,1)
 assert s.count('osi_malloc(SDP_DATA_BUF_SIZE)')==3
 s=s.replace('osi_malloc(SDP_DATA_BUF_SIZE)','ibt_sdp_request_alloc(p_ccb)')
 return s

def generate(source,out,control):
 raw=source.read_bytes();assert hashlib.sha256(raw).hexdigest()==LOCKED and raw==control.read_bytes(),'SDK source drift'
 out.write_text(transform(raw.decode()),encoding='utf8')
