/* BridgingHeader.h -- the C the app and its speech extension call from Swift: Android's front end (ssa_engine,
 * ssa_map), the Apple front end's own (ssp_ssml, ssp_speech, ssp_import), the Braille Lite's import and state
 * (bl_firmware.h, bl_state.h), and the Braille Lite unit tab's emulator (emu_unit.h, emu_catalog.h, chords.h) and its
 * files (bl_files.h, bl_files_state.h, bl_files_xfer.h), all in SSI263Core (build_apple.sh). */
#include "ssa_engine.h"
#include "ssa_map.h"
#include "ssa_import.h"
#include "ssp_ssml.h"
#include "ssp_speech.h"
#include "ssp_import.h"
#include "blazie/bl_firmware.h"
#include "blazie/bl_state.h"
#include "../apps/blazie/emu_unit.h"
#include "../apps/blazie/emu_catalog.h"
#include "../apps/blazie/chords.h"
#include "blazie/bl_files.h"
#include "blazie/bl_files_state.h"
#include "blazie/bl_files_xfer.h"
