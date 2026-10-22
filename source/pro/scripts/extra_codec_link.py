"""Keep pinned prebuilt decoder archives in PlatformIO's final SCons link.

IDF/CMake consumes imported targets; this hybrid builder reconstructs LIBS
from component targets and omits them. Files stay in the standard link group.
No global allocation wrapping and no SDK mutation.
"""
from pathlib import Path
Import('env')
directory=Path(env.subst('$BUILD_DIR'))/'esp-idf/ibt_extra_codecs'
archives=[directory/'libibt_audio_simple_dec.a',directory/'libibt_audio_codec.a']
env.Append(LIBS=[env.File(str(p)) for p in archives])
env.Depends('$BUILD_DIR/firmware.elf',[str(p) for p in archives])
