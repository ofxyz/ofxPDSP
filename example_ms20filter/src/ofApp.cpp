#include "ofApp.h"

// ─────────────────────────────────────────────────────────────────────────────
// Acid pattern — classic rolling 303 bassline in A minor
// Notes are MIDI semitones: 33=A1, 45=A2, 57=A3, etc.
// ─────────────────────────────────────────────────────────────────────────────
void ofApp::loadAcidPattern()
{
    //         note  rest   accent slide
    pattern[ 0] = { 33, false, true,  false }; // A1 (accent)
    pattern[ 1] = { 33, false, false, true  }; // A1 (slide)
    pattern[ 2] = { 36, false, false, true  }; // C2 (slide)
    pattern[ 3] = { 40, false, false, false }; // E2
    pattern[ 4] = {  0, true,  false, false }; // rest
    pattern[ 5] = { 45, false, true,  false }; // A2 (accent)
    pattern[ 6] = { 45, false, false, true  }; // A2 (slide)
    pattern[ 7] = { 48, false, false, false }; // C3
    pattern[ 8] = { 43, false, false, true  }; // G2 (slide)
    pattern[ 9] = { 45, false, true,  false }; // A2 (accent)
    pattern[10] = {  0, true,  false, false }; // rest
    pattern[11] = { 38, false, false, false }; // D2
    pattern[12] = { 40, false, false, true  }; // E2 (slide)
    pattern[13] = { 43, false, false, false }; // G2
    pattern[14] = { 36, false, true,  false }; // C2 (accent)
    pattern[15] = {  0, true,  false, false }; // rest
}

void ofApp::triggerStep(int step)
{
    const Step& s = pattern[step];
    if (s.rest) {
        synth.gate.off();
        return;
    }

    float vel   = s.accent ? 1.0f : 0.65f;
    float slide = s.slide  ? 60.0f : 8.0f; // portamento ms

    synth.pitch.enableSmoothing(slide);
    synth.pitch.set(static_cast<float>(s.note));

    // Accent: boost cutoff by +12 semitones (one-step scream)
    float accentBoost = s.accent ? 12.0f : 0.0f;
    synth.cutoff.set(dispCutoff + accentBoost);

    synth.gate.trigger(vel);
}

void ofApp::updateFilterFromMouse(int x, int y)
{
    // X = cutoff (C1=24 → C8=108), Y = resonance (0 → 1.05 allowing self-osc)
    dispCutoff = ofMap(x, 0, ofGetWidth(),  24.0f, 108.0f, true);
    dispReso   = ofMap(y, ofGetHeight(), 0, 0.0f,  1.05f,  true);
    synth.cutoff.set(dispCutoff);
    synth.reso.set(dispReso);
}

void ofApp::setup()
{
    ofSetFrameRate(60);
    ofBackground(10, 10, 14);
    ofSetWindowTitle("ofxPDSP — MS20Filter / Korg35 demo");

    loadAcidPattern();

    // ── Patch voice → engine outputs (stereo, -6 dB headroom) ────────────
    synth.out_L() * dB(-6.0f) >> engine.audio_out(0);
    synth.out_R() * dB(-6.0f) >> engine.audio_out(1);

    // Pick the system default output, falling back to any stereo device.
    auto devices = engine.listDevices();
    int chosen = -1;
    for (size_t i = 0; i < devices.size(); ++i) {
        if (devices[i].isDefaultOutput && devices[i].outputChannels > 0) {
            chosen = static_cast<int>(i);
            break;
        }
    }
    if (chosen < 0) {
        for (size_t i = 0; i < devices.size(); ++i) {
            if (devices[i].outputChannels >= 2) {
                chosen = static_cast<int>(i);
                break;
            }
        }
    }
    if (chosen < 0) chosen = 0;

    engine.setOutputDeviceID(chosen);
    engine.setup(44100, 512, 3);
}

void ofApp::update()
{
    if (!playing) return;

    float stepDurationSec = 60.0f / (bpm * 4.0f); // 16th note
    stepTimer += ofGetLastFrameTime();

    if (stepTimer >= stepDurationSec) {
        stepTimer -= stepDurationSec;
        triggerStep(currentStep);
        currentStep = (currentStep + 1) % NUM_STEPS;
    }
}

