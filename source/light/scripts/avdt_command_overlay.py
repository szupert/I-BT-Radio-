"""Light4-only, pinned SDK command-copy correction; shared SDK stays unchanged."""
from pathlib import Path
import hashlib

def transform(text):
    def once(old,new):
        nonlocal text
        assert text.count(old)==1,old[:120]
        text=text.replace(old,new,1)
    marker='void avdt_ccb_ret_cmd(tAVDT_CCB *p_ccb, tAVDT_CCB_EVT *p_data)'
    helper='''/* R243: retain the whole header/headroom/payload, not unused pool capacity.
 * p_curr_cmd is immutable until ACK/failure; retransmits need the same span.
 * avdt_msg_send only prepends into existing offset, it does not append data. */
static BT_HDR *ibt_avdt_clone_command(const BT_HDR *source)
{
    if (source == NULL) return NULL;
    const size_t bytes = sizeof(BT_HDR) + (size_t)source->offset + (size_t)source->len;
    if (bytes > AVDT_CMD_BUF_SIZE) return NULL;
    BT_HDR *copy = (BT_HDR *)osi_malloc(bytes);
    if (copy != NULL) memcpy(copy, source, bytes);
    return copy;
}

'''
    once(marker,helper+marker)
    once('''            if ((p_msg = (BT_HDR *) osi_malloc(AVDT_CMD_BUF_SIZE)) != NULL) {
                memcpy(p_msg, p_ccb->p_curr_cmd,
                       (sizeof(BT_HDR) + p_ccb->p_curr_cmd->offset + p_ccb->p_curr_cmd->len));''',
         '''            if ((p_msg = ibt_avdt_clone_command(p_ccb->p_curr_cmd)) != NULL) {''')
    once('''            if ((p_ccb->p_curr_cmd = (BT_HDR *) osi_malloc(AVDT_CMD_BUF_SIZE)) != NULL) {
                memcpy(p_ccb->p_curr_cmd, p_msg, (sizeof(BT_HDR) + p_msg->offset + p_msg->len));

                avdt_msg_send(p_ccb, p_msg);
            }''',
         '''            if ((p_ccb->p_curr_cmd = ibt_avdt_clone_command(p_msg)) != NULL) {
                avdt_msg_send(p_ccb, p_msg);
            } else {
                /* Queue ownership already transferred. Use the SDK's normal
                 * reject path to notify the owner and free this command once.
                 * Do not lose p_msg or leave START waiting for an unsent ACK. */
                UINT8 error = AVDT_ERR_RESOURCE;
                p_ccb->p_curr_cmd = p_msg;
                avdt_ccb_cmd_fail(p_ccb, (tAVDT_CCB_EVT *)&error);
            }''')
    return text

def generate(source, output, control):
    raw=source.read_bytes()
    assert raw==control.read_bytes(),'Unrecognized AVDTP SDK source'
    text=transform(raw.decode('utf8').replace('\r\n','\n'))
    output.parent.mkdir(parents=True,exist_ok=True)
    if not output.exists() or output.read_text()!=text:output.write_text(text,encoding='utf8')
    return dict(original_sha256=hashlib.sha256(raw).hexdigest(),overlay_sha256=hashlib.sha256(output.read_bytes()).hexdigest())
