"""R253 Light SDP response storage; outgoing R251 builders and SDK ABI unchanged."""
HELPER=r'''/* R253: reserve the response's declared sequence length, not the 4 KiB ceiling.
 * A split sequence header uses at most four temporary bytes before its length
 * is known. No CCB layout/side table: capacity follows the immutable header.
 * rsp_list remains an ordinary osi_malloc pointer owned/freed by the SDK. */
static UINT16 ibt_sdp_response_size(const UINT8 *header, size_t have,
                                    size_t *bytes, BOOLEAN *complete)
{
    *complete = FALSE;
    if (!have) { *bytes = 0; return SDP_SUCCESS; }
    if ((header[0] >> 3) != DATA_ELE_SEQ_DESC_TYPE) return SDP_ILLEGAL_PARAMETER;
    const unsigned size_code = header[0] & 7u;
    const size_t extra = size_code < 5 ? 0u : size_code == 5 ? 1u : size_code == 6 ? 2u : 4u;
    const size_t head = 1u + extra;
    if (have < head) { *bytes = have; return SDP_SUCCESS; }
    UINT32 payload = 0;
    if (!extra) payload = 1u << size_code;
    else for (size_t i = 1; i < head; ++i) payload = (payload << 8) | header[i];
    if (payload > SDP_MAX_LIST_BYTE_COUNT - head) return SDP_INVALID_PDU_SIZE;
    *bytes = head + (size_t)payload;
    *complete = TRUE;
    return SDP_SUCCESS;
}
static UINT16 ibt_sdp_response_append(tCONN_CB *ccb, const UINT8 *part,
                                      UINT16 count, BOOLEAN final)
{
    const size_t used = ccb->list_len;
    if (used > SDP_MAX_LIST_BYTE_COUNT || count > SDP_MAX_LIST_BYTE_COUNT - used ||
        (used && !ccb->rsp_list)) return SDP_INVALID_PDU_SIZE;
    const size_t needed = used + count;
    UINT8 header[5];
    const size_t prefix = needed < sizeof(header) ? needed : sizeof(header);
    for (size_t i = 0; i < prefix; ++i) header[i] = i < used ? ccb->rsp_list[i] : part[i - used];
    size_t wanted = 0, capacity = 0;
    BOOLEAN known = FALSE, old_known = FALSE;
    UINT16 status = ibt_sdp_response_size(header, prefix, &wanted, &known);
    if (status != SDP_SUCCESS) return status;
    if (known && needed > wanted) return SDP_INVALID_PDU_SIZE;
    if (final && (!known || needed != wanted)) return SDP_INVALID_CONT_STATE;
    if (!needed) return SDP_SUCCESS; /* An empty continuation needs no storage. */
    if (used) {
        status = ibt_sdp_response_size(ccb->rsp_list, used < 5u ? used : 5u,
                                       &capacity, &old_known);
        if (status != SDP_SUCCESS) return status;
        if (!old_known) capacity = used;
    } else if (ccb->rsp_list) {
        /* Previous service handle was already parsed; it has no live bytes. */
        osi_free(ccb->rsp_list);
        ccb->rsp_list = NULL;
    }
    if (!ccb->rsp_list || capacity < wanted) {
        UINT8 *replacement = (UINT8 *)osi_malloc(wanted);
        if (!replacement) {
            SDP_TRACE_ERROR("[SDP-RSP] allocation failed bytes=%u retained=%u\n",
                            (unsigned)wanted, (unsigned)used);
            return SDP_NO_RESOURCES; /* Old pointer still belongs to the CCB. */
        }
        if (used) memcpy(replacement, ccb->rsp_list, used);
        if (ccb->rsp_list) osi_free(ccb->rsp_list);
        ccb->rsp_list = replacement;
        SDP_TRACE_DEBUG("[SDP-RSP] allocated bytes=%u ceiling=%u\n",
                        (unsigned)wanted, (unsigned)SDP_MAX_LIST_BYTE_COUNT);
    }
    if (count) memcpy(ccb->rsp_list + used, part, count);
    return SDP_SUCCESS;
}
'''

def transform(s):
    mark='static size_t ibt_sdp_request_bytes'
    assert s.count(mark)==1 and 'ibt_sdp_response_append' not in s
    s=s.replace(mark,HELPER+'\n'+mark,1)
    for count in ['list_byte_count','lists_byte_count']:
        old='''        if (p_ccb->rsp_list == NULL) {
            p_ccb->rsp_list = (UINT8 *)osi_malloc (SDP_MAX_LIST_BYTE_COUNT);
            if (p_ccb->rsp_list == NULL) {
                SDP_TRACE_ERROR ("SDP - no gki buf to save rsp\\n");
                sdp_disconnect (p_ccb, SDP_NO_RESOURCES);
                return;
            }
        }
        memcpy (&p_ccb->rsp_list[p_ccb->list_len], p_reply, COUNT);'''.replace('p_reply, COUNT','p_reply, '+count)
        new='''        const UINT16 rsp_status = ibt_sdp_response_append(p_ccb, p_reply, COUNT,
                                                              p_reply[COUNT] == 0);
        if (rsp_status != SDP_SUCCESS) {
            sdp_disconnect(p_ccb, rsp_status);
            return;
        }'''.replace('COUNT',count)
        assert s.count(old)==1,count
        s=s.replace(old,new,1)
    assert 'osi_malloc (SDP_MAX_LIST_BYTE_COUNT)' not in s
    return s

def generate(path):
    raw=path.read_text(encoding='utf8')
    path.write_text(transform(raw),encoding='utf8')