void ofApp::draw()
{
    int winW = ofGetWidth();
    int winH = ofGetHeight();

    // FBO height is the fixed design resolution; width follows the window
    // aspect ratio so the UI never gets letterboxed.
    int fboW = std::max(1, static_cast<int>(std::round(winW * kDesignH / static_cast<float>(winH))));
    int fboH = static_cast<int>(kDesignH);

    if (!uiFbo.isAllocated()
        || uiFbo.getWidth()  != fboW
        || uiFbo.getHeight() != fboH) {
        ofFboSettings s;
        s.width          = fboW;
        s.height         = fboH;
        s.internalformat = GL_RGBA;
        s.numSamples     = 0;
        uiFbo.allocate(s);
        uiFbo.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    }

    uiW = fboW;

    uiFbo.begin();
    ofClear(10, 10, 14, 255);
    drawUI();
    uiFbo.end();

    uiFbo.draw(0, 0, winW, winH);
}

void ofApp::drawUI()
{
    int W = uiW;
    int H = static_cast<int>(kDesignH);

    // ── Title ─────────────────────────────────────────────────────────────
    ofSetColor(220, 180, 60);
    ofDrawBitmapString("pdsp::MS20Filter  /  Korg35Filter  demo", 20, 28);
    ofSetColor(130, 130, 140);
    ofDrawBitmapString("Nonlinear TPT Sallen-Key · self-oscillation · MS-20 bite", 20, 44);

    // ── Cutoff bar ────────────────────────────────────────────────────────
    float cutoffNorm = ofMap(dispCutoff, 24.0f, 108.0f, 0.0f, 1.0f, true);
    ofSetColor(40, 40, 50);
    ofDrawRectangle(20, 80, W - 40, 18);
    ofSetColor(60, 160, 220);
    ofDrawRectangle(20, 80, (W - 40) * cutoffNorm, 18);
    ofSetColor(200);
    ofDrawBitmapString("CUTOFF   " + ofToString(dispCutoff, 0) + " st  (mouse X)", 24, 93);

    // ── Reso bar — colour shifts red near self-oscillation ────────────────
    float resoNorm = ofMap(dispReso, 0.0f, 1.05f, 0.0f, 1.0f, true);
    ofSetColor(40, 40, 50);
    ofDrawRectangle(20, 110, W - 40, 18);
    ofColor resoCol = resoNorm < 0.8f
        ? ofColor(220, 80, 200)                                         // purple
        : ofColor(220 + (resoNorm - 0.8f) * 5.f * 35, 40, 40);        // red = danger
    ofSetColor(resoCol);
    ofDrawRectangle(20, 110, (W - 40) * resoNorm, 18);
    string resoLabel = dispReso >= 0.95f ? "  ← SELF-OSC" : "";
    ofSetColor(200);
    ofDrawBitmapString("RESO     " + ofToString(dispReso, 2) + resoLabel + "  (mouse Y)", 24, 123);

    // ── Drive bar ─────────────────────────────────────────────────────────
    float driveNorm = ofMap(dispDrive, 1.0f, 4.0f, 0.0f, 1.0f, true);
    ofSetColor(40, 40, 50);
    ofDrawRectangle(20, 140, W - 40, 18);
    ofSetColor(220, 120, 40);
    ofDrawRectangle(20, 140, (W - 40) * driveNorm, 18);
    ofSetColor(200);
    ofDrawBitmapString("DRIVE    " + ofToString(dispDrive, 1) + "  (keys 1-4)", 24, 153);

    // ── VCA envelope meter ────────────────────────────────────────────────
    float envLevel = synth.meter_env();
    ofSetColor(40, 40, 50);
    ofDrawRectangle(20, 170, W - 40, 18);
    ofSetColor(60, 220, 80);
    ofDrawRectangle(20, 170, (W - 40) * envLevel, 18);
    ofSetColor(200);
    ofDrawBitmapString("VCA ENV  " + ofToString(envLevel, 2), 24, 183);

    // ── Step sequencer grid ───────────────────────────────────────────────
    int gridX = 20, gridY = 220;
    int cellW = (W - 40) / NUM_STEPS;
    int cellH = 36;

    for (int i = 0; i < NUM_STEPS; ++i) {
        const Step& s = pattern[i];
        bool active   = (i == (currentStep - 1 + NUM_STEPS) % NUM_STEPS) && playing;

        ofColor bg = s.rest ? ofColor(25, 25, 30) : ofColor(45, 45, 55);
        if (s.accent) bg = ofColor(80, 55, 20);
        if (active)   bg = ofColor(200, 160, 40);

        ofSetColor(bg);
        ofDrawRectangle(gridX + i * cellW + 1, gridY, cellW - 2, cellH);

        if (!s.rest) {
            // Note pitch indicator (vertical within cell)
            float noteNorm = ofMap(s.note, 24, 60, 1.0f, 0.0f, true);
            ofSetColor(active ? ofColor(255) : ofColor(140, 200, 255));
            ofDrawRectangle(gridX + i * cellW + 1, gridY + cellH * noteNorm, cellW - 2, 3);

            // Slide marker
            if (s.slide) {
                ofSetColor(80, 200, 180);
                ofDrawRectangle(gridX + i * cellW + cellW - 4, gridY + cellH - 6, 3, 5);
            }
        }
    }
    ofSetColor(140);
    ofDrawBitmapString("PATTERN  (16 steps, 130 BPM acid riff)", gridX, gridY + cellH + 14);

    // ── Keys legend ───────────────────────────────────────────────────────
    int ly = 310;
    ofSetColor(100, 100, 110);
    ofDrawBitmapString("KEYS", 20, ly);
    ofSetColor(180);
    ofDrawBitmapString("SPACE  — play / stop", 20, ly + 16);
    ofDrawBitmapString("1      — drive 1.0  (clean)", 20, ly + 32);
    ofDrawBitmapString("2      — drive 1.5  (warm bite)", 20, ly + 48);
    ofDrawBitmapString("3      — drive 2.5  (MS-20 crunch)", 20, ly + 64);
    ofDrawBitmapString("4      — drive 4.0  (full aggression)", 20, ly + 80);
    ofDrawBitmapString("UP/DN  — BPM  " + ofToString(bpm, 0), 20, ly + 96);
    ofDrawBitmapString("MOUSE  — move to sweep cutoff (X) + resonance (Y)", 20, ly + 112);

    // ── Status bar ────────────────────────────────────────────────────────
    ofSetColor(playing ? ofColor(60, 220, 80) : ofColor(180, 60, 60));
    ofDrawBitmapString(playing ? "▶  PLAYING" : "■  STOPPED", 20, H - 20);
    ofSetColor(120);
    ofDrawBitmapString("BPM " + ofToString(bpm, 0), W - 80, H - 20);
}

