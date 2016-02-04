
#include "ofxMidiInProcessor.h"

#define OFXPDSP_MIDIKEYSPROCESSOR_MESSAGERESERVE 128


ofxMidiInProcessor::ofxMidiInProcessor(){
    midiMessageVectorA.reserve(OFXPDSP_MIDIKEYSPROCESSOR_MESSAGERESERVE);
    midiMessageVectorA.clear();
    midiMessageVectorB.reserve(OFXPDSP_MIDIKEYSPROCESSOR_MESSAGERESERVE);
    midiMessageVectorA.clear();
    readVector = &midiMessageVectorA;
    writeVector = &midiMessageVectorB;
    
    midiIn_p = nullptr;
    connected = false;
    
}

ofxMidiInProcessor::~ofxMidiInProcessor(){
    closePort();
}

void ofxMidiInProcessor::linkToMidiIn(ofxMidiIn &midiInput){
    if(connected){
        closePort();
    }
    
    midiIn_p = &midiInput;
    midiIn_p->addListener(this); // add ofApp as a listener
    connected = true;
}


void ofxMidiInProcessor::openPort(int index){
    if(connected){
        closePort();
    }
    
    midiIn.openPort(index); 
    
    if(midiIn.isOpen()){
        midiIn_p = &midiIn;
        midiIn_p->addListener(this); // add ofApp as a listener
        connected = true;
    }
}

void ofxMidiInProcessor::closePort(){
    if(connected){
        connected = false; 
        midiIn_p->removeListener(this);
        if(midiIn.isOpen()){
            midiIn.closePort();
        }       
        midiIn_p = nullptr;
    }
}

void ofxMidiInProcessor::listPorts(){
    midiIn.listPorts(); // print input ports to console
}


void ofxMidiInProcessor::prepareToPlay( int expectedBufferSize, double sampleRate ){
    oneSlashMicrosecForSample = 1.0 / (1000000.0 / sampleRate);
}

void ofxMidiInProcessor::releaseResources(){}




void ofxMidiInProcessor::newMidiMessage(ofxMidiMessage& eventArgs) noexcept{
    	
    midiMutex.lock();
        // calculate the right offset inside the bufferSize
        chrono::duration<double> offset = chrono::high_resolution_clock::now() - bufferChrono; 
        int sampleOffset =  static_cast<int>(  
            static_cast <double>( chrono::duration_cast<chrono::microseconds>(offset).count()) * oneSlashMicrosecForSample);
        writeVector->push_back(_ofxPositionedMidiMessage(eventArgs, sampleOffset));
    midiMutex.unlock();
    
}

void ofxMidiInProcessor::processMidi( const int &bufferSize ) noexcept{
    if(connected){
        midiMutex.lock();
            //switch buffers
            vector<_ofxPositionedMidiMessage>* temp = readVector;
            readVector = writeVector;
            writeVector = temp;
            //update chrono
            bufferChrono = chrono::high_resolution_clock::now();
            //clear buffer to write
            writeVector->clear(); 
        midiMutex.unlock();
        
        //now sanitize messages to bufferSize
        for(_ofxPositionedMidiMessage &msg : *readVector){
            if(msg.sample >= bufferSize){ msg.sample = bufferSize-1; }
        }            
    }
}

