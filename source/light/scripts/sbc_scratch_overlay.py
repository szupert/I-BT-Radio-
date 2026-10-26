"""Light-only generated source overlay; never mutates the shared SDK file."""
from pathlib import Path
import hashlib

def transform(text):
    def once(old,new):
        nonlocal text
        assert text.count(old)==1, old[:80]
        text=text.replace(old,new,1)
    once('    UINT64                      last_frame_us;',
         '    UINT64                      last_frame_us;\n    UINT16 *ibt_resample_scratch; // Light: allocated only for unequal rates')
    old='''    static UINT16 up_sampled_buffer[SBC_MAX_NUM_FRAME * SBC_MAX_NUM_OF_BLOCKS
                                    * SBC_MAX_NUM_OF_CHANNELS * SBC_MAX_NUM_OF_SUBBANDS * 2];
    static UINT16 read_buffer[SBC_MAX_NUM_FRAME * SBC_MAX_NUM_OF_BLOCKS
                              * SBC_MAX_NUM_OF_CHANNELS * SBC_MAX_NUM_OF_SUBBANDS];'''
    once(old,'''    enum { IBT_READ_BYTES = sizeof(UINT16) * SBC_MAX_NUM_FRAME * SBC_MAX_NUM_OF_BLOCKS
                * SBC_MAX_NUM_OF_CHANNELS * SBC_MAX_NUM_OF_SUBBANDS,
           IBT_UP_BYTES = IBT_READ_BYTES * 2 };
    UINT16 *up_sampled_buffer, *read_buffer;''')
    once('    /* Some Feeding PCM frequencies require to split the number of sample */', '''    // Equal-rate feeding returned above; preserve unequal-rate support lazily.
    if (!a2dp_source_local_param.ibt_resample_scratch) {
        a2dp_source_local_param.ibt_resample_scratch = osi_calloc(IBT_UP_BYTES + IBT_READ_BYTES);
        if (!a2dp_source_local_param.ibt_resample_scratch) return FALSE;
    }
    up_sampled_buffer = a2dp_source_local_param.ibt_resample_scratch;
    read_buffer = up_sampled_buffer + IBT_UP_BYTES / sizeof(UINT16);

    /* Some Feeding PCM frequencies require to split the number of sample */''')
    once('sizeof(up_sampled_buffer) -', 'IBT_UP_BYTES -')
    needle='    osi_free(a2dp_source_local_param_ptr);'
    assert text.count(needle)==2
    text=text.replace(needle,'''    osi_free(a2dp_source_local_param_ptr->ibt_resample_scratch);
    a2dp_source_local_param_ptr->ibt_resample_scratch = NULL;
'''+needle)
    once('#include "sbc_encoder.h"', '#include "sbc_encoder.h"\n#include "dual/sbc_packet_budget.h"\n#include "dual/sbc_frame_check.h"')
    once('static int btc_a2dp_source_state = BTC_A2DP_SOURCE_STATE_OFF;', '''static uint32_t ibt_sbc_packet_snapshot;
static uint32_t ibt_sbc_proofs[8] = {0, 0, 2166136261u, 0, 0, 0, 0, 0};
uint32_t ibt_sbc_proof(unsigned field) {
    return field < 8 ? __atomic_load_n(&ibt_sbc_proofs[field], __ATOMIC_RELAXED) : 0;
}
static void ibt_sbc_observe_pcm(const SINT16 *pcm, unsigned samples) {
    uint32_t count = ibt_sbc_proof(0);
    if (count >= 4096) return;
    uint32_t packed = ibt_sbc_proof(1), peak = packed >> 16, nonzero = packed & 65535u;
    uint32_t hash = ibt_sbc_proof(2);
    if (samples > 16) samples = 16;
    for (unsigned i = 0; i < samples && count < 4096; ++i, ++count) {
        const int32_t value = pcm[i];
        const uint32_t magnitude = value < 0 ? (uint32_t)-value : (uint32_t)value;
        if (magnitude > peak) peak = magnitude;
        nonzero += value != 0;
        hash = (hash ^ (uint16_t)value) * 16777619u;
    }
    __atomic_store_n(&ibt_sbc_proofs[0], count, __ATOMIC_RELAXED);
    __atomic_store_n(&ibt_sbc_proofs[1], (peak << 16) | nonzero, __ATOMIC_RELAXED);
    __atomic_store_n(&ibt_sbc_proofs[2], hash, __ATOMIC_RELAXED);
}
void ibt_sbc_transport_note(unsigned failed) {
    __atomic_fetch_or(&ibt_sbc_packet_snapshot, failed ? (1u << 30) : (1u << 29), __ATOMIC_RELAXED);
}
uint32_t ibt_sbc_packet_last(void) {
    return __atomic_load_n(&ibt_sbc_packet_snapshot, __ATOMIC_RELAXED);
}
static int btc_a2dp_source_state = BTC_A2DP_SOURCE_STATE_OFF;''')
    once('''        if (NULL == (p_buf = osi_malloc(BTC_MEDIA_AA_BUF_SIZE))) {''', '''        SBC_ENC_PARAMS *ibt_enc = &a2dp_source_local_param.btc_aa_src_cb.encoder;
        const size_t ibt_packet_bytes = ibt_sbc_packet_bytes(
            a2dp_source_local_param.btc_aa_src_cb.TxAaMtuSize,
            ibt_enc->s16NumOfBlocks, ibt_enc->s16NumOfSubBands,
            ibt_enc->s16NumOfChannels, ibt_enc->s16ChannelMode,
            ibt_enc->s16BitPool, nb_frame, sizeof(BT_HDR),
            BTC_MEDIA_AA_SBC_OFFSET, BTC_MEDIA_AA_BUF_SIZE);
        if (!ibt_packet_bytes || ibt_enc->u8NumPacketToEncode != 1) return;
        // No printf/heap/locks for diagnostics on the encoding path.
        const uint32_t ibt_sizes = (uint32_t)ibt_packet_bytes |
            ((uint32_t)a2dp_source_local_param.btc_aa_src_cb.TxAaMtuSize << 13) |
            ((uint32_t)(nb_frame > 15 ? 15 : nb_frame) << 25);
        uint32_t ibt_previous = __atomic_load_n(&ibt_sbc_packet_snapshot, __ATOMIC_RELAXED);
        while (!__atomic_compare_exchange_n(&ibt_sbc_packet_snapshot, &ibt_previous,
            (ibt_previous & 0xe0000000u) | ibt_sizes, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
        if (NULL == (p_buf = osi_malloc(ibt_packet_bytes))) {''')
    once('''            /* Fill allocated buffer with 0 */
            memset(a2dp_source_local_param.btc_aa_src_cb.encoder.as16PcmBuffer, 0, blocm_x_subband
                   * a2dp_source_local_param.btc_aa_src_cb.encoder.s16NumOfChannels);''',
         '''            /* R248: preserve aa_feed_residue on short PCM reads.
             * TRUE feeding fills the complete frame; do not erase its prefix. */''')
    once('''                SBC_Encoder(&(a2dp_source_local_param.btc_aa_src_cb.encoder));''', '''                ibt_sbc_observe_pcm(a2dp_source_local_param.btc_aa_src_cb.encoder.as16PcmBuffer,
                    blocm_x_subband * a2dp_source_local_param.btc_aa_src_cb.encoder.s16NumOfChannels);
                SBC_Encoder(&(a2dp_source_local_param.btc_aa_src_cb.encoder));
                const UINT8 *ibt_frame = a2dp_source_local_param.btc_aa_src_cb.encoder.pu8Packet;
                const uint32_t ibt_encoded = __atomic_fetch_add(&ibt_sbc_proofs[5], 1, __ATOMIC_RELAXED);
                if (!ibt_encoded) __atomic_store_n(&ibt_sbc_proofs[3],
                    ((uint32_t)ibt_frame[0] << 24) | ((uint32_t)ibt_frame[1] << 16) |
                    ((uint32_t)ibt_frame[2] << 8) | ibt_frame[3], __ATOMIC_RELAXED);
                const uint32_t ibt_check = __atomic_load_n(&ibt_sbc_proofs[4], __ATOMIC_RELAXED);
                unsigned ibt_checked = (ibt_check >> 24) & 127u, ibt_bad = ibt_check >> 31;
                if (ibt_checked < 32) {
                    ibt_bad |= !ibt_sbc_frame_crc_ok(ibt_frame,
                        a2dp_source_local_param.btc_aa_src_cb.encoder.u16PacketLength);
                    ++ibt_checked;
                }
                __atomic_store_n(&ibt_sbc_proofs[4], (ibt_bad << 31) | (ibt_checked << 24) |
                    (((uint32_t)a2dp_source_local_param.btc_aa_src_cb.encoder.FrameHeader & 255u) << 16) |
                    a2dp_source_local_param.btc_aa_src_cb.encoder.u16PacketLength, __ATOMIC_RELAXED);''')
    once('''            fixed_queue_enqueue(a2dp_source_local_param.btc_aa_src_cb.TxAaQ, p_buf, FIXED_QUEUE_MAX_TIMEOUT);''', '''            fixed_queue_enqueue(a2dp_source_local_param.btc_aa_src_cb.TxAaQ, p_buf, FIXED_QUEUE_MAX_TIMEOUT);
            __atomic_fetch_add(&ibt_sbc_proofs[6], 1, __ATOMIC_RELAXED);''')
    once('''    return fixed_queue_dequeue(a2dp_source_local_param.btc_aa_src_cb.TxAaQ, 0);''', '''    BT_HDR *ibt_packet = fixed_queue_dequeue(a2dp_source_local_param.btc_aa_src_cb.TxAaQ, 0);
    if (ibt_packet) __atomic_fetch_add(&ibt_sbc_proofs[7], 1, __ATOMIC_RELAXED);
    return ibt_packet;''')
    return text

def generate(source,output):
    text=Path(source).read_text()
    # This is the already version-locked negotiated-rate correction.
    assert hashlib.sha256(text.encode()).hexdigest() == '37b4bbfa4505ff2059d814d19d59950cdc3c9a7bf129f9e033961e1b3fceb795'
    patched=transform(text)
    output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    if not output.exists() or output.read_text()!=patched:output.write_text(patched)
    return hashlib.sha256(patched.encode()).hexdigest()
