#include <stdlib.h>
#include "instruction.h"

Instruction::Instruction(const Config *config_p_)
{
    config_p = config_p_;

    std::vector<int> bit_width_tmp; 
    std::vector<int> start_tmp;

    int ch_addr_bits = 
        config_p->addr_bits - config_p->shift_bits - config_p->ch_bits;

    ////////////////// opcode = 0 /////////////////////
    int bit_width_0[5] = {2, 1, 12, 1, ch_addr_bits}; 
    int start_0[5]    = {62, 61, 49, 48, 0};

    field_size.push_back(sizeof(bit_width_0) / sizeof(int));
  
    for(int i = 0; i < field_size[0]; i++) {
        bit_width_tmp.push_back(bit_width_0[i]); 
        start_tmp.push_back(start_0[i]); 
    }

    bit_width.push_back(bit_width_tmp); 
    start.push_back(start_tmp); 

    bit_width_tmp.clear(); 
    start_tmp.clear();

    num_instruction = 0;
    interval = config_p->default_interval;  
}

Instruction::Instruction(uint64_t addr_offset, uint64_t exec_addr_,
        const Config *config_p_)
 : Instruction(config_p_)
{

    start_pnm_addr = addr_offset; 
    exec_addr = exec_addr_;
    num_instruction = 0; 
}


Instruction::~Instruction()
{
    bit_width.clear(); 
    start.clear();
} 

void Instruction::init(
        uint64_t addr_offset,
        uint64_t exec_addr_,
        int total_instruction_)
{
    start_pnm_addr = addr_offset;
    exec_addr = exec_addr_;    
    total_instruction = total_instruction_;
    num_instruction = 0; 
}

uint64_t Instruction::index_to_address(int idx, int channel_id)
{ 
    assert(idx < NUM_INST_BUFFER); // = 256KB
 
    uint64_t pnm_addr = start_pnm_addr + idx * 8;

    return Address(channel_id, true, pnm_addr, config_p).GetHexAddress();
}

void Instruction::write_instruction(
        unsigned opcode,
        bool local_bit,
        unsigned output_idx,
        Address *ad,
        std::ofstream &trc,
        int &time)
{
    assert(opcode == 0);

    bool trace_end = num_instruction == (total_instruction - 1) ? true : false;
 
    uint32_t inst_vec[5] = {
        opcode,
        (uint32_t)local_bit,
        output_idx,
        (uint32_t)trace_end,
        (uint32_t)ad->channel_addr};

    uint64_t inst_tmp = 0;
    for (int i = 0; i < field_size[opcode]; i++) {
        assert(inst_vec[i] < ((uint32_t)1 << bit_width[opcode][i]));  

        inst_tmp += ((uint64_t) inst_vec[i] << start[opcode][i]);
    }

    if ((num_instruction % 8) == 0) {
        if(num_instruction != 0)
            trc << std::endl; 

        trc
            << "0x"
            << std::hex << index_to_address(num_instruction, ad->channel)
            << " WRITE "
            << std::dec << time ; 

        time += interval;
    }

    trc << " 0x" << std::hex << inst_tmp; 

    num_instruction++; 

    if (trace_end) {
        trc << std::endl;

        trc
            << "0x"
            << std::hex << exec_addr
            << " SFENCE "
            << std::dec << time << std::endl;

        time += interval; 
        trc
            << "0x"
            << std::hex << exec_addr
            << " WRITE "
            << std::dec << time << " 0xcafe" << std::endl;

        time += interval;  
    }
}
