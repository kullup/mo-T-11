# MO-T 11

MO-T 11 is a standalone Windows x64 wavetable synth. It has 16 voices, three oscillators per voice, 11 factory wavetables, nine factory patches, and a 72-route modulation matrix. The front panel follows [Mockup.png](Mockup.png).

## Play

- Launch **MO-T 11.exe**. It opens the system audio output and listens to available Windows MIDI inputs on channels 1–16. The bottom panel shows the number of open MIDI inputs and flashes when a MIDI note arrives. Click **Audio** to select a different output.
- Play a MIDI controller or click and drag across the on-screen keys. In the synth window, play `A W S E D F T G Y H U J K` from C to C, including while editing controls. Use **−** and **+** to shift octaves. **Panic** clears sounding notes.
- **Poly** plays chords. **Mono** plays the most recently held note and retriggers the envelopes on each note change. **Legato** keeps the envelopes and key-synced LFOs running across overlapping notes. In either mono mode, release the current key to return to the previous held key.
- **Glide** sets the pitch travel time between overlapping notes in Mono or Legato. Hold one key while pressing another, or drag across the on-screen keyboard. Set it to **Off** for immediate pitch changes. **Unison** stacks up to four layers per note in any mode. Use **Detune** to separate their pitches and **Width** to spread them across the stereo field. Voicing settings are saved with the patch.
- Browse patches with the header arrows or click the patch display to jump to a factory patch. **Open** and **Save** use `.motpatch` files. The app remembers the working patch and selected audio output between launches.
- Drag a knob or fader, use the mouse wheel, or focus it and use the arrow keys. Double-click a control to return it to its Init value.

## Make a patch

- **Table Osc.** Each of the three rows has its own wavetable display and **Pos**, **Tune**, **Fine**, and **Vol** knobs. **Tune** shifts pitch by up to 24 semitones in either direction. **Fine** adjusts pitch by up to 50 cents in either direction. Click a display to choose a wavetable for that row. Under **Classic analog**, Sine, Triangle, Sawtooth, and Square are fixed shapes. With Variable Pulse, **Pos** sets the duty cycle from 5% to 95% and can be modulated for PWM. The **Analog Stack** factory patch layers sawtooth, square, and a triangle sub-oscillator.
- **Osc Setup.** Select **A**, **B**, or **C** in the setup strip to adjust its octave, stereo pan, and start phase. The octave offset adds to the **Tune** setting. **Reset** starts each note at the phase knob’s setting. **Free** varies the starting phase from note to note. **Stack/Cycle** switches that row between a stack of frames and a single waveform. Patches from earlier versions load with oscillator C muted.
- **Filter and envelopes.** Click the filter mode to cycle low-pass, band-pass, and high-pass. Use cutoff, resonance, and drive to shape the signal. The amp ADSR controls the voice’s gain. Route **Filter Env** to **Cutoff** for a filter sweep, or to another destination for a different contour.
- **LFOs.** Set rate and depth, then cycle sine, triangle, ramp, and square. **Free** runs continuously. **Key** starts a separate cycle for each played note.
- **Matrix.** Click a target in the display to reassign its column. Each column keeps its route amounts when reassigned, and each target can occupy one column. Available targets include oscillator position, pitch, level and pan, plus filter cutoff, resonance and drive, overall pan, and per-voice amp gain. Patches save the target assignments and route amounts together.
- Press a cell to select its route. Move the **Amount** fader for a direct setting or drag **Fine** for small adjustments. Amber indicates positive depth and blue indicates negative depth. **Clear** zeros the selected route. At full depth, pitch moves up to an octave, cutoff up to four octaves, and amp gain from silence to twice the voice level, depending on polarity.

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
