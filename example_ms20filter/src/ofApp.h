#pragma once

#include "ofMain.h"
#include "ofxPDSP.h"

// ─────────────────────────────────────────────────────────────────────────────
// MS20Voice — a minimal Korg35/MS20Filter demo voice
//
// Signal chain:
//   VAOscillator (saw + pulse blend)
//     → Saturator1         (pre-filter grit)
//     → MS20Filter         ← cutoff, reso, drive
//     → AHR envelope (VCA)
//     → output
// ─────────────────────────────────────────────────────────────────────────────
class MS20Voice : public pdsp::Patchable {
public:
    MS20Voice()                     { patch(); }
    MS20Voice(const MS20Voice&)     { patch(); }

    pdsp::TriggerControl gate;
    pdsp::ValueControl   pitch;
    pdsp::ValueControl   cutoff;
    pdsp::ValueControl   reso;
    pdsp::ValueControl   drive;
    pdsp::ValueControl   envMod;    // how many semitones the VCF env sweeps
    pdsp::ValueControl   decay;     // VCF + VCA decay ms

    pdsp::Patchable& out_L() { return out("L"); }
    pdsp::Patchable& out_R() { return out("R"); }

    float meter_env() const { return vcaEnv.meter_output(); }

private:
    void patch() {
        // OSC: saw + soft-square mix
        osc.out_saw()          >> sawMix;
        osc.out_pulse() * 0.4f >> sawMix;
        0.5f                   >> osc.in_pw();
        pitch                  >> osc.in_pitch();
        pitch.enableSmoothing(40.0f); // portamento-style smoothing

        // Pre-filter drive
        sawMix >> sat;

        // Filter: Korg35 lowpass
        sat         >> filter.in_signal();
        reso        >> filter.in_reso();
        drive       >> filter.in_drive();

        // Cutoff = base + env sweep
        cutoff              >> filter.in_cutoff();
        vcfEnvAmp           >> filter.in_cutoff(); // sums into input
        vcfEnv              >> vcfEnvAmp.in_signal();
        envMod              >> vcfEnvAmp.in_mod();

        gate     >> vcfEnv.in_trig();
        1.0f     >> vcfEnv.in_attack();
        decay    >> vcfEnv.in_hold();
        100.0f   >> vcfEnv.in_release();
        vcfEnv.setCurve(1.0f);

        // VCA
        filter.out_signal() >> vca;
        gate   >> vcaEnv.in_trig();
        vcaEnv >> vca.in_mod();
        1.0f   >> vcaEnv.in_attack();
        decay  >> vcaEnv.in_hold();
        60.0f  >> vcaEnv.in_release();
        vcaEnv.setCurve(1.0f);

        // Outputs
        vca >> leftOut;
        vca >> rightOut;
        addModuleOutput("L", leftOut);
        addModuleOutput("R", rightOut);

        // Defaults
        pitch.set(36.0f);
        cutoff.set(60.0f);
        reso.set(0.4f);
        drive.set(1.5f);
        envMod.set(24.0f);
        decay.set(180.0f);
    }

    pdsp::VAOscillator  osc;
    pdsp::PatchNode     sawMix;
    pdsp::Saturator1    sat;
    pdsp::MS20Filter    filter;
    pdsp::Amp           vcfEnvAmp;
    pdsp::AHR           vcfEnv;
    pdsp::AHR           vcaEnv;
    pdsp::Amp           vca;
    pdsp::PatchNode     leftOut;
    pdsp::PatchNode     rightOut;
};


// ─────────────────────────────────────────────────────────────────────────────
// ofApp
// ─────────────────────────────────────────────────────────────────────────────
class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo);
    void gotMessage(ofMessage);
    void mouseEntered(int x, int y) {}
    void mouseExited(int x, int y)  {}

private:
    pdsp::Engine  engine;
    MS20Voice     synth;

    // ── Acid sequencer ────────────────────────────────────────────────────
    // Classic 303-style pattern — 16 steps
    struct Step {
        int   note;     // MIDI semitones (0 = rest)
        bool  rest;
        bool  accent;
        bool  slide;
    };

    static constexpr int   NUM_STEPS = 16;
    static constexpr float DEFAULT_BPM = 130.0f;

    Step    pattern[NUM_STEPS];
    int     currentStep  {0};
    float   stepTimer    {0.0f};
    float   bpm          {DEFAULT_BPM};
    bool    playing      {true};

    // ── Display state ─────────────────────────────────────────────────────
    float dispCutoff  {60.0f};
    float dispReso    {0.4f};
    float dispDrive   {1.5f};
    float dispBpm     {DEFAULT_BPM};
    int   drivePreset {1};  // 1-4

    // ── UI scaling (pixel-art style) ──────────────────────────────────────
    // The UI is composed at a low "design" resolution into an offscreen
    // FBO, then upscaled to the window with nearest-neighbour filtering so
    // every shape and glyph becomes a chunky pixel block on HiDPI / 4K.
    static constexpr float kDesignH = 480.0f;
    ofFbo uiFbo;
    int   uiW {1280};

    void loadAcidPattern();
    void triggerStep(int step);
    void updateFilterFromMouse(int x, int y);
    void drawUI();
};
