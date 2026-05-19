
// Korg35Filter.h
// ofxPDSP
// ofxMS303, MIT License, 2025
//
// 2-pole Korg 35 / MS-20 Mk1-style nonlinear Sallen-Key lowpass filter.
//
// The Korg 35 chip used in the MS-20 Mk1 is a Sallen-Key topology with
// nonlinear resonance feedback (diode saturation).  This is what gives the
// MS-20 its aggressive, self-oscillating character distinct from the smooth
// Moog ladder found in the TB-303.
//
// Model: nonlinear TPT Sallen-Key (Zavalishin / Mystran approach)
//
//   xin → (+) → one-pole stage 1 → one-pole stage 2 → y (LPF output)
//          ↑                                               |
//          └────  −k · tanh(drive · s2)  ←←←←←←←←←←←←←←┘
//
// Per-sample recurrence (s1, s2 = filter states; alpha = g/(1+g)):
//   u   = tanhf(drive * (xin − k * s2))     ← nonlinear input with feedback
//   vn1 = (u  − s1) * alpha;   s1 += 2*vn1  ← TPT one-pole stage 1
//   vn2 = (tanhf(s1) − s2) * alpha; s2 += 2*vn2  ← TPT one-pole stage 2
//   output = s2
//
// Self-oscillation at k → 2.0.  drive > 1 clamps feedback aggressively,
// producing the characteristic MS-20 "bite" / hard-clipping squeal.
//
// Inputs:
//   "signal"  — audio to filter           (default input)
//   "freq"    — cutoff in Hz              (default 4000 Hz)
//   "reso"    — resonance 0..1            (default 0.0;  maps to k = 0..1.95)
//   "drive"   — feedback saturation 1..4  (default 1.0)
//
// Output:
//   "signal"  — lowpass filtered audio    (default output)

#ifndef PDSP_FILTERS_KORG35_H_INCLUDED
#define PDSP_FILTERS_KORG35_H_INCLUDED

#include "../pdspCore.h"
#include "../math/dsphelpers/warpCutoff.h"

#include <cmath>

namespace pdsp {

/*!
@brief 2-pole Korg 35 / MS-20 Mk1-style nonlinear Sallen-Key lowpass filter.

Topology: two cascaded one-pole OTA stages with tanh saturation in the
resonance feedback path, modelling the Korg 35 chip used in the Korg MS-20 Mk1.

At resonance (in_reso) >= ~1.0 the filter self-oscillates at the cutoff
frequency.  The in_drive() input controls how aggressively the feedback path
is clipped — higher values produce the signature MS-20 "bite".

Use pdsp::MS20Filter for a higher-level module with pitch (semitone) input.
*/
class Korg35Filter : public Unit {
public:

    Korg35Filter();

    /*! @brief Default input — audio signal to filter. */
    Patchable& in_signal();

    /*! @brief Cutoff frequency in Hz.  Default 4000 Hz. */
    Patchable& in_freq();

    /*! @brief Resonance 0..1 (maps internally to k = 0..1.95).  Default 0.0. */
    Patchable& in_reso();

    /*! @brief Feedback drive / bite 1..4.  Higher = more MS-20 aggression.  Default 1.0. */
    Patchable& in_drive();

    /*! @brief Lowpass filtered output.  Default output. */
    Patchable& out_signal();

private:
    void process(int bufferSize) noexcept override;
    void prepareUnit(int expectedBufferSize, double sampleRate) override;
    void releaseResources() override;

    // Control-rate coefficient update — templated over which inputs changed.
    template<bool freqChange, bool resoChange, bool driveChange>
    inline void process_once(const float* freqBuf, const float* resoBuf, const float* driveBuf) noexcept;

    // Audio-rate inner loop — templated over which inputs are audio-rate.
    template<bool freqAR, bool resoAR, bool driveAR>
    void process_audio(const float* inBuf, const float* freqBuf,
                       const float* resoBuf, const float* driveBuf,
                       int bufferSize) noexcept;

    InputNode  input_signal;
    InputNode  input_freq;
    InputNode  input_reso;
    InputNode  input_drive;

    OutputNode output_signal;

    // Filter state (two first-order sections)
    float s1, s2;

    // Precomputed coefficients (updated from control-rate inputs)
    float alpha; // g/(1+g) where g = tan(π·fc/fs)
    float k;     // resonance feedback gain: reso * 1.95
    float drive; // saturation drive: 1..4

