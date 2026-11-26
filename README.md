# MO-T 11

MO-T 11 is a standalone Windows x64 wavetable synth. It has 16 voices, three oscillators per voice, 11 factory wavetables, nine factory patches, and a 136-route modulation matrix. The front panel follows [Mockup.png](Mockup.png).

## Play

- Launch **MO-T 11.exe**. It opens the system audio output and listens to available Windows MIDI inputs on channels 1–16. Click **Show Keys** to open the performance panel, which shows the number of open MIDI inputs and flashes when a MIDI note arrives. Click **Audio** to select a different output.
- Play a MIDI controller or click and drag across the on-screen keys. In the synth window, play `A W S E D F T G Y H U J K` from C to C, including while editing controls. Use **−** and **+** to shift octaves. **Panic** clears sounding notes.
- **Poly** plays chords. **Mono** plays the most recently held note and retriggers the envelopes on each note change. **Legato** keeps the envelopes and key-synced LFOs running across overlapping notes. In either mono mode, release the current key to return to the previous held key.
- **Glide** sets the pitch travel time between overlapping notes in Mono or Legato. Hold one key while pressing another, or drag across the on-screen keyboard. Set it to **Off** for immediate pitch changes. **Unison** stacks up to four layers per note in any mode. Use **Detune** to separate their pitches and **Width** to spread them across the stereo field. Voicing settings are saved with the patch.
- Turn **Arpeggiator** on and hold a chord on your MIDI controller or computer keyboard. The synth plays the held pitches as sixteenth notes with a 70% gate. A single held note repeats, including on the on-screen keyboard. Set **Tempo** from 40 to 240 BPM and click the direction button to cycle **Up**, **Down**, and **Up/Down**. Sustain keeps released keys in the pattern until you lift the pedal. The patch stores all three controls. Factory patches start with the arpeggiator off at 120 BPM in Up mode.
- Browse patches with the header arrows or click the patch display to jump to a factory patch. **Open** and **Save** use `.motpatch` files. The app remembers the working patch and selected audio output between launches.
- Use **Theme** in the header to switch between **Classic**, **Classic Dark**, **Midnight**, **Enamel**, and **Enamel Dark**. Both Enamel themes have color-coded plates, brushed silver knobs, and green displays. Enamel Dark uses deeper plate colors and light lettering. Classic Dark and Midnight have slim walnut sides. The app remembers your choice between launches.
- Drag a knob or fader, use the mouse wheel, or focus it and use the arrow keys. Double-click a control to return it to its Init value.

## Make a patch

- **Table Osc.** Each of the three rows has its own wavetable display and **Pos**, **Tune**, **Fine**, and **Vol** knobs. **Tune** shifts pitch by up to 24 semitones in either direction. **Fine** adjusts pitch by up to 50 cents in either direction. Click a display to choose a wavetable for that row. Under **Classic analog**, Sine, Triangle, Sawtooth, and Square are fixed shapes. With Variable Pulse, **Pos** sets the duty cycle from 5% to 95% and can be modulated for PWM. The **Analog Stack** factory patch layers sawtooth, square, and a triangle sub-oscillator.
- **Osc Setup.** Select **A**, **B**, or **C** in the setup strip to adjust its octave, stereo pan, and start phase. The octave offset adds to the **Tune** setting. **Reset** starts each note at the phase knob’s setting. **Free** varies the starting phase from note to note. **Stack/Cycle** switches that row between a stack of frames and a single waveform. Patches from earlier versions load with oscillator C muted.
- **Filter and envelopes.** Click the filter mode to cycle low-pass, band-pass, and high-pass. Use cutoff, resonance, and drive to shape the signal. The amp ADSR controls the voice’s gain. Route **Filter Env** to **Cutoff** for a filter sweep, or to another destination for a different contour.
- **LFOs.** Set rate and depth, then cycle sine, triangle, ramp, and square. **Free** runs continuously. **Key** starts a separate cycle for each played note.
- **Effects.** Switch each effect between **Bypass** and **On**. **Tape Delay** has a 50 ms to 1.2 s delay time, feedback, and **Wear** for darker, less stable repeats. **Plate Reverb** has a **Decay** range of 250 ms to 20 s. Turn up **Damp** to make the high frequencies fade faster. Each effect has its own wet/dry **Mix**. Tape delay feeds the plate, and the patch stores both effects' settings and bypass states. **Panic** clears both tails.
- **Mod Table.** Use **Add Route** to pick a source and destination. The route appears at 0% and can be adjusted with **Amount**. Scroll to see more routes. Click a row or use the arrow keys to select it. Use **Clear** to remove the selected route.
- A patch can use one route for each combination of eight sources and 17 destinations. **Target** moves all routes that share the selected destination to an unused destination and preserves their depths. Destinations include oscillator position, pitch, level and pan, plus filter cutoff, resonance and drive, overall pan, and per-voice amp gain. At full depth, pitch moves up to an octave, cutoff up to four octaves, and amp gain from silence to twice the voice level, depending on polarity.

## Build and test

Install Visual Studio 2022 Build Tools with MSVC x64, a Windows SDK, and C++ CMake tools. Run these commands from the project folder in **Developer PowerShell for VS 2022**. [Microsoft Build Tools setup](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc)

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
$tools = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake = Join-Path $tools 'CMake/bin/cmake.exe'
$ctest = Join-Path $tools 'CMake/bin/ctest.exe'
$ninja = Join-Path $tools 'Ninja/ninja.exe'

& $cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM=$ninja"
& $cmake --build build-release --parallel 4
& $ctest --test-dir build-release --output-on-failure
```

The app builds to `build-release/MOT11_artefacts/Release/MO-T 11.exe`. The first configuration fetches [JUCE 8.0.12](https://github.com/juce-framework/JUCE/releases/tag/8.0.12). To build offline, add `-DMOT_JUCE_DIR=C:/path/to/JUCE` to the configure command and point it to a local checkout of that release. To run the headless checks without fetching JUCE, configure a separate build directory with `-DMOT_BUILD_APP=OFF`. The JUCE dependency is subject to its upstream licensing terms. [JUCE license](https://github.com/juce-framework/JUCE/blob/8.0.12/LICENSE.md)
