
// MS20Filter.h
// ofxPDSP
// ofxMS303, MIT License, 2025
//
// High-level module wrapper for pdsp::Korg35Filter.
// Accepts cutoff in semitones (like VAFilter / SVFilter) and converts
// via PitchToFreq, so it fits naturally in existing PDSP patches.
//
// Inputs:
//   "signal"  — audio to filter                    (default input)
//   "pitch"   — cutoff in semitones                (alias: "cutoff", default 84 = C6 ≈ 1047 Hz)
//   "reso"    — resonance 0..1                     (default 0.0)
//   "drive"   — feedback saturation drive 1..4     (default 1.0)
//
// Output:
//   "signal"  — lowpass filtered audio             (default output)

#ifndef PDSP_MODULE_MS20FILTER_H_INCLUDED
#define PDSP_MODULE_MS20FILTER_H_INCLUDED

#include "../../DSP/pdspCore.h"
#include "../../DSP/filters/Korg35Filter.h"
#include "../../DSP/utility/PitchToFreq.h"

namespace pdsp {

/*!
@brief MS-20 Mk1-style Korg 35 lowpass filter.  Mono.

A higher-level module wrapping pdsp::Korg35Filter with a pitch (semitone)
cutoff input, matching the VAFilter / SVFilter API convention.

The filter is a nonlinear Sallen-Key 2-pole lowpass with tanh saturation in
the resonance feedback path.  At full resonance it self-oscillates at the
cutoff frequency.  in_drive() controls how hard the feedback clips — this is
the "MS-20 bite" characteristic absent from smoother ladder/SVF topologies.

@see pdsp::VAFilter   (Moog ladder, same API style)
@see pdsp::SVFilter   (state variable filter, same API style)
@see pdsp::Korg35Filter (the underlying raw Unit)
*/
class MS20Filter : public Patchable {
public:

    MS20Filter()                                  { patch(); }
    MS20Filter(const MS20Filter& /*other*/)       { patch(); }
    MS20Filter& operator=(const MS20Filter& /*other*/) { return *this; }

    /*!
    @brief Default input — audio signal to filter.
    */
    Patchable& in_signal();

    /*!
    @brief Cutoff control in semitones.  Default 84 (≈ C6 / 1047 Hz).
    */
    Patchable& in_pitch();

    /*!
    @brief Alias for in_pitch() — cutoff in semitones.
    */
    Patchable& in_cutoff();

    /*!
    @brief Resonance 0..1.  Default 0.0.  At ~1.0 the filter self-oscillates.
    */
    Patchable& in_reso();

    /*!
    @brief Feedback drive / bite 1..4.  Default 1.0.
    Higher values clip the resonance feedback harder — the signature MS-20 aggression.
    */
    Patchable& in_drive();

    /*!
    @brief Default output — lowpass filtered audio.
    */
    Patchable& out_signal();

    /*!
    @brief Returns the current cutoff pitch in semitones (thread-safe meter).
    */
    float meter_cutoff() const;

private:
    void patch();

    Korg35Filter filter;
    PitchToFreq  p2f;
    PatchNode    reso;
    PatchNode    drive;
};


// ─────────────────────────────────────────────────────────────────────────────
// Inline implementation
// ─────────────────────────────────────────────────────────────────────────────

inline void MS20Filter::patch()
{
    // Module I/O registration
    addModuleInput("signal", filter.in_signal()); // default input
    addModuleInput("pitch",  p2f);
    addModuleInput("reso",   reso);
    addModuleInput("drive",  drive);
    addModuleOutput("signal", filter.out_signal()); // default output

    // Internal wiring: pitch → Hz → filter cutoff
    p2f   >> filter.in_freq();
    reso  >> filter.in_reso();
    drive >> filter.in_drive();

    // Defaults: 84 semitones = C6 ≈ 1047 Hz, no resonance, no extra drive
    p2f.set(84.0f);
    reso.set(0.0f);
    drive.set(1.0f);
}

inline Patchable& MS20Filter::in_signal() { return in("signal"); }
inline Patchable& MS20Filter::in_pitch()  { return in("pitch");  }
inline Patchable& MS20Filter::in_cutoff() { return in("pitch");  }
inline Patchable& MS20Filter::in_reso()   { return in("reso");   }
inline Patchable& MS20Filter::in_drive()  { return in("drive");  }
inline Patchable& MS20Filter::out_signal(){ return out("signal"); }

inline float MS20Filter::meter_cutoff() const { return p2f.meter_output(); }

} // namespace pdsp

#endif // PDSP_MODULE_MS20FILTER_H_INCLUDED
