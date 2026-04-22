# CERBERUS screenshots

Canonical visual reference captured from DOSBox Staging running CERBERUS v0.8.1 (stock build, commit `bdfe95c`) on 2026-04-22. DOSBox Staging's emulated 486 DX + S3 Trio64 + Sound Blaster 16 profile is the capture target; the on-screen content is what a real bench run of the same tier produces, minus bench numbers (bench was skipped for fast capture).

Harness scripts at `tools/capture_screenshots.ps1` and `tools/capture_help.ps1`. Configs at `devenv/cap_ui.conf`, `devenv/cap_noui.conf`, `devenv/cap_help.conf`.

## Screenshots

### [01-visual-journey-pit-metronome.png](01-visual-journey-pit-metronome.png)

The PIT Metronome visual journey card. A dot bounces at 18.2 Hz across the display with a PC-speaker click per tick. Steady rhythm means the timer is healthy; stutter means a TSR has hooked INT 8. Part of the v0.6.0 visual journey that makes CERBERUS's work legible, not just numeric.

### [02-visual-journey-audio-hardware.png](02-visual-journey-audio-hardware.png)

The Audio Hardware journey title card with the three-headed Cerberus art at the left and a plain-language description of what's about to happen. "Playing a test scale as PCM samples through your Sound Blaster DSP. If you hear 8 ascending square-wave notes, your SB PCM path works." The `[any key to continue, S to skip, Esc to skip all]` footer is the standard journey dismissal contract.

### [03-three-pane-summary.png](03-three-pane-summary.png)

The main scrollable three-pane summary UI, top of scroll. DETECTION pane shows every identified subsystem: emulator, CPU family/model/stepping, FPU, memory, cache, bus, video adapter + chipset, audio, BIOS family + date. BENCHMARKS pane begins below. The Norton-style F-key legend at the bottom (`1Help 3Exit Up/Dn PgUp/Dn Home/End ... rows 1-24 of 44`) is the v0.8.0 M3 CUA-lite feature: the grammar a DOS user from Borland IDE or MSD already knows.

### [04-batch-text-output.png](04-batch-text-output.png)

CERBERUS run with `/NOUI`: `ui_render_batch` dumps each of the three head sections to stdout instead of launching the interactive scrollable summary. BENCHMARKS / SYSTEM VERDICTS / UPLOAD STATUS sections visible, consistency verdicts (Timing self-check WARN, 486DX-FPU PASS, Ext-mem-vs-CPU PASS, Audio-mixer PASS), and nickname/notes round-trip. Used by bench operators who want a one-shot capture and don't need UI interaction.

### [05-help-overlay.png](05-help-overlay.png)

F1 help overlay (v0.8.0 M3.3). Opens over the scrollable summary, shows the navigation + commands reference, version, and build variants. Press any key to dismiss and return to the summary. Part of the CUA-lite interaction grammar: F1 = help, F3 or Esc = exit, arrow keys scroll.

## Reproducing the captures

```
powershell -ExecutionPolicy Bypass -File tools\capture_screenshots.ps1
powershell -ExecutionPolicy Bypass -File tools\capture_help.ps1
```

Both scripts assume DOSBox Staging at the default install path and `dist\CERBERUS.EXE` at the current stock build. Outputs land back in this directory.

Timing notes: DOSBox Staging takes ~30-40 seconds to complete CERBERUS's detect phase under the emulation overhead. F1 is delivered via a raw scancode (VK 0x70 / scan 0x3B) because SendKeys' virtual-key path does not propagate F1 to the guest reliably — an idiosyncrasy of DOSBox Staging's SDL input backend.
