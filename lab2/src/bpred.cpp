#include "bpred.h"

#define TAKEN   true
#define NOTTAKEN false

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

BPRED::BPRED(uint32_t policy) {
    if(policy == 1)
    {
        (*this).policy = BPRED_ALWAYS_TAKEN;
    }
    else if(policy == 2)
    {
        (*this).policy = BPRED_GSHARE;
    }
    ghr = 0;
    stat_num_branches = 0;
    stat_num_mispred = 0;
    pht.assign(4096, 2); // initialized to weak taken state
    pht_index = 0;
    policy = 0;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool BPRED::GetPrediction(uint32_t PC){
    if(policy == BPRED_ALWAYS_TAKEN)
    {
        return TAKEN;
    }
    else if(policy == BPRED_GSHARE)
    {
        // pht index = PC last 12 bits XOR global history registers
        pht_index = ( PC & 0xfff ) ^ ( ghr & 0xfff );
        // Return predict direction
        if(pht[pht_index] <= 1)
            return NOTTAKEN;
        else 
            return TAKEN;  
    }
    else
    {
        return TAKEN;
    }
    
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  BPRED::UpdatePredictor(uint32_t PC, bool resolveDir, bool predDir) {
    if(policy == BPRED_GSHARE){
        // Get PHT index, which is the xor of last 12 bits in PC and GHR
        pht_index = ( PC & 0xfff ) ^ ( ghr & 0xfff );
        // Update GHR, shift the direction of resolved direction
        ghr = (ghr << 1) + resolveDir;
        if(resolveDir == NOTTAKEN)
        {
            pht[pht_index] = SatDecrement (pht[pht_index]);
        }
        else
        {
            pht[pht_index] = SatIncrement (pht[pht_index], 3);
        }
    }
    
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

