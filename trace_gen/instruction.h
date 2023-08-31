#ifndef __INSTRUCTION_H
#define __INSTRUCTION_H

#include <iostream>
#include <cassert>
#include <vector>
#include <fstream>

#include "common.h"
#include "config.h"

#define NUM_INST_BUFFER 256*1024/8 // 64bit instruction 

class Instruction {
  public:   
    Instruction(const Config *config_p_);
    Instruction(uint64_t start_addr, uint64_t exec_addr,
            const Config *config_p_);

    ~Instruction();  
       
    void init(uint64_t addr_offset, uint64_t exec_addr_, int total_instruction); 
    void write_instruction(
            unsigned opcode,
            bool local_bit,
            unsigned output_idx,
            Address *ad,
            std::ofstream &trc,
            int &time); 

  private:
    std::vector<int> field_size; 
    std::vector<std::vector<int> > bit_width;  
    std::vector<std::vector<int> > start;

    uint64_t index_to_address(int idx, int channel_id); 
   
    uint64_t start_pnm_addr;  
    uint64_t exec_addr; 
    int num_instruction;
    int total_instruction; 
    int interval;  
    
    const Config *config_p;
}; 

# endif
