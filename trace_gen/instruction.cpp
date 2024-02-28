#include <stdlib.h>
#include "instruction.h"

Instruction::Instruction(const Config *config_p_)
{
    config_p = config_p_;

    std::vector<int> bit_width_tmp; 
    std::vector<int> start_tmp;

    int ch_addr_bits = 
        config_p->addr_bits - config_p->shift_bits - config_p->ch_bits;
    printf("ch addr bits : %u, addr bits: %u, shift bits %u, ch bits: %u\n",
    ch_addr_bits, config_p->addr_bits, config_p->shift_bits, config_p->ch_bits);
    ////////////////// opcode = 0 /////////////////////
    int bit_width_0[5] = {2, 1, 12, 1, ch_addr_bits}; 
    int start_0[5]    = {62, 61, 49, 48, 0};
    field_size.push_back(sizeof(bit_width_0) / sizeof(int));
    printf("field_size[0] content: %u\n", field_size.back());
  
    for(int i = 0; i < field_size[0]; i++) {
        bit_width_tmp.push_back(bit_width_0[i]); 
        start_tmp.push_back(start_0[i]); 

    //    printf("bit_width push back: %u, start_temp push back: %u\n",
    //    bit_width_0[i], start_0[i]);
    }

    bit_width.push_back(bit_width_tmp); 
    start.push_back(start_tmp); 

    bit_width_tmp.clear(); 
    start_tmp.clear();


    ////////////////// opcode = 1 /////////////////////
    int bit_width_1[5]  = {2, 1, 12, 1, ch_addr_bits};
    int start_1[5]      = {62, 61, 49, 48, 0};
    field_size.push_back(sizeof(bit_width_1) / sizeof(int));
    printf("field_size[1] content: %u\n", field_size.back()); 

    for(int i = 0; i < field_size[1]; i++){
        bit_width_tmp.push_back(bit_width_1[i]);
        start_tmp.push_back(start_1[i]);
    }

    bit_width.push_back(bit_width_tmp);
    start.push_back(start_tmp);

    bit_width_tmp.clear(); 
    start_tmp.clear();

    ////////////////// opcode = 2 /////////////////////
    int bit_width_2[5]  = {2, 1, 12, 1, ch_addr_bits};
    int start_2[5]      = {62, 61, 49, 48, 0};
    field_size.push_back(sizeof(bit_width_2) / sizeof(int));
    printf("field_size[2] content: %u\n", field_size.back()); 

    for(int i = 0; i < field_size[2]; i++){
        bit_width_tmp.push_back(bit_width_2[i]);
        start_tmp.push_back(start_2[i]);
    }

    bit_width.push_back(bit_width_tmp);
    start.push_back(start_tmp);

    bit_width_tmp.clear(); 
    start_tmp.clear();

    //printf("printing bit_width[0]\n");
    //for(auto& element : bit_width[0]){
    //    printf("%u ", element);
    //}
    //printf("\n");

    //printf("printing start[0]\n");
    //for(auto& element : start[0]){
    //    printf("%u ", element);
    //}
    //printf("\n");

    

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
    printf("start_pnm_addr: %lx\n", addr_offset);
    printf("exec_addr: %lx\n", exec_addr);
    printf("total_instructions: %u\n", total_instruction);
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
    assert(opcode == 0 || opcode == 1 || opcode == 2);

    bool trace_end = num_instruction == (total_instruction - 1) ? true : false;
 
    uint32_t inst_vec[5] = {
        opcode,
        (uint32_t)local_bit,
        output_idx,
        (uint32_t)trace_end,
        (uint32_t)ad->channel_addr};

    uint64_t inst_tmp = 0;
    //in default, i = 0; i < 5; i++)
    for (int i = 0; i < field_size[opcode]; i++) {
        //printf("\ni: %u\n", i);
        //if(i==0) printf("opcode: %u\n" , inst_vec[i]);
        //if(i==1) printf("local_bit: %u\n" , inst_vec[i]);
        //if(i==2) printf("output_idx: %u.\n" , inst_vec[i]);
        //if(i==3) printf("trace_end: %u\n" , inst_vec[i]);
        //if(i==4) printf("channel_addr: %u\n" , inst_vec[i]);
        assert(inst_vec[i] < ((uint32_t)1 << bit_width[opcode][i]));  
        //printf("inst_vec[i]: %u, bit_width[0][i]: %u\n",
        //inst_vec[i], bit_width[0][i]);
        inst_tmp += ((uint64_t) inst_vec[i] << start[opcode][i]);
        //printf("inst_tmp: %lx\n", inst_tmp);
        //printf("start[opcode][i]: %u\n", start[opcode][i]);
    }

    if ((num_instruction % 8) == 0) {
        if(num_instruction != 0)
            trc << std::endl; 
        //printf("channel: %u\n", ad->channel);
        //trc << "channel " << ad->channel << "\t" << "num inst "<< num_instruction<<"\t";
        trc
            << "0x"
            << std::hex << index_to_address(num_instruction, ad->channel)
            << " WRITE "
            << std::dec << time ; 

        time += interval;
    }

    trc << " 0x" << std::hex << inst_tmp; 
    //printf("0x%lx\n\n", inst_tmp);

    num_instruction++; 

    if (trace_end) {
        printf("trace end writing 0xcafe\n");
        printf("num inst: %u, exec_addr: %lu\n", num_instruction, exec_addr);
        trc << std::endl;
        //trc << "num inst " << num_instruction << "\t";
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
