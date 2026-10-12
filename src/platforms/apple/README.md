# SSI-263 Speech for macOS and iOS (in progress)

A system speech voice for VoiceOver and every app on the Mac, iPhone and iPad, speaking with the same emulated
SSI-263 hardware as the NVDA add-ons, the SAPI engine, the Linux module and the Android app, with the same PCM for
the same text and settings. The Android app is the blueprint (`src/platforms/android`), and TGSpeechBox's Apple app
is the model for the Xcode project, the speech extension and its VoiceOver pauses.

The apps ship hollow: no firmware at all, only this project's own MIT code and MAME's BSD-3-Clause cores. Each user
imports the firmware they own, recognised by content (SHA-256), never by name.

## Voices and languages

Ten voices, each speaking one language, offered to the system only once their firmware is imported: the Braille
Lite 2000 in English, Spanish, German and Italian, the Slovak Braille 'n Speak 2000, the Speak-Out, the Mockingboard
(1.1 and early) and the two Accents. Then the Braille Lite 2000's language updates: one voice per language the
release table names a voice for (`src/csrc/blazie/bl_languages.txt`: Hungarian, Turkish, Polish and 14 more), read
from the library (`blv_release_*`, `ssa_release_voice`), never listed in Swift. Each is offered under the system's
own locale for its language (hu-HU; Arabic as ar-001, Norwegian as nb-NO, Portuguese as pt-PT beside Brazil's
pt-BR), and named in English ("Braille Lite 2000 (Russian)", the table's `vname`). The Setup list shows each
voice's file (`BL2HUN.BNS — not imported`); the languages wait behind "Show all Braille Lite importable languages (17,
N imported)", imported or not. One import takes them all from the NVDA add-on too (`_ssi263_blazie/bl2000/`). A release no voice speaks yet (the betas, Japanese) is still
imported, kept under `units/` for the Braille Lite unit tab. Languages work as in TGSpeechBox's speech extension. Each voice is offered once
per locale it speaks: the English voices as en-US, en-GB, en-AU and en-CA, and the Spanish Braille Lite as es-ES and
es-MX. The locale ends the voice's identifier (`com.ssi263speech.braillelite.de-de`), and VoiceOver switches language
by choosing another voice for the next request. `<lang>` elements inside a request are not switched on. "Lock
language" (Voice settings) speaks every request with the voice chosen in the app, whatever the request names, for
readers that always ask for the system's language.

## The app's pages, as Android's

Setup and Voice settings follow the Android app's pages and words (`SettingsActivity.kt`), with VoiceOver in place of
TalkBack and nothing built in. Setup: the voices, the firmware list (file names, the languages under "Show all"), the
import (a line per unit in the confirmation: how long it takes, and whether it replaces one already here; the device
kept awake while it runs; refused below 16 MB free), "Try it", where to choose the voice, the licences. Voice settings:
the voice and Lock language, rate, pitch, tone, engine volume (135 percent by default), the unit's settings (inflection,
short pauses, numbers, run ahead, line lift, the idle sound, the sample rate), the Speak-Out's, and VoiceOver's pauses.
Left out on purpose: Android's built-in Accent SA and Mockingboards (the app carries no firmware), its update check (the
App Store's), and 44 kHz (the Audio Unit runs at 22050).

## The Braille Lite unit tab