    float halfT;      // 0.5 / sampleRate  (for vect_warpCutoff)
    float twoSlashT;  // 2.0 * sampleRate  (for vect_warpCutoff)
};


// ─────────────────────────────────────────────────────────────────────────────
// Inline implementation
// ─────────────────────────────────────────────────────────────────────────────

inline Korg35Filter::Korg35Filter()
{
    addInput("signal", input_signal);
    addInput("freq",   input_freq);
    addInput("reso",   input_reso);
    addInput("drive",  input_drive);
    addOutput("signal", output_signal);
    updateOutputNodes();

    input_freq.setDefaultValue(4000.0f);
    input_reso.setDefaultValue(0.0f);
    input_drive.setDefaultValue(1.0f);

    input_freq.enableBoundaries(20.0f, 18000.0f);
    input_reso.enableBoundaries(0.0f, 1.0f);
    input_drive.enableBoundaries(1.0f, 4.0f);

    if (dynamicConstruction) {
        prepareToPlay(globalBufferSize, globalSampleRate);
    }
}

inline Patchable& Korg35Filter::in_signal() { return in("signal"); }
inline Patchable& Korg35Filter::in_freq()   { return in("freq");   }
inline Patchable& Korg35Filter::in_reso()   { return in("reso");   }
inline Patchable& Korg35Filter::in_drive()  { return in("drive");  }
inline Patchable& Korg35Filter::out_signal(){ return out("signal"); }

inline void Korg35Filter::prepareUnit(int /*expectedBufferSize*/, double sampleRate)
{
    s1    = 0.0f;
    s2    = 0.0f;
    k     = 0.0f;
    drive = 1.0f;

    halfT     = static_cast<float>(0.5 / sampleRate);
    twoSlashT = static_cast<float>(2.0 * sampleRate);

    // Default alpha from 4000 Hz cutoff
    float g = std::tan(static_cast<float>(M_PI) * 4000.0f / static_cast<float>(sampleRate));
    if (g > 0.999f) g = 0.999f;
    alpha = g / (1.0f + g);
}

inline void Korg35Filter::releaseResources() {}


template<bool freqChange, bool resoChange, bool driveChange>
inline void Korg35Filter::process_once(const float* freqBuf, const float* resoBuf,
                                        const float* driveBuf) noexcept
{
    if (freqChange) {
        float wa;
        vect_warpCutoff(&wa, freqBuf, halfT, twoSlashT, 1);
        float g = wa * halfT; // g = tan(π·fc/fs)
        if (g > 0.999f) g = 0.999f;
        alpha = g / (1.0f + g);
    }
    if (resoChange) {
        k = resoBuf[0] * 1.95f;
    }
    if (driveChange) {
        drive = driveBuf[0];
    }
}


template<bool freqAR, bool resoAR, bool driveAR>
void Korg35Filter::process_audio(const float* inBuf, const float* freqBuf,
                                  const float* resoBuf, const float* driveBuf,
                                  int bufferSize) noexcept
{
    float* outBuf = getOutputBufferToFill(output_signal);

    for (int n = 0; n < bufferSize; ++n) {

        if (freqAR) {
            float wa;
            vect_warpCutoff(&wa, freqBuf + n, halfT, twoSlashT, 1);
            float g = wa * halfT;
            if (g > 0.999f) g = 0.999f;
            alpha = g / (1.0f + g);
        }
        if (resoAR)  k     = resoBuf[n]  * 1.95f;
        if (driveAR) drive = driveBuf[n];

        // ── Korg 35 nonlinear TPT recurrence ───────────────────────────────
        //
        // Stage 1: nonlinear input gain + feedback subtraction
        float u   = tanhf(drive * (inBuf[n] - k * s2));
        float vn1 = (u - s1) * alpha;
        s1 += 2.0f * vn1;

        // Stage 2: inter-stage saturation (gives the "mid-bite" character)
        float vn2 = (tanhf(s1) - s2) * alpha;
        s2 += 2.0f * vn2;

        outBuf[n] = s2;
    }
}


inline void Korg35Filter::process(int bufferSize) noexcept
{
    int inputState;
    const float* inputBuf = processInput(input_signal, inputState);

    if (inputState == AudioRate) {
        int freqState, resoState, driveState;
        const float* freqBuf  = processInput(input_freq,  freqState);
        const float* resoBuf  = processInput(input_reso,  resoState);
        const float* driveBuf = processInput(input_drive, driveState);

        // Switcher encodes all three input states with non-overlapping multipliers:
        //   freq:  Unchanged=0, Changed=1,  AudioRate=2
        //   reso:  Unchanged=0, Changed=4,  AudioRate=8   (state * 4)
        //   drive: Unchanged=0, Changed=16, AudioRate=32  (state * 16)
        int sw = freqState + resoState * 4 + driveState * 16;

        // Update control-rate coefficients (Changed bits: 1, 4, 16 → mask 21)
        switch (sw & 21) {
            case  0: process_once<false,false,false>(freqBuf, resoBuf, driveBuf); break;
            case  1: process_once<true, false,false>(freqBuf, resoBuf, driveBuf); break;
            case  4: process_once<false,true, false>(freqBuf, resoBuf, driveBuf); break;
            case  5: process_once<true, true, false>(freqBuf, resoBuf, driveBuf); break;
            case 16: process_once<false,false,true >(freqBuf, resoBuf, driveBuf); break;
            case 17: process_once<true, false,true >(freqBuf, resoBuf, driveBuf); break;
            case 20: process_once<false,true, true >(freqBuf, resoBuf, driveBuf); break;
            case 21: process_once<true, true, true >(freqBuf, resoBuf, driveBuf); break;
            default: break;
        }

        // Run audio-rate inner loop (AudioRate bits: 2, 8, 32 → mask 42)
        switch (sw & 42) {
            case  0: process_audio<false,false,false>(inputBuf,freqBuf,resoBuf,driveBuf,bufferSize); break;
            case  2: process_audio<true, false,false>(inputBuf,freqBuf,resoBuf,driveBuf,bufferSize); break;
            case  8: process_audio<false,true, false>(inputBuf,freqBuf,resoBuf,driveBuf,bufferSize); break;
            case 10: process_audio<true, true, false>(inputBuf,freqBuf,resoBuf,driveBuf,bufferSize); break;
            case 32: process_audio<false,false,true >(inputBuf,freqBuf,resoBuf,driveBuf,bufferSize); break;
            case 34: process_audio<true, false,true >(inputBuf,freqBuf,resoBuf,driveBuf,bufferSize); break;
            case 40: process_audio<false,true, true >(inputBuf,freqBuf,resoBuf,driveBuf,bufferSize); break;
            case 42: process_audio<true, true, true >(inputBuf,freqBuf,resoBuf,driveBuf,bufferSize); break;
            default: break;
        }
    } else {
        setOutputToZero(output_signal);
    }
}

} // namespace pdsp

#endif // PDSP_FILTERS_KORG35_H_INCLUDED
