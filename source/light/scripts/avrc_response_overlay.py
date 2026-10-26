"""Pinned Light4 AVRCP response allocation. Payload builders stay byte-identical."""
import hashlib

def transform(s):
    helper='''/* R246: all offsets include the transport prepend headroom. Short control
 * responses do not need the metadata-text maximum. The text builders below
 * still require BT_DEFAULT_BUFFER_SIZE and retain that original capacity.
 * Existing packets can be appended to: query their real heap allocation and
 * grow transactionally, never assume a previous short response has 4112B. */
static size_t ibt_avrc_response_bytes(const tAVRC_RESPONSE *rsp, size_t offset,
                                      size_t previous_len)
{
    size_t payload = 5; /* also covers the generic rejected response */
    size_t count = 0;
    if (rsp->rsp.status == AVRC_STS_NO_ERROR) {
        switch (rsp->pdu) {
        case AVRC_PDU_NEXT_GROUP:
        case AVRC_PDU_PREV_GROUP:
        case AVRC_PDU_SET_PLAYER_APP_VALUE:
        case AVRC_PDU_INFORM_DISPLAY_CHARSET:
        case AVRC_PDU_INFORM_BATTERY_STAT_OF_CT:
        case AVRC_PDU_REQUEST_CONTINUATION_RSP:
        case AVRC_PDU_ABORT_CONTINUATION_RSP:
        case AVRC_PDU_SET_ABSOLUTE_VOLUME:
            break;
        case AVRC_PDU_GET_PLAY_STATUS:
            payload = 13;
            break;
        case AVRC_PDU_GET_CAPABILITIES:
            count = rsp->get_caps.count;
            if (rsp->get_caps.capability_id == AVRC_CAP_COMPANY_ID) {
                if (count > AVRC_CAP_MAX_NUM_COMP_ID) count = AVRC_CAP_MAX_NUM_COMP_ID;
                count *= 3;
            } else if (count > AVRC_CAP_MAX_NUM_EVT_ID) count = AVRC_CAP_MAX_NUM_EVT_ID;
            payload = (previous_len > 4 ? previous_len : 6) + count;
            break;
        case AVRC_PDU_LIST_PLAYER_APP_ATTR:
            count = rsp->list_app_attr.num_attr;
            if (count > AVRC_MAX_APP_ATTR_SIZE) count = AVRC_MAX_APP_ATTR_SIZE;
            payload = (previous_len > 4 ? previous_len : 5) + count;
            break;
        case AVRC_PDU_LIST_PLAYER_APP_VALUES:
            count = rsp->list_app_values.num_val;
            if (count > AVRC_MAX_APP_ATTR_SIZE) count = AVRC_MAX_APP_ATTR_SIZE;
            payload = (previous_len > 4 ? previous_len : 5) + count;
            break;
        case AVRC_PDU_GET_CUR_PLAYER_APP_VALUE:
            count = rsp->get_cur_app_val.num_val;
            if (count > AVRC_MAX_APP_ATTR_SIZE) count = AVRC_MAX_APP_ATTR_SIZE;
            payload = (previous_len > 4 ? previous_len : 5) + 2 * count;
            break;
        case AVRC_PDU_REGISTER_NOTIFICATION:
            switch (rsp->reg_notif.event_id) {
            case AVRC_EVT_TRACK_CHANGE: payload = 5 + AVRC_UID_SIZE; break;
            case AVRC_EVT_PLAY_POS_CHANGED: payload = 9; break;
            case AVRC_EVT_APP_SETTING_CHANGE:
                count = rsp->reg_notif.param.player_setting.num_attr;
                if (count > AVRC_MAX_APP_SETTINGS) count = AVRC_MAX_APP_SETTINGS;
                payload = 6 + 2 * count;
                break;
            default: payload = 6; break;
            }
            break;
        default:
            return BT_DEFAULT_BUFFER_SIZE; /* variable metadata/text: unchanged */
        }
    }
    if (payload < previous_len) payload = previous_len;
    const size_t bytes = sizeof(BT_HDR) + offset + payload;
    return bytes <= BT_DEFAULT_BUFFER_SIZE ? bytes : 0;
}

static BOOLEAN ibt_avrc_ensure_response(tAVRC_RESPONSE *rsp, BT_HDR **packet)
{
    BT_HDR *old = *packet;
    const size_t old_span = sizeof(BT_HDR) + (size_t)old->offset + old->len;
    const size_t allocated = heap_caps_get_allocated_size(old);
    if (old_span > allocated || old_span > BT_DEFAULT_BUFFER_SIZE) return FALSE;
    const size_t bytes = ibt_avrc_response_bytes(rsp, old->offset, old->len);
    if (bytes == 0) return FALSE;
    if (bytes <= allocated) return TRUE;
    BT_HDR *grown = (BT_HDR *)osi_malloc(bytes);
    if (grown == NULL) return FALSE; /* old owner/data intact on OOM */
    memcpy(grown, old, old_span);
    *packet = grown;
    osi_free(old);
    return TRUE;
}

'''
    marker='static BT_HDR *avrc_bld_init_rsp_buffer(tAVRC_RESPONSE *p_rsp)'
    assert s.count(marker)==1;s=s.replace(marker,helper+marker,1)
    old='    BT_HDR *p_pkt = (BT_HDR *)osi_malloc(BT_DEFAULT_BUFFER_SIZE);'
    new='''    const size_t bytes = ibt_avrc_response_bytes(p_rsp, offset, 0);
    if (bytes == 0) return NULL;
    BT_HDR *p_pkt = (BT_HDR *)osi_malloc(bytes);'''
    assert s.count(old)==1;s=s.replace(old,new,1)
    old='''        alloc = TRUE;
    }
    status = AVRC_STS_NO_ERROR;'''
    new='''        alloc = TRUE;
    } else if (!ibt_avrc_ensure_response(p_rsp, pp_pkt)) {
        return AVRC_STS_INTERNAL_ERR;
    }
    status = AVRC_STS_NO_ERROR;'''
    assert s.count(old)==1;s=s.replace(old,new,1)
    s=s.replace('#include "osi/allocator.h"','#include "osi/allocator.h"\n#include "esp_heap_caps.h"',1)
    s=s.replace('always of size BT_DEFAULT_BUFFER_SIZE.','BT_DEFAULT_BUFFER_SIZE for these variable text responses.',1)
    return s

def generate(source,output,control):
    raw=source.read_bytes();assert raw==control.read_bytes(),'AVRCP response SDK version changed'
    s=transform(raw.decode('utf8').replace('\r\n','\n'))
    if not output.exists() or output.read_text()!=s:output.write_text(s,encoding='utf8')
    return dict(original_sha256=hashlib.sha256(raw).hexdigest(),overlay_sha256=hashlib.sha256(output.read_bytes()).hexdigest())
