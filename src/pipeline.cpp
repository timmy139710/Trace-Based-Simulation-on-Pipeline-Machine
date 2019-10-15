/***********************************************************************
 * File         : pipeline.cpp
 * Author       : Soham J. Desai 
 * Date         : 14th January 2014
 * Description  : Superscalar Pipeline for Lab2 ECE 6100
 **********************************************************************/

#include "pipeline.h"
#include <cstdlib>

extern int32_t PIPE_WIDTH;
extern int32_t ENABLE_MEM_FWD;
extern int32_t ENABLE_EXE_FWD;
extern int32_t BPRED_POLICY;

/**********************************************************************
 * Support Function: Read 1 Trace Record From File and populate Fetch Op
 **********************************************************************/

void pipe_get_fetch_op(Pipeline *p, Pipeline_Latch* fetch_op){
    uint8_t bytes_read = 0;
    bytes_read = fread(&fetch_op->tr_entry, 1, sizeof(Trace_Rec), p->tr_file);

    // check for end of trace
    if( bytes_read < sizeof(Trace_Rec)) {
      fetch_op->valid=false;
      p->halt_op_id=p->op_id_tracker;
      return;
    }

    // got an instruction ... hooray!
    fetch_op->valid=true;
    fetch_op->stall=false;
    fetch_op->is_mispred_cbr=false;
    p->op_id_tracker++;
    fetch_op->op_id=p->op_id_tracker;
    
    return; 
}


/**********************************************************************
 * Pipeline Class Member Functions 
 **********************************************************************/

Pipeline * pipe_init(FILE *tr_file_in){
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Initialize Pipeline Internals
    Pipeline *p = (Pipeline *) calloc (1, sizeof (Pipeline));

    p->tr_file = tr_file_in;
    p->halt_op_id = ((uint64_t)-1) - 3;           

    // Allocated Branch Predictor
    if(BPRED_POLICY){
      p->b_pred = new BPRED(BPRED_POLICY);
    }

    return p;
}


/**********************************************************************
 * Print the pipeline state (useful for debugging)
 **********************************************************************/

void pipe_print_state(Pipeline *p){
    std::cout << "--------------------------------------------" << std::endl;
    std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << std::endl;

    uint8_t latch_type_i = 0;   // Iterates over Latch Types
    uint8_t width_i      = 0;   // Iterates over Pipeline Width
    for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
        switch(latch_type_i) {
            case 0:
                printf(" IF: ");
                break;
            case 1:
                printf(" ID: ");
                break;
            case 2:
                printf(" EX: ");
                break;
            case 3:
                printf(" MA: ");
                break;
            default:
                printf(" ---- ");
        }
    }
    printf("\n");
    for(width_i = 0; width_i < PIPE_WIDTH; width_i++) {
        for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
            if(p->pipe_latch[latch_type_i][width_i].valid == true) {
	      printf(" %6u ",(uint32_t)( p->pipe_latch[latch_type_i][width_i].op_id));
            } else {
                printf(" ------ ");
            }
        }
        printf("\n");
    }
    printf("\n");

}


/**********************************************************************
 * Pipeline Main Function: Every cycle, cycle the stage 
 **********************************************************************/

void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;
    
    // if(p->stat_num_cycle <= 150)
    // {
    //   pipe_print_state(p);
    // }

    pipe_cycle_WB(p);
    pipe_cycle_MA(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_IF(p);
    
    // if(p->stat_num_cycle <= 1000)
    // {
    //   pipe_print_state(p);
    // }
}
/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

void insertion_sort(Pipeline *p){
  // implement insertion sort to sort the order between superscalars
  int i, j;
  for(int i = 1; i < PIPE_WIDTH; ++i){
    Pipeline_Latch temp = p->pipe_latch[IF_LATCH][i];
    j = i - 1;
    while
      ( j >= 0 
        && 
        ( (p->pipe_latch[IF_LATCH][j].op_id > temp.op_id && temp.valid) \
          ||
          (!p->pipe_latch[IF_LATCH][j].valid && temp.valid)
        )
      )
    {
      p->pipe_latch[IF_LATCH][j+1] = p->pipe_latch[IF_LATCH][j];
      --j;
    }
    p->pipe_latch[IF_LATCH][j+1] = temp;
  }
}

