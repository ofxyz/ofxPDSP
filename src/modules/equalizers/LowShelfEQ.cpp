
#include "LowShelfEQ.h"

void pdsp::LowShelfEQ::patch(){
 
    addUnitInput("0", eq0);
    addUnitInput("1", eq1);
    addUnitOutput("0", eq0);
    addUnitOutput("1", eq1);
    
    addUnitInput("freq", freq);
    addUnitInput("Q", Q);
    addUnitInput("gain", gain);
    
    freq.set(10000.0f);
    Q.set(0.707f);
    gain.set(0.0f);
    
    freq >> eq0.in_freq();
    freq >> eq1.in_freq();
    Q >> eq0.in_Q();
    Q >> eq1.in_Q();
    gain >> eq0.in_gain();
    gain >> eq1.in_gain();
    
}

pdsp::Patchable& pdsp::LowShelfEQ::in_0(){
    return in("0");
}
    
pdsp::Patchable& pdsp::LowShelfEQ::in_1(){
    return in("1");
}

pdsp::Patchable& pdsp::LowShelfEQ::in_freq(){
    return in("freq");
}

pdsp::Patchable& pdsp::LowShelfEQ::in_Q(){
    return in("Q");
}

pdsp::Patchable& pdsp::LowShelfEQ::in_gain(){
    return in("gain");
}

pdsp::Patchable& pdsp::LowShelfEQ::out_0(){
    return out("0");
}

pdsp::Patchable& pdsp::LowShelfEQ::out_1(){
    return out("1");
}