The app's third tab runs a Blazie unit on the firmware already imported for the voices: one app, no second firmware
picker (PR 18's standalone emulator was declined for that). Its units are the emulator's own catalog (`emc_list`,
src/apps/blazie/emu_catalog.c, the list every emulator shell reads) over the unit folder: the shipped units under
their flat names and every release on the table by its content, `units/` included -- never a unit without its
firmware. A release unit's first start makes its state by its own recipe (`emc_make_factory`). Each unit's memory is
the app's own (Application Support, "Braille Lite units"), saved when it is switched off or the app is left; the
voices' states in the App Group are only read. The unit runs in real time (`emu_unit.h`: its sound pulled by the
audio engine, its keys pushed as they go down and up through `chords.c`, as the Windows shell). Its braille field is
the Windows and GTK pattern: one read-only element holding the display as Unicode braille, keeping the keyboard (F D
S J K L, the space bar, ; and A the bars, by the keys' places), Alt+Shift+Right and Left the display's bars held.
Whether VoiceOver sends the field to braille alone, and whether its keys reach the unit, are for the testers.

## The native core: SSI263Core

`build_apple.sh` builds the chip, every voice's board, host and voice on MAME's Z180, 8085, V40 and 8086 cores (C++17
interpreters with no JIT, which iOS allows), the voice table (`src/csrc/voices.c`), Android's plain-C front end
(`ssa_engine`, `ssa_map` and `ssa_import`, used unchanged) and the Apple front end's own C (`core/`). These are the
same sources and flags as `build_android.sh`, with `-ffp-contract=off` throughout so the arithmetic is the reference's.
It makes static libraries for each Apple platform:

| Platform | Architectures | Minimum |
|---|---|---|
| macOS | arm64, x86_64 | 13.0 |
| iOS (devices) | arm64 | 16.0 |
| iOS Simulator | arm64, x86_64 | 16.0 |

    sh src/platforms/apple/build_apple.sh              every slice, then build/apple/SSI263Core.xcframework
    sh src/platforms/apple/build_apple.sh macos        one platform only

The output goes under `build/apple/` (gitignored). The apps link it with `-lc++`.

## TestFlight

    python src/platforms/apple/testflight.py build [--bump]      archives for macOS and iOS, made fresh and checked
    python src/platforms/apple/testflight.py upload              exactly those, then processed, for the internal group
    python src/platforms/apple/testflight.py status | crashes

This is TGSpeechBox's `testflight.py`, with its rules kept. The build number is the same on both platforms
(`CURRENT_PROJECT_VERSION` in `project.yml`, and the Audio Unit's version with it), raised by one for each upload. A
failed-looking upload is never retried before `status` shows what arrived. What goes up is the archive `build` checked:
its build numbers, its icon and privacy manifests, and `test/check_archive_no_firmware.py`, which makes sure it
carries no firmware at all (Android's checks, plus Aicom's files). The icon is drawn by `tools/make_icon.swift`
(`tools/make_icon.sh` adds the Mac sizes).

## Test

    sh build_linux.sh                                       the desktop references, on the Mac too
    python src/platforms/android/test/test_android_native.py
    python src/platforms/apple/test/test_apple_core.py [--simulator]
    SSI263_APPLE_TEST_BREAK=1 python src/platforms/apple/test/test_apple_core.py      control: must FAIL
    python src/platforms/apple/test/test_apple_speech.py
    SSI263_APPLE_SPEECH_BREAK=ssml-1|ssml-2|speech-1|speech-2|import|import-2 python ...   controls: each must FAIL
    sh src/platforms/apple/test/run_swift_tests.sh                                     the import, in Swift
    sh src/platforms/apple/test/test_installer.sh                  the app's import on the real firmware, as the app runs it,
                                                                   then the unit tab's catalog and two units started
    SSI263_UNIT_BREAK=factory sh src/platforms/apple/test/test_installer.sh             control: must FAIL
    SSI263_IMPORT_BREAK=1|state|speakout|accent|release sh src/platforms/apple/test/run_swift_tests.sh   controls

`test_apple_core.py` links Android's host-side program (`test_android_native.c`) against each runnable slice's
`libssi263core.a`, the library the apps link, and requires every case's samples and PCM hash to be the desktop
program's, along with the Accent SA's front-end text. `test_android_native.py` holds that desktop program to the
references, so the slices are held to them too. It runs macOS arm64 natively, x86_64 under Rosetta, and, with
`--simulator`, the iOS Simulator's arm64 in a booted simulator (`xcrun simctl spawn`). The device slice is checked
on the device by the app. Its control runs the slices with the request's rate dropped (`ssa_map_break`), so the run
must fail.

`test_apple_speech.py` holds the Apple front end's own C to its rules, on the macos-arm64 slice:
- **SSML:** VoiceOver's SSML is parsed as TGSpeechBox's speech extension parses it. The cases cover iOS 27's `<s>`
  parts, an `800.0ms` break folding into the sentence end before it, the strengths, the 30 ms floor, the 2 s cap, the
  end-of-request pause, a break-only spacer, the entities, the rate, and the volume rotor's decibels.
- **Speaking:** every voice whose files are there speaks each request byte for byte as its segments through
  `ssa_start`/`ssa_pull` directly, with the pauses' zeros between them. A stop holds, and off, short and long differ
  in length.
- **Import:** each of Aicom's files is known by its sha256 under any name, but not with one byte changed; a release
  with no shipped voice (a release voice's, and one no voice speaks) comes back as `SSP_FW_RELEASE` plus its row.
- **The release voices:** every one whose firmware is in the firmware folder (`release_voices.py`: found by content,
  its state made once by the desktop library and kept in `build/apple/test/release-states` only while its sha256 is
  the table's) speaks each request as every voice does.

`run_swift_tests.sh` compiles the app's import (`FirmwareImport.swift`, `ZipReader.swift`) with
`FirmwareImportTests.swift`: every case of Android's `FirmwareImportTest.kt` (the zip layouts, the add-on, update
programs, only the releases on the list, never a state, the Speak-Out's HEX, the words), the Accents' files (the
Accent add-on's `accent-sa/` ROMs and driver, loose and renamed), each zip form the reader takes (stored, deflated,
the sizes in a data descriptor), and zips made by `ditto` (Finder's Compress), Info-ZIP's `zip` and Python's
`zipfile`.

On the Mac, `test_android_native.py` runs every block but the Accent SA's, whose reference is the NVDA driver on the
Windows DLLs: `SSI263_ANDROID_TEST_ONLY=bl,ra,num,so,mini,import`.

## Files

| File | What it does |
|---|---|
| `build_apple.sh` | SSI263Core: the static libraries for each slice, lipo'd per platform, and the XCFramework with its headers |
| `core/ssp_ssml.h`, `ssp_ssml.c` | VoiceOver's SSML: the segments and their pauses at the Pause mode (off, short, long), as TGSpeechBox's speech extension makes them, and the prosody's rate, pitch and volume (the rotor's decibels) |
| `core/ssp_speech.h`, `ssp_speech.c` | A request spoken segment by segment through Android's `ssa_engine`, each segment's PCM unchanged, the pauses' zeros between them, pulled block by block; a stop from any thread |
| `core/ssp_import.h`, `ssp_import.c` | Every firmware the apps import: Aicom's files by sha256 (Android carries them built in), then Android's judgement of the Braille Lite's and the Speak-Out's |
| `app/FirmwareImport.swift` | Android's `FirmwareImport.kt` in Swift, with the Accents' files: what a source holds (zip layouts, the add-ons, update programs, single files), judged by the native side by content, and the refusals in words |
| `app/ZipReader.swift` | A zip's entries in order from their local headers, as `ZipInputStream` reads them: stored and deflated (Apple's Compression), sizes in the header or a data descriptor |
| `app/UnitCatalog.swift` | The unit tab's units (`emc_list` over the unit folder, present only), each one's memory in the app's own folder, a unit's first state |
| `app/UnitRunner.swift` | One unit running: `emu_create`, its sound through AVAudioEngine (`emu_render`), keys through `chords.c`, the bars, the display, the save |
| `app/UnitKeys.swift` | The unit's keyboard by the keys' places: macOS key codes, iOS HID usages |
| `app/BrailleField.swift` | The braille field: the display as Unicode braille for the screen reader, the keyboard kept, never edited |
| `app/UnitModel.swift`, `app/UnitView.swift` | The tab's state (the chosen unit, start and stop, volume, quick response) and its screen |
| `testflight.py` | TestFlight: fresh archives for both platforms, checked (build numbers, icon, privacy manifests, no firmware), then uploaded as checked |
| `test/check_archive_no_firmware.py` | An archive, app or .ipa carries no firmware: Android's checks (Blazie images, Intel HEX, states, disk images, the Speak-Out's and the Mockingboards' files, names) and Aicom's files by sha256; its controls |
| `app/PrivacyInfo.xcprivacy`, `extension/PrivacyInfo.xcprivacy` | No tracking, no data collected; UserDefaults only for the App Group's settings the app and its extension share (1C8F.1), and file attributes (FileManager.attributesOfItem: the firmware's file protection) in the app's own containers (C617.1). Plain plists, Apple's own layout and no comments: build 1's commented ones were refused as invalid (ITMS-91056) |
| `app/Assets.xcassets`, `tools/make_icon.swift`, `tools/make_icon.sh` | The icon: "263" over a sound wave on deep blue, drawn in code, opaque |
| `test/test_apple_core.py` | Each runnable slice against the desktop program, byte for byte, with its control |
| `test/InstallerTests.swift`, `test_installer.sh` | The app's FirmwareInstaller (inspect, then the Job's commit: the Braille Lite's state made and checked, each unit heard) on the repository's firmware against the macos-arm64 slice, in a temporary folder in place of the App Group's |
| `test/FirmwareImportTests.swift`, `run_swift_tests.sh` | The import's layouts and words with a fake native side, the zip forms and the real tools' zips; the controls |
| `test/test_apple_speech.c`, `.py` | The Apple front end's own C: the SSML and pauses, each voice's requests against their segments' own PCM, a stop, Aicom's files, the releases; the controls |
| `test/release_voices.py` | The release voices' files for the tests: each .BNS found by content, its state made once and checked against the table |