// check RAW hazard when no forwarding
bool checkRAW(Trace_Rec &oldInst, Trace_Rec &fetchInst)
{
  // If src1 or src2 match dest and src op is load instruction, stall the instruction
  // Otherwise, forward the data so there is no stall 
  return ((
        (fetchInst.src1_needed && \
        (fetchInst.src1_reg == oldInst.dest) )
          ||
        (fetchInst.src2_needed && \
        (fetchInst.src2_reg == oldInst.dest))
        )
        &&
      (oldInst.dest_needed));
}

// check status register when no forwarding
bool checkCC(Trace_Rec &oldInst, Trace_Rec &fetchInst)
{
  return(oldInst.cc_write && fetchInst.cc_read);
}

void pipe_cycle_WB(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    if(p->pipe_latch[MA_LATCH][ii].valid){
      p->stat_retired_inst++;
      if(p->pipe_latch[MA_LATCH][ii].op_id >= p->halt_op_id){
        p->halt=true;
      }
      if(p->pipe_latch[MA_LATCH][ii].is_mispred_cbr){
        p->fetch_cbr_stall = false; // pipeline resolved
      }
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_MA(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[MA_LATCH][ii].valid = p->pipe_latch[EX_LATCH][ii].valid;
    if(p->pipe_latch[MA_LATCH][ii].valid){
      p->pipe_latch[MA_LATCH][ii]=p->pipe_latch[EX_LATCH][ii];
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_EX(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[EX_LATCH][ii].valid = !p->pipe_latch[ID_LATCH][ii].stall;
    if(p->pipe_latch[EX_LATCH][ii].valid){
      p->pipe_latch[EX_LATCH][ii]=p->pipe_latch[ID_LATCH][ii];
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_ID(Pipeline *p){
  
  int ii;
  insertion_sort(p); // sort superscalar pipeline for in-order operations

  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[ID_LATCH][ii].stall = false;

    // If older pipeline is stall, then stall newer pipeline
    if(ii > 0){
      int j = ii - 1;
      while(j >= 0){
        if(p->pipe_latch[ID_LATCH][j].stall){
          p->pipe_latch[ID_LATCH][ii].stall = true;
        }
        --j;
      }
    }

    // Instruction going to be fetched
    Trace_Rec fetchInst = p->pipe_latch[IF_LATCH][ii].tr_entry;

    // Check dependency in EX stage
    for(int cur = 0; cur < PIPE_WIDTH; cur++)
    {
      Trace_Rec oldInst = p->pipe_latch[EX_LATCH][cur].tr_entry;

      if(ENABLE_EXE_FWD)
      {
        // RAW dependency
        if(p->pipe_latch[EX_LATCH][cur].valid)
        {
          if(!p->pipe_latch[ID_LATCH][ii].stall && checkRAW(oldInst, fetchInst) && oldInst.op_type == OP_LD)
          {
            p->pipe_latch[ID_LATCH][ii].stall = true;
          }
        }

        // check cc_read and cc_write
        if(fetchInst.cc_read)
        {
          if(p->pipe_latch[EX_LATCH][cur].valid && checkCC(oldInst, fetchInst)){
              p->pipe_latch[ID_LATCH][ii].stall = (oldInst.op_type == OP_LD)? true : false;  
          }
        }
      }
      else
      {
        if(p->pipe_latch[EX_LATCH][cur].valid)
        { 
          // RAW dependency
          if(!p->pipe_latch[ID_LATCH][ii].stall)
          {
            p->pipe_latch[ID_LATCH][ii].stall = checkRAW(oldInst, fetchInst);
          }

          // check cc_read and cc_write dependency, only branch will have cc_read
          if(!p->pipe_latch[ID_LATCH][ii].stall)
          {
            p->pipe_latch[ID_LATCH][ii].stall = checkCC(oldInst, fetchInst);
          }
        }
      } // end ENABLE_EXE_FWD statement
    }

    if(ENABLE_MEM_FWD){
      // todo
      // No need to stall for LD because register write and read could complete in 1 cycle
    }
    else // Does not enable memory forwarding in MEM stage 
    { 
      // Check dependence for super-scalar pipeline
      for(int cur = 0; cur < PIPE_WIDTH; cur++)
      {
        Trace_Rec oldInst = p->pipe_latch[MA_LATCH][cur].tr_entry;

        if(p->pipe_latch[MA_LATCH][cur].valid)
        {
          // RAW hazard
          if(!p->pipe_latch[ID_LATCH][ii].stall)
          {
            p->pipe_latch[ID_LATCH][ii].stall = checkRAW(oldInst, fetchInst);
          }
          // check cc_read and cc_write dependency, only branch will have cc_read
          if(!p->pipe_latch[ID_LATCH][ii].stall)
          {
            p->pipe_latch[ID_LATCH][ii].stall = checkCC(oldInst, fetchInst);
          }
        }
      }
    } // end ENABLE_MEM_FWD statement

    // Data dependency between ID latchs
    // In super-scalar machine, there might be hazards between instructions in ID stage
    // pipeline with smaller ii index is older due to the fetch order in IF stage
    for(int old = 0; old < ii; old++)
    {
      Trace_Rec oldInst = p->pipe_latch[ID_LATCH][old].tr_entry;

      if(p->pipe_latch[ID_LATCH][old].valid)
      {
        // check RAW dependency between pipes
        if(!p->pipe_latch[ID_LATCH][ii].stall)
        {
          p->pipe_latch[ID_LATCH][ii].stall = checkRAW(oldInst, fetchInst);
        }
        // check status register dependency between pipes
        if(!p->pipe_latch[ID_LATCH][ii].stall)
        {
          p->pipe_latch[ID_LATCH][ii].stall = checkCC(oldInst, fetchInst);
        }
      }
    }
    
    // If there is misprediction and IF latch does not contains validinstruction, stall ID latch
    if(p->fetch_cbr_stall && !p->pipe_latch[IF_LATCH][ii].valid){
      p->pipe_latch[ID_LATCH][ii].stall = true;
    }

    // If  stall, no longer valid to receive new instructions 
    if(p->pipe_latch[ID_LATCH][ii].stall){
      p->pipe_latch[ID_LATCH][ii].valid = false;
    }
    else{
      // no longer valid to accept new instruction
      p->pipe_latch[ID_LATCH][ii] = p->pipe_latch[IF_LATCH][ii];
    }
    
  } // end pipe_width for loop
}

//--------------------------------------------------------------------//

void pipe_cycle_IF(Pipeline *p){
  int ii;
  Pipeline_Latch fetch_op;
  bool tr_read_success;

  for(ii=0; ii<PIPE_WIDTH; ii++){
    // If older pipeline is mispred, newer pipeline is not valid
    // If there is corresponding stall in ID stage, 
    if(p->fetch_cbr_stall)
    {
      // However, if newer pipeline has a stall in ID stage
      // Means that currently fetch latch still has valid instruction that hasn't been passed to ID stage
      if(p->pipe_latch[IF_LATCH][ii].valid && p->pipe_latch[ID_LATCH][ii].stall)
      {
        p->pipe_latch[IF_LATCH][ii].valid = true;
      }
      else
      {
        p->pipe_latch[IF_LATCH][ii].valid = false;
      }
    }
    else
    {
      p->pipe_latch[IF_LATCH][ii].valid = true;
    }

    // If older pipeline is mispred, newer pipeline does not fetch
    // If there is no stall or mispred, fetch new instruction
    if(!p->pipe_latch[ID_LATCH][ii].stall && !p->fetch_cbr_stall)
    {
      pipe_get_fetch_op(p, &fetch_op);

      // If it is branch, check predictor
      if(BPRED_POLICY && fetch_op.tr_entry.op_type == OP_CBR)
      {
        pipe_check_bpred(p, &fetch_op);
      }
  
      // copy the op in IF LATCH
      p->pipe_latch[IF_LATCH][ii] = fetch_op;
      // if(p->pipe_latch[IF_LATCH][ii].tr_entry.op_type == OP_CBR && p->stat_num_cycle <= 200 && \
      //     p->pipe_latch[IF_LATCH][ii].is_mispred_cbr)
      // {
      //     printf("b");
      //     printf(" %u", ii);
      // }
    }
  }
  
}

//--------------------------------------------------------------------//

void pipe_check_bpred(Pipeline *p, Pipeline_Latch *fetch_op){
  // call branch predictor here, if mispred then mark in fetch_op
  // update the predictor instantly
  // stall fetch using the flag p->fetch_cbr_stall
  bool predictResult = false;
  predictResult = p->b_pred->GetPrediction(fetch_op->tr_entry.inst_addr); // call predictor
  p->b_pred->UpdatePredictor(fetch_op->tr_entry.inst_addr, fetch_op->tr_entry.br_dir, predictResult);
  if(predictResult != fetch_op->tr_entry.br_dir) // if mispredict
  {
    p->b_pred->stat_num_mispred++;
    p->fetch_cbr_stall = true;       // stall the fetch
    fetch_op->is_mispred_cbr = true; // label the position of mispred branch
  }
  p->b_pred->stat_num_branches++; // increment the number of branches
}

//--------------------------------------------------------------------//

