"""Version-locked IDF 5.5.5 correction: negotiated PCM/SBC rate, never silent resampling.
Only the dedicated SYSTEM toolchain is patched. Original Apache SDK files are retained.
"""
from pathlib import Path
import hashlib

def once(text,old,new):
 if text.count(old)!=1:raise RuntimeError('SBC patch context mismatch; build refused: '+old[:60])
 return text.replace(old,new,1)

def transform(name,text):
 if name=='bta_av_co.c':
  text=once(text,'(A2D_SBC_IE_SAMP_FREQ_44), /* samp_freq */','(A2D_SBC_IE_SAMP_FREQ_16 | A2D_SBC_IE_SAMP_FREQ_32 | A2D_SBC_IE_SAMP_FREQ_44 | A2D_SBC_IE_SAMP_FREQ_48), /* samp_freq */')
  text=once(text,'        case 32000:\n        case 48000:', '        case 32000:\n            sbc_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_32;\n            break;\n        case 48000:')
  text=once(text,'/* SBC SRC codec capabilities */', 'extern UINT32 ibt_sbc_output_rate(void);\n\n/* SBC SRC codec capabilities */')
  text=once(text,'A2D_BldSbcInfo(AVDT_MEDIA_AUDIO, (tA2D_SBC_CIE *) &bta_av_co_sbc_caps, p_codec_info);', """tA2D_SBC_CIE app_caps = bta_av_co_sbc_caps;
        UINT32 app_rate = ibt_sbc_output_rate();
        app_caps.samp_freq = app_rate == 16000 ? A2D_SBC_IE_SAMP_FREQ_16 :\n            app_rate == 32000 ? A2D_SBC_IE_SAMP_FREQ_32 :
            app_rate == 48000 ? A2D_SBC_IE_SAMP_FREQ_48 : A2D_SBC_IE_SAMP_FREQ_44;
        A2D_BldSbcInfo(AVDT_MEDIA_AUDIO, &app_caps, p_codec_info);""")
  text=once(text,'if (A2D_BldSbcInfo(A2D_MEDIA_TYPE_AUDIO, (tA2D_SBC_CIE *)&btc_av_sbc_default_config, bta_av_co_cb.codec_cfg.info) != A2D_SUCCESS) {', """tA2D_SBC_CIE app_config = btc_av_sbc_default_config;
    UINT32 app_rate = ibt_sbc_output_rate();
    if (app_rate == 16000) app_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_16;\n    else if (app_rate == 32000) app_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_32;
    else if (app_rate == 48000) app_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_48;
    if (A2D_BldSbcInfo(A2D_MEDIA_TYPE_AUDIO, &app_config, bta_av_co_cb.codec_cfg.info) != A2D_SUCCESS) {""")
  # Do not let 8/12/16/24k fall through to the newly separated32k case.
  text=once(text,'        case 24000:\n        case 32000:', '        case 24000:\n            sbc_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_48;\n            break;\n        case 32000:')
 else:
  text=once(text,'    media_feeding.cfg.pcm.sampling_freq = 44100;', """    tA2D_SBC_CIE negotiated;
    UINT16 negotiated_mtu;
    bta_av_co_audio_get_sbc_config(&negotiated, &negotiated_mtu);
    media_feeding.cfg.pcm.sampling_freq = negotiated.samp_freq == A2D_SBC_IE_SAMP_FREQ_16 ? 16000 :\n        negotiated.samp_freq == A2D_SBC_IE_SAMP_FREQ_32 ? 32000 :
        negotiated.samp_freq == A2D_SBC_IE_SAMP_FREQ_48 ? 48000 : 44100;""")
  text=once(text,'    case 32000:\n    case 48000:', """    case 32000:
        if (a2dp_source_local_param.btc_aa_src_cb.encoder.s16SamplingFreq != SBC_sf32000) {
            a2dp_source_local_param.btc_aa_src_cb.encoder.s16SamplingFreq = SBC_sf32000;
            reconfig_needed = TRUE;
        }
        break;
    case 48000:""")
  text=once(text,'    case 24000:\n    case 32000:', """    case 24000:
        if (a2dp_source_local_param.btc_aa_src_cb.encoder.s16SamplingFreq != SBC_sf48000) {
            a2dp_source_local_param.btc_aa_src_cb.encoder.s16SamplingFreq = SBC_sf48000;
            reconfig_needed = TRUE;
        }
        break;
    case 32000:""")
 if name=='bta_av_co.c':
  text=once(text,'        case 16000:\n', '        case 16000:\n            sbc_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_16;\n            break;\n')
  # 8/12k must keep their previous48k mapping, not fall into16k.
  text=once(text,'        case 12000:\n        case 16000:', '        case 12000:\n            sbc_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_48;\n            break;\n        case 16000:')
 else:
  text=once(text,'    case 12000:\n    case 16000:\n', '    case 12000:\n        if (a2dp_source_local_param.btc_aa_src_cb.encoder.s16SamplingFreq != SBC_sf48000) {\n            a2dp_source_local_param.btc_aa_src_cb.encoder.s16SamplingFreq = SBC_sf48000;\n            reconfig_needed = TRUE;\n        }\n        break;\n    case 16000:\n        if (a2dp_source_local_param.btc_aa_src_cb.encoder.s16SamplingFreq != SBC_sf16000) {\n            a2dp_source_local_param.btc_aa_src_cb.encoder.s16SamplingFreq = SBC_sf16000;\n            reconfig_needed = TRUE;\n        }\n        break;\n')
 return text

def apply(sdk,control):
 for name in ('bta_av_co.c','btc_a2dp_source.c'):
  original=(control/name).read_text();patched=transform(name,original)
  path=sdk/name;actual=path.read_text()
  previous={'bta_av_co.c':'c327477f5494e393fa415c9f6b8fc758e63e65837c3ae1eab549151404262bec','btc_a2dp_source.c':'795da9ac235bb5b1592f4f3c15f99d501aa7bb2b701169a65629ae1f59a2da10'}
  if actual not in (original,patched) and hashlib.sha256(actual.encode()).hexdigest()!=previous[name] and hashlib.sha256(actual.encode()).hexdigest()!={'bta_av_co.c': '992ac25b844c4dfdcbd1278e86e27a7c74c0ad3e32c2c5cf4bde1756e0061178', 'btc_a2dp_source.c': '795da9ac235bb5b1592f4f3c15f99d501aa7bb2b701169a65629ae1f59a2da10'}[name]:raise RuntimeError('Unexpected SDK source; build refused: '+str(path))
  if actual!=patched:path.write_text(patched)
  print('[IBT-SBC-RATE] '+name+' sha256='+hashlib.sha256(path.read_bytes()).hexdigest())