void ofApp::mouseMoved(int x, int y)    { updateFilterFromMouse(x, y); }
void ofApp::mouseDragged(int x, int y, int) { updateFilterFromMouse(x, y); }

void ofApp::mousePressed(int x, int y, int button)  {}
void ofApp::mouseReleased(int x, int y, int button) {}

void ofApp::keyPressed(int key)
{
    switch (key) {
        case ' ':
            playing = !playing;
            if (!playing) synth.gate.off();
            break;
        case '1': dispDrive = 1.0f;  synth.drive.set(dispDrive); drivePreset = 1; break;
        case '2': dispDrive = 1.5f;  synth.drive.set(dispDrive); drivePreset = 2; break;
        case '3': dispDrive = 2.5f;  synth.drive.set(dispDrive); drivePreset = 3; break;
        case '4': dispDrive = 4.0f;  synth.drive.set(dispDrive); drivePreset = 4; break;
        case OF_KEY_UP:
            bpm = std::min(bpm + 5.0f, 220.0f);
            break;
        case OF_KEY_DOWN:
            bpm = std::max(bpm - 5.0f, 60.0f);
            break;
    }
}

void ofApp::keyReleased(int key)    {}
void ofApp::windowResized(int w, int h) {}
void ofApp::dragEvent(ofDragInfo)   {}
void ofApp::gotMessage(ofMessage)   {}
