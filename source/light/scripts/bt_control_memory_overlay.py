"""Pinned Light4-only L2CAP command sizing and failed AVRCP discovery ownership."""
import hashlib

def l2cap(text):
    old='''    BT_HDR  *p_buf = (BT_HDR *)osi_malloc(L2CAP_CMD_BUF_SIZE);
    UINT8   *p;'''
    new='''    /* R245: callers append exactly len command bytes after these headers.
     * Keep all headroom; do not allocate the unused 660-byte pool capacity.
     * The SDK pool ceiling remains unchanged for other L2CAP packet builders. */
    const size_t bytes = sizeof(BT_HDR) + (size_t)L2CAP_SEND_CMD_OFFSET +
                         HCI_DATA_PREAMBLE_SIZE + L2CAP_PKT_OVERHEAD +
                         L2CAP_CMD_OVERHEAD + (size_t)len;
    if (bytes > L2CAP_CMD_BUF_SIZE) return NULL;
    BT_HDR *p_buf = (BT_HDR *)osi_malloc(bytes);
    UINT8   *p;'''
    assert text.count(old)==1
    return text.replace(old,new,1)

def avrc(text):
    old='''        result = SDP_ServiceSearchAttributeRequest(bd_addr, p_db->p_db, avrc_sdp_cback);'''
    new=old+'''
        if (result == FALSE) {
            /* No asynchronous request owns this database on failure. Leaving
             * service_uuid set would reject all future discovery as BUSY. */
            avrc_cb.service_uuid = 0;
            avrc_cb.p_db = NULL;
            avrc_cb.p_cback = NULL;
        }'''
    assert text.count(old)==1
    return text.replace(old,new,1)

def bta(text):
    old='''                APPL_TRACE_DEBUG("disc %d", p_cb->disc);
            }
        }
    }
}'''
    new='''                APPL_TRACE_DEBUG("disc %d", p_cb->disc);
            } else {
                /* R245: rejected startup has no pending SDP owner/callback.
                 * Return the full database now, rather than retaining 1000B
                 * through audio playback until a later retry or teardown. */
                utl_freebuf((void **)&p_cb->p_disc_db);
            }
        }
    }
}'''
    assert text.count(old)==1
    return text.replace(old,new,1)

def generate(source,output,control,kind):
    raw=source.read_bytes();assert raw==control.read_bytes(),'SDK version changed: '+str(source)
    result=globals()[kind](raw.decode('utf8').replace('\r\n','\n'))
    if not output.exists() or output.read_text()!=result:output.write_text(result,encoding='utf8')
    return dict(original_sha256=hashlib.sha256(raw).hexdigest(),overlay_sha256=hashlib.sha256(output.read_bytes()).hexdigest())
