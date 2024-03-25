#include "pnm.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <iostream>
#include <map>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>


namespace dramsim3 {

Instruction::Instruction(uint64_t inst_data, int channel_id,
                         const Config *config_p_) {
    config_p = config_p_;
    //printf("inst_hex: %16lx\n", inst_data);
    //printf("opcode (hex): %x\n", static_cast<int>((inst_data>>62) & 0x3));

    opcode         = static_cast<Opcode>((inst_data >> 62) & 0x3);//compare 
    if (opcode == Opcode::SUM) {//Opcode::SUM = 0
        cache_hit  = static_cast<bool>((inst_data >> 61) & 0x1);
        psum_idx   = static_cast<int>((inst_data >> 49) & 0xfff);
        trace_end  = static_cast<bool>((inst_data >> 48) & 0x1);
        type       = static_cast<DataType>((inst_data >> 47) & 0x1);
        input_idx  = static_cast<int>((inst_data >> 43) & 0xf);

        int ch_addr_bits = config_p->addr_bits
                           - config_p->shift_bits
                           - config_p->ch_bits;
        uint64_t ch_addr_mask = (((uint64_t)1 << ch_addr_bits) - 1);
        addr       = Address(channel_id,
                     static_cast<uint64_t>
                     ((inst_data & ch_addr_mask) << config_p->shift_bits),
                     config_p);

    }
    else if(opcode == Opcode::DENSE){
        cache_hit       = static_cast<bool>((inst_data >> 61) & 0x1);
        //change psum_idx to densemm_idx
        densemm_idx     = static_cast<int>((inst_data >> 49) & 0xfff);
        trace_end       = static_cast<bool>((inst_data >> 48) & 0x1);
        type            = static_cast<DataType>((inst_data >> 47) & 0x1);
        input_idx       = static_cast<int>((inst_data >> 43) & 0xf);

        int ch_addr_bits = config_p->addr_bits
                           - config_p->shift_bits
                           - config_p->ch_bits;
        uint64_t ch_addr_mask = (((uint64_t)1 << ch_addr_bits) - 1);
        addr       = Address(channel_id,
                     static_cast<uint64_t>
                     ((inst_data & ch_addr_mask) << config_p->shift_bits),
                     config_p);
    } 
    else if(opcode == Opcode::SPARSE){
        cache_hit       = static_cast<bool>((inst_data >> 61) & 0x1);
        //change psum_idx to sparsemm_idx
        sparsemm_idx    = static_cast<int>((inst_data >> 49) & 0xfff);
        trace_end       = static_cast<bool>((inst_data >> 48) & 0x1);
        type            = static_cast<DataType>((inst_data >> 47) & 0x1);
        input_idx       = static_cast<int>((inst_data >> 43) & 0xf);

        int ch_addr_bits = config_p->addr_bits
                           - config_p->shift_bits
                           - config_p->ch_bits;
        uint64_t ch_addr_mask = (((uint64_t)1 << ch_addr_bits) - 1);
        addr       = Address(channel_id,
                     static_cast<uint64_t>
                     ((inst_data & ch_addr_mask) << config_p->shift_bits),
                     config_p);
    }
    else if(opcode == Opcode::ACTIVATION){
        cache_hit       = static_cast<bool>((inst_data >> 61) & 0x1);
        //sparsemm_idx instead
        sparsemm_idx    = static_cast<int>((inst_data >> 49) & 0xfff);
        trace_end       = static_cast<bool>((inst_data >> 48) & 0x1);
        type            = static_cast<DataType>((inst_data >> 47) & 0x1);
        input_idx       = static_cast<int>((inst_data >> 43) & 0xf);

        int ch_addr_bits = config_p->addr_bits
                           - config_p->shift_bits
                           - config_p->ch_bits;
        uint64_t ch_addr_mask = (((uint64_t)1 << ch_addr_bits) - 1);
        addr       = Address(channel_id,
                     static_cast<uint64_t>
                     ((inst_data & ch_addr_mask) << config_p->shift_bits),
                     config_p);
    }
    else {
        std::cerr << "unknown opcode" << (int)opcode << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
    hex_addr = addr.GetHexAddress();
}

PNM::PNM(int channel, const Config &config, const Timing &timing,
         Controller& ctrl)
    : channel_id_(channel),
      clk_(0), hardware_clk_(0),
      config_(config),
      sls_exec(false), program_count(-1), 
      num_write_inst(0), num_read_inst(-1), 
      num_accum(-1), add_to_buf_cnt(-1),
      start_clk(-1), hardware_busy_clk_cnt(-1), end_clk(-1),
      sys_array_start_clk(-1), sys_array_end_clk(-1),
      cycles_stalled(0),
      sys_array_busy(0),
      num_densemm(-1), num_sparsemm(-1),
      inst_offset_idx(0), psum_offset_idx(0), densemm_offset_idx(0), sparsemm_offset_idx(0),
      ctrl_(ctrl)
{
    printf("\ninitializing PNM module, channel: %u\n", channel);
    srand(42);
    printf("PNM INST BUF START: 0x%x\n", PNM_INST_BUF_START);
    printf("PNM INST BUF END: 0x%x\n", PNM_INST_BUF_END);
    printf("PNM CONFIG REG START: 0x%x\n", PNM_CONFIG_REG_START);
    printf("PNM CONFIG REG END: 0x%x\n", PNM_CONFIG_REG_END);
    printf("PNM PSUM BUF START: 0x%x\n", PNM_PSUM_BUF_START);
    printf("PNM PSUM BUF END: 0x%x\n", PNM_PSUM_BUF_END);
    printf("PNM DENSEMM BUF START: 0x%x\n", PNM_DENSEMM_BUF_START);
    printf("PNM DENSEMM BUF END: 0x%x\n", PNM_DENSEMM_BUF_END);
    printf("PNM SPARSEMM BUF START: 0x%x\n", PNM_SPARSEMM_BUF_START);
    printf("PNM SPARSEMM BUF END: 0x%x\n", PNM_SPARSEMM_BUF_END);
    printf("PNM clk ratio: %u\n", config_.clk_ratio);

    //initialize blocksparse kernel status vector
    num_blocksp_kernels = config_.num_blocksp_kernels;
    //num_spgemm_kernels = config_.num_spgemm_kernels;
    selected_kernel_buffer = 0;
    for(int i = 0; i < num_blocksp_kernels; i++){
        blocksp_kernel_status.push_back(0);
        sparse_kern_end_clk.push_back(INT_MAX);
        sparse_kern_busy_cnt.push_back(0);
    }

    //maximum number of instructions
    //              (256*1024)/(8) = 32768
    auto num_inst = config_.instruction_buffer_size / config_.instruction_size;
    printf("num_inst: %u, inst_buffer_size: %u, inst_size: %u\n", num_inst, config_.instruction_buffer_size, config_.instruction_size);
    //insert dummy instructions
    //0x0, 0x8, 0x10, 0x18, 0x20, 0x28, ... (32768 times total)
    for (int i = 0; i < num_inst; i++) {
       inst_buf.insert(std::pair<uint64_t, Instruction>(
                                 index_to_address(i, PNMRegister::INSTRUCTION),
                                           Instruction(&config_)));
    }

    uint64_t inst_buffer_offset =
        index_to_address(num_inst / 2, PNMRegister::INSTRUCTION)
        - index_to_address(0, PNMRegister::INSTRUCTION);

    write_instruction_offset.push_back(0);

    if (config_.instruction_double == true) {
        write_instruction_offset.push_back(inst_buffer_offset);
        pc_offset.push_back(num_inst / 2);
    }
    pc_offset.push_back(0);

    //total number of psums that can fit in device
    //maximum psum index
    //             (256*1024) / 16 / 4 = 4096
    auto num_psum = config_.psum_buffer_size
                    / config_.sparse_feature_size
                    / config_.data_size;
    printf("num_psum: %u, psum_buffer_size: %u, sparse_feature_size: %u, data_size: %u\n",
    num_psum, config_.psum_buffer_size, config_.sparse_feature_size, config_.data_size);
    //insert dummy psums into psum 
    //PNM_PSUM_BUF_START + 0x00
    //PNM_PSUM_BUF_START + 0x40
    //PNM_PSUM_BUF_START + 0x80
    //PNM_PSUM_BUF_START + 0xc0
    //4096 times...
    for (int i = 0; i < num_psum; i++) {
        float *t_data = new float[config_.sparse_feature_size];
        for (int j = 0; j < config_.sparse_feature_size; j++) {
            t_data[j] = 0;
        }
        psum.insert(std::pair<uint64_t, float *>(
                              index_to_address(i, PNMRegister::PSUM),
                                        t_data));
    }

    uint64_t psum_buffer_offset =   
        index_to_address(num_psum / 2,PNMRegister::PSUM) 
        - index_to_address(0, PNMRegister::PSUM);

    read_psum_offset.push_back(0);

    if (config_.psum_double == true) {
        read_psum_offset.push_back(psum_buffer_offset);//analogous to write_instruction_offset
        psum_offset.push_back(num_psum / 2);//analogous to pc_offset
    }
    psum_offset.push_back(0);

    //insert dummy densemms into densemm_buf
    auto num_densemm = config_.densemm_buffer_size
                        / config_.densemm_feature_size
                        / config_.data_size;
    printf("num_densemm: %u, densemm_buffer_size: %u, densemm_feature_size: %u, data_size: %u\n",
    num_densemm, config_.densemm_buffer_size, config_.densemm_feature_size, config_.data_size);
    
    for(int i = 0; i < num_densemm; i++){
        float* t_data = new float[config_.densemm_feature_size];
        for(int j = 0; j < config_.densemm_feature_size; j++){
            t_data[j] = 1;
        }
        densemm_buf.insert(
            std::pair<uint64_t, float*>(
                index_to_address(i, PNMRegister::DENSEMM), t_data
            )
        );
    }
    
    uint64_t densemm_buffer_offset = 
        index_to_address(num_densemm/2, PNMRegister::DENSEMM)
        - index_to_address(0, PNMRegister::DENSEMM);

    //if double,    [0, densemm_buffer_offset]
    //if not,       [0]
    read_densemm_offset.push_back(0); 

    if(config_.densemm_double == true){
        read_densemm_offset.push_back(densemm_buffer_offset);
        densemm_offset.push_back(num_densemm / 2);
    }

    //if double     [num_densemm/2, 0]
    //if not        [0]
    densemm_offset.push_back(0);


    //insert dummy sparsemms into sparsemm_buf
    auto num_sparsemm = config_.sparsemm_buffer_size
                        / config_.sparsemm_feature_size
                        / config_.data_size;

    printf("num_sparsemm: %u, sparsemm_buffer_size: %u, sparsemm_feature_size: %u, data_size: %u\n",
    num_sparsemm, config_.sparsemm_buffer_size, config_.sparsemm_feature_size, config_.data_size);
    
    for(int i = 0; i < num_sparsemm; i++){
        float* t_data = new float[config_.sparsemm_feature_size];
        for (int j = 0; j < config_.sparsemm_feature_size; j++){
            t_data[j] = 0;
        }
        sparsemm_buf.insert(
            std::pair<uint64_t, float*>(
                index_to_address(i, PNMRegister::SPARSEMM), t_data
            )
        );
    }

    uint64_t sparsemm_buffer_offset = 
        index_to_address(num_sparsemm/2, PNMRegister::SPARSEMM)
        - index_to_address(0, PNMRegister::SPARSEMM);
    
    read_sparsemm_offset.push_back(0);

    if(config_.sparsemm_double == true){
        read_sparsemm_offset.push_back(sparsemm_buffer_offset);
        sparsemm_offset.push_back(num_sparsemm/2);
    }

    sparsemm_offset.push_back(0);

    //instantiate sys array input buffers
    // = 256 * 128
    auto num_sys_arr_input = ACT_INPUT_BUF_BYTE_SIZE
                            / config_.data_size;

    sys_arr_input_act_buf = new float[num_sys_arr_input];
    sys_arr_input_wgt_buf = new float[num_sys_arr_input];
    for(int i = 0; i < num_sys_arr_input; i++){
        //sys_arr_input_act_buf.push_back(0.0);
        //sys_arr_input_wgt_buf.push_back(0.0);
        sys_arr_input_act_buf[i] = 0.0;
        sys_arr_input_wgt_buf[i] = 0.0;
    }

    //number of numbers that go into the sys array buffers for sparsemm
    num_block_sp_input = config_.sparsemm_blk_size * config_.sparsemm_blk_size;
    
    block_sp_input_act_buf = new float[num_block_sp_input];
    block_sp_input_wgt_buf = new float[num_block_sp_input];
    for(int i = 0; i < num_block_sp_input; i++){
        block_sp_input_act_buf[i] = 0.0;
        block_sp_input_wgt_buf[i] = 0.0;
    }

    activation_sparse = config_.activation_sparse;

    //if activations are sparse, weight is dense
    if(activation_sparse){
        printf("activation_sparse TRUE\n");
        //number instructions required to make one sparse_ element
        req_num_inst_act_buf = 1638/2;
        req_num_inst_wgt_buf = 4096;
        kern_num_sp_elements = 1638;
    }
    //if actiations dense weight sparse
    //up to 8/256 = 3.125% density is the same complexity
    else{
        req_num_inst_act_buf = 4096;
        req_num_inst_wgt_buf = 82;
        kern_num_sp_elements = 82;
    }

    spgemm_input_act_buf = new float[req_num_inst_act_buf];
    spgemm_input_wgt_buf = new float[req_num_inst_wgt_buf];
    spgemm_input_act_idx = 0;
    spgemm_input_wgt_idx = 0;

    //spgemm_input_act_buf

    //for(int i = 0; i < num_sys_arr_input; i++){
    //    float* act_data = new float[config_.densemm_feature_size];
    //    float* wgt_data = new float[config_.densemm_feature_size];
    //    for(int j = 0; j < config_.densemm_feature_size; j++){
    //        act_data[j] = 0;
    //        wgt_data[j] = 0;
    //    }
    //
    //    sys_arr_input_act_buf.push_back(act_data);
    //    sys_arr_input_wgt_buf.push_back(wgt_data);
    //}

    sys_arr_input_act_idx = 0;
    sys_arr_input_wgt_idx = 0;
}

bool PNM::Done() {
    if ((num_write_inst == 0) &&
        (program_count == -1) &&
        (num_accum == -1)     &&
        (num_densemm == -1)   &&
        (num_sparsemm == -1)) 
        return true;

    return false;
}

void PNM::ClockTick() {
    //set to stop when 0xcafe 
    bool data_added_to_buf = false;
    if(0){
    //if(clk_ > 1000000){
        std::cerr << "FORCE STOP" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
    if (!sls_exec) {
        if(clk_ % config_.clk_ratio == 0){
            hardware_clk_++;
        }
        clk_++;
        // Nothing 
        return;
    }

    //printf("")
    //printf("\npnm::clocktick()\n");
    //printf("\nchannel: %u\n", channel_id_);
    printf("clock: %lu, hardware clk: %lu\n", clk_, hardware_clk_);
    // 1. Adder -> PSUM
    if(clk_ % config_.clk_ratio == 0){
        if (!adder_.empty()) {
            //printf("clock: %lu, executeadder\n", clk_);
            ExecuteAdder();
        }

        if(!dense_.empty()){
            //printf("clocktick: %lu, execute densematmul\n", hardware_clk_);
            ExecuteDenseMatmul();
        }

        if(!sparse_.empty()){
            //printf("clock: %lu, execute sparsematmul\n", clk_);
            ExecuteSparseMatmul();
        }

        hardware_clk_++;
    }

    // 2. Data in return_queue_ -> SLS op
    //if (!return_queue_.empty()) {
    if(!return_queue_.empty() &&
        return_queue_.begin()->complete_cycle <= clk_){
        //printf("return queue instruction valid\n");
        add_to_buf_cnt++;
        data_added_to_buf = ReturnDataReady();
    }

    // 3-1. Cache check
    //else if (!cache_q_.empty()) {
    //if (!cache_q_.empty()) {
    if(!cache_q_.empty() && data_added_to_buf == false){
        ReadCache();
    }

    // 4. Instruction fetch -> Send to controller
    //if(return_queue_.size() < 4096 && program_count != -1){
    if(program_count != -1){
    //if(return_queue_.size() < 500){
        //printf("schedule instruction\n");
        ScheduleInstruction();
    }

    clk_++;
    return;
}

void PNM::Write(Transaction trans, uint64_t pnm_addr) {
    if(1) printf("\nPNM write to 0x%lx, channel : %u\n", pnm_addr, channel_id_);
    // instruction
    if ((pnm_addr >= PNM_INST_BUF_START)
         && (pnm_addr <= PNM_INST_BUF_END)) {
        //if(1) printf("instruction -- pnm_addr: 0x%lx\n", pnm_addr);
        WriteInstruction(trans);
        
    }
    // control register
    else if ((pnm_addr >= PNM_CONFIG_REG_START)
              && (pnm_addr <= PNM_CONFIG_REG_END)) {
        printf("control register -- pnm_addr: 0x%lx\n", pnm_addr);
        WriteConfigRegister(trans);
    }
    else {
        std::cerr << " Write: unknown pnm address " << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
    return;
}

void PNM::WriteInstruction(Transaction trans) {
    
    //if(1) printf("writing instruction in channel %u\n", channel_id_);
    
    uint64_t *t_data = (uint64_t*) trans.data;
    for (int i = 0; i < trans.data_size; i++) {
        auto it = inst_buf.find(write_instruction_offset[inst_offset_idx]
                                + trans.addr + i*8);

        if (it == inst_buf.end()) {
            std::cerr << "instruction address not matching " 
                      << channel_id_ << "  " 
                      << std::hex << trans.addr + i*8  << std::dec
                      << std::endl;
            AbruptExit(__FILE__, __LINE__);
        }

        it->second = Instruction(t_data[i], channel_id_, &config_);
        num_write_inst++;
        
        //only print once to save time
        if(1){
            printf("clk: %lu, inst_addr: 0x%lx, inst_hexode: 0x%lx, hex_addr: 0x%lx, num_write_inst: %u, cache hit: %u\n", 
            clk_, it->first, t_data[i], it->second.hex_addr, num_write_inst, it->second.cache_hit);
        }
        //printf("\n");
 
    }
    delete[] t_data;

    return ;
}

void PNM::WriteConfigRegister(Transaction trans) {
    printf("writing config register\n");

    if (trans.addr == Address(channel_id_,
                              true,
                              PNM_CONFIG_REG_START + PNM_EXE_REG_OFFSET,
                              &config_).GetHexAddress()) { // SLS EXEC
        if (trans.data_size == 0)
            AbruptExit(__FILE__, __LINE__);


        uint64_t* t_data = (uint64_t*) trans.data;
        sls_exec = t_data[0] == 0xcafe ? true : false;
        
        if (sls_exec) {
            printf("sls_exec is TRUE\n");
            program_count = 0;
            num_read_inst = 0;

            num_accum = 0;
            num_densemm = 0;
            num_sparsemm = 0;

            inst_offset_idx = (inst_offset_idx + 1)
                              % write_instruction_offset.size();
            psum_offset_idx = (psum_offset_idx + 1)
                              % psum_offset.size();
            densemm_offset_idx = (densemm_offset_idx + 1)
                                % densemm_offset.size();
            sparsemm_offset_idx = (sparsemm_offset_idx + 1)
                                % sparsemm_offset.size();
            start_clk = hardware_clk_;


            std::cout << clk_ << "::CH"
                      << channel_id_ << "::SLS_EXEC_ENABLED"
                      << std::endl;

        }
    }
    else {
         std::cerr << " unknown config register write " << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    return;
}


float *PNM::Read(uint64_t hex_addr, uint64_t pnm_addr) {

    printf("[READ] -- pnm_addr: 0x%lx\n", pnm_addr);
    // psum
    if ((pnm_addr >= PNM_PSUM_BUF_START)
         && (pnm_addr <= PNM_PSUM_BUF_END)) {
        return ReadPsumData(hex_addr);
    }

    //densemm
    if ((pnm_addr >= PNM_DENSEMM_BUF_START)
         && (pnm_addr <= PNM_DENSEMM_BUF_END)) {
        return ReadDensemmData(hex_addr);
    }

    //sparsemm
    if ((pnm_addr >= PNM_SPARSEMM_BUF_START)
         && (pnm_addr <= PNM_SPARSEMM_BUF_END)) {
        return ReadSparsemmData(hex_addr);
    }

    // control register
    else if ((pnm_addr >= PNM_CONFIG_REG_START)
              && (pnm_addr <= PNM_CONFIG_REG_END)) {
        return ReadConfigRegister(hex_addr);
    }
    else {
        std::cerr << " READ: unknown pnm address " 
                  << std::hex << hex_addr << std::dec 
                  << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    return NULL;
}

float* PNM::ReadConfigRegister(uint64_t hex_addr) {

    float *t_data = new float[config_.sparse_feature_size];

    if (hex_addr == Address(channel_id_,
                            true,
                            PNM_CONFIG_REG_START + PNM_STATUS_REG_OFFSET,
                            &config_
                           ).GetHexAddress()) {

        for (int i = 0; i < config_.sparse_feature_size; i++) {
            t_data[i] = 0;
        }

        t_data[0] = (float)program_count;
        t_data[1] = this->Done() ? 1111 : 0 ;

    }
    else {
        std::cerr << " unknown config register write "
                  << std::hex << hex_addr << std::dec
                  << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    return t_data;
}

//read psum
float* PNM::ReadPsumData(uint64_t hex_addr) {
    printf("[READING PSUM]\n");
    auto it = psum.find(read_psum_offset[psum_offset_idx] + hex_addr);
    if (it == psum.end()) {
        std::cout << "psum buffer idx: "
                  << psum_offset_idx << "  "
                  << std::hex << hex_addr << std::dec << "  "
                  << read_psum_offset[psum_offset_idx]
                  << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    //it->second holds same value repeated for sparse_feature_size
    //printf("psum value\n");
    //for(int i=0; i<config_.sparse_feature_size; i++){
    //    printf("%f ", it->second[i]);
    //}
    printf("hex_addr: 0x%lx, psum value: %f\n\n", hex_addr, it->second[0]);
    return it->second;
}

//read densemm
float* PNM::ReadDensemmData(uint64_t hex_addr) {
    printf("[READING DENSEMM]\n");
    auto it = densemm_buf.find(read_densemm_offset[densemm_offset_idx] + hex_addr);
    if (it == densemm_buf.end()) {
        std::cout << "densemm buffer idx: "
                  << densemm_offset_idx << "  "
                  << std::hex << hex_addr << std::dec << "  "
                  << read_densemm_offset[densemm_offset_idx]
                  << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
    printf("hex_addr: 0x%lx, densemm value: [", hex_addr);
    for(int i=0; i <config_.densemm_feature_size; i++){
        printf("%f ", it->second[i]);
    }
    printf("]\n");

    return it->second;
}

//read sparsemm
float* PNM::ReadSparsemmData(uint64_t hex_addr) {

    auto it = sparsemm_buf.find(read_sparsemm_offset[sparsemm_offset_idx] + hex_addr);
    if (it == sparsemm_buf.end()) {
        std::cout << "sparse buffer idx: "
                  << sparsemm_offset_idx << "  "
                  << std::hex << hex_addr << std::dec << "  "
                  << read_sparsemm_offset[sparsemm_offset_idx]
                  << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    return it->second;
}

uint64_t PNM::index_to_address(int idx, PNMRegister reg_mode) {

    uint64_t idx_addr;
    switch (reg_mode) {
        case PNMRegister::INSTRUCTION :
            idx_addr = PNM_INST_BUF_START;
            // = instruction_size = 8Byte  vs. idx = 64 Byte
            idx_addr += idx * config_.instruction_size;
            break;
        case PNMRegister::PSUM :
            idx_addr = PNM_PSUM_BUF_START;
            // size of a psum in sparse feature size x 4 (=fp32)
            idx_addr += idx * (config_.sparse_feature_size * config_.data_size);
            break;
        case PNMRegister::DENSEMM :
            idx_addr = PNM_DENSEMM_BUF_START;
            idx_addr += idx * (config_.densemm_feature_size * config_.data_size);
            break;
        case PNMRegister::SPARSEMM :
            idx_addr = PNM_SPARSEMM_BUF_START;
            idx_addr += idx * (config_.sparsemm_feature_size * config_.data_size);
            break;
        default:
            idx_addr = -1;
            std::cerr << "unknown reg_mode" << std::endl;
            AbruptExit(__FILE__, __LINE__);
            break;
    }

    return Address(channel_id_, true, idx_addr, &config_).GetHexAddress();
}


void PNM::ScheduleInstruction() {
    //printf("scheduling instruction\n");
    if (program_count == -1)
        return;
    
    auto it = inst_buf.find(index_to_address(pc_offset[inst_offset_idx]
                                             + program_count,
                                             PNMRegister::INSTRUCTION));
    if (it == inst_buf.end()) {
        std::cerr<< "instruction fetching error " << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    //check if data from instruction is held in cache
    auto cache_it = data_cache_.find(it->second.hex_addr);

    //if not cache hit (is predetermined with some probability)
    //if (it->second.cache_hit == false) {
   
    //if(it->second.cache_hit == 0){
    //    printf("[SCHEDULE INST] activation instruction schedule\n");
    //}
    //else{
    //    printf("[SCHEDULE INST] weight instruction schedule\n");
    //}
    printf("[SCHEDULE INST] program count: %u\n", program_count);
    bool isActivation = (it->second.cache_hit == 0);
    
    //if cache miss, and not activation
    if(cache_it == data_cache_.end() && !isActivation){
        printf("[SCHEDULE INST - CACHE MISS]\n");
        //printf("cachpe miss\n");
        Transaction trans = InstructionToTransaction(it->second);
        //if first time this instruction is being requested
        if (requested_rd_q_.count(it->second.hex_addr) == 0) {
            //if memory controller willing to accept request
            if (ctrl_.WillAcceptTransaction(trans.addr, trans.is_write)) {
                //add transaction request
                ctrl_.AddTransaction(trans);
                //printf("[SCHEUDLE INST] first time request, requested q size: %lu\n",
                //requested_rd_q_.size());
                //printf("instruction addr: 0x%lx\n", trans.addr);
            }
            //if memory controller not willing to accept request
            else {
                //can't do anything, just return
                //wait for next clock tick to do anything
                //printf("controller busy :(\n");
                return;
            }
        }
        //if not first time requesting data OR successfully requested to memory controller
        //add <hex_addr, Instruction> to requested_rd_queue
        requested_rd_q_.insert(std::make_pair(it->second.hex_addr, it->second));
        //printf("[SCHEDULE INST] add to requested_rd_q\n");
        printf("[SCHEDULE INST] ");
        if(it->second.opcode == Opcode::SUM){
            printf("opcode: SUM, psum_idx: 0x%x ", it->second.psum_idx);
        }
        else if(it->second.opcode == Opcode::DENSE){
            printf("opcode : DENSE, densemm_idx: 0x%x ", it->second.densemm_idx);
        }
        else if(it->second.opcode == Opcode::SPARSE){
            printf("opcode : SPARSE, sparsemm_idx: 0x%x ", it->second.sparsemm_idx);
        }
        else if(it->second.opcode == Opcode::ACTIVATION){
            printf("opcode : ACTIVATION, sparsemm_idx: 0x%x ", it->second.sparsemm_idx);
        }
        printf("hex_addr: 0x%lx\n", it->second.hex_addr);


        //set complete cycle to inf, add to return queue
        //printf("[SCHEDULE INST] push back to return queue\n");
        it->second.complete_cycle = INT_MAX;
        it->second.data = new float[config_.densemm_feature_size];
        return_queue_.push_back(it->second);
        //printf("[SCHEDULE INST] hexaddr: 0x%lx, complete cycle: %lu", 
        //return_queue_.back().hex_addr, return_queue_.back().complete_cycle);
    }
    //if cache hit or is activation
    else {
        //printf("cache hit\n");
        //if cache queue is not full (cache queue can take up to 8 instructions)
        //if (cache_q_.size() < 8) {
        if(1){
            if(isActivation){
                printf("[SCHEDULE INST - CACHE HIT] activation auto cache hit \n");
            }
            else{
                printf("[SCHEDULE INST - CACHE HIT] weight cache hit\n");
            }    
            printf("[SCHEDULE INST] addr: %lx\n", it->second.hex_addr);
            // = max cache queue size
            //add Instruction to cache_q
            printf("[SCHEDULE INST] cache q size: %lu\n", cache_q_.size());
            //printf("[SCHEDULE INST] cache q empty: %lu\n", cache_q_.empty());
            //set complete cycle to inf, add to return queue
            //printf("[SCHEDULE INST] hex addr: 0x%lx added to return queue\n", it->second.hex_addr);
            //printf("instruction added to return queue with infinite complete cycle\n");
            it->second.complete_cycle = INT_MAX;
            it->second.data = new float[config_.densemm_feature_size];
            return_queue_.push_back(it->second);
            cache_q_.push_back(it->second);

            //printf("[SCHEULDE INST] data: [");
            //for(int i = 0; i < config_.densemm_feature_size; i++){
            //    printf("%f", it->second.data[i]);
            //}
            //printf("]\n");

            //DEBUGGING
            //if(it->second.hex_addr == 0x75e140){
            //    printf("[SCHEDULE INST] print return queue for debug\n");
            //    for(auto& element: return_queue_){
            //        printf("hex_addr: 0x%lx, complete cycle: %lu\n",
            //        element.hex_addr, element.complete_cycle);
            //    }
            //}

            //update most frequent cache update time
            //if a weight cache hit, update access time
            if(cache_it != data_cache_.end()){
                auto xs_it = cache_access_time.find(it->second.hex_addr);
                xs_it->second = clk_;
            }
            //printf("[SCHEDULE INST] updating cache access time to %lu\n", clk_);
        }
        //if cache hit, but queue cache is full???
        else{
            //do nothing, and wait until cache space feed up 
            //clock tick will ReadCache on next cycle
            printf("[SCHEDULE INST - CACHE HIT] cache q full\n");
            return;
        }
    }

    //increaese program_count, so it points to next scheduled instruction 
    //when the next clocktick() is called
    program_count++;

    //if end of trace, do fancy stuff
    if (it->second.trace_end == 1){
        // program_cout == num_write_inst

        std::cout << clk_ << "::CH"
                  << channel_id_ << "::ScheduleInstruction END"
                  << std::endl;

        printf("[TRACE END] - channel: %u\n", channel_id_);
        printf("\tprogram count: %u\n", program_count);
        num_read_inst = program_count;
        program_count = -1;
        num_write_inst = 0;
    }
    it->second = Instruction(&config_);
    return;
}

Transaction PNM::InstructionToTransaction(const Instruction inst) {
    // only read (write is not supported)
    return Transaction(inst.hex_addr, false, NULL, inst.data_size);
}

void PNM::ReadCache() {
    auto it = cache_q_.begin();
    auto it_c = data_cache_.find(it->hex_addr);
    bool isActivation = (it->cache_hit == 0);

    if (it_c == data_cache_.end()){//if instruction not in data cache
        if(isActivation == false){//if instruction is not activation (weight)
            printf("[READ CACHE] cache q not empty, but weight, so no auto cache hit\n");
            return;//return back if instruction is not in data cache and instruction is a weight
            //weights must always be in the data cache for cache hit to occur.
        }
        else{
            printf("[READ CACHE] activation auto cache hit ");
            printf("hex addr: %lx\n", it->hex_addr);
        }
    }

#ifdef CMD_TRACE
    std::cout << "in cache [" << channel_id_ << "] "
              << std::left << std::setw(18)
              << clk_ << " cache hit \t\t"
              << std::hex << it->addr.channel << "   "
              << it->addr.rank << "   "
              << it->addr.bankgroup << "   "
              << it->addr.bank << "   0x"
              << it->addr.row << "   0x"
              << it->addr.column << std::dec
              << std::endl;
#endif  // CMD_TRACE
    //one cycle to read
    //printf("[READ CACHE] update instruction complete cycle and instruction data\n");
    printf("[READ CACHE] hex_addr: 0x%lx\n", it->hex_addr);
    it->complete_cycle = clk_ + 1;//should update complete cycle in return queue aswell

    //update instruction in the cache with corresponding weight data
    //it->data = new float[config_.sparse_feature_size];
    //for (int i = 0; i < config_.sparse_feature_size; i++) {
    //    it->data[i] = it_c->second[i];
    //}

    //update instruction in return queue with corresponding weight data


    //update instructions in return_queue to have complete cycle = clk_ + 1
    
    for(auto& element : return_queue_){
        if(element.hex_addr == it->hex_addr){
            element.complete_cycle = clk_ + 1;

            //just set all activations as 1
            if(isActivation){
                for(int i = 0; i < config_.densemm_feature_size; i++){
                    element.data[i] = 1.0;
                }
            }

            //get weight data from cache
            else{
                element.data = it_c->second;
            }
        }
    }

    //checking if return queue properly updated
    //printf("[READ CACHE] print return queue\n");
    //for(unsigned i = 0; i < return_queue_.size(); i++){
    //    printf("\ti: %u, hex_addr: 0x%lx, complete_cycle: %lu, data[0]: %f\n",
    //    i, return_queue_[i].hex_addr, return_queue_[i].complete_cycle, return_queue_[i].data[0]);
    //}
    //printf("[READ CACHE] return queue size: %lu\n", return_queue_.size());
    //remove instruction from cache_queue (finished processing inst)
    cache_q_.erase(it);
}

bool PNM::ReturnDataReady() {
    //take first instruction in return_queue_ that is valid
    //clk_ > complete_cycle (current cycle is after instruction valid cycle)
    
    //while the first instruction in the queue is valid
    //break if 
    //1) double buffers filled
    //2) end of queue
    //3) instruction not valid
    auto inst = return_queue_.begin();
    //auto next_inst = return_queue_.begin();
    //next_inst++;
    int num_inst_added = 0;
    printf("[RETURN DATA READY] return queue size: %lu\n", return_queue_.size());
    while(1){
        
        if(dense_.size() >= 2){
            printf("[RETURN DATA READY] dense double buffers all filled, try again later\n");
            break;
        }
        else if(sparse_.size() >= (uint64_t)num_blocksp_kernels){
            printf("[RETURN DATA READY] sparse double buffers all filled, can't receive any more data\n");
            //double buffers all filled up, can't take any more. just leave in the return queue (수신거부)
            break;
        }
    
        //printf("[RETURN DATA READY] - channel: %u, return queue size: %lu, ", 
        //channel_id_, return_queue_.size());
        if (inst == return_queue_.end()) {
            //printf("instruction doesn't exist\n");
            printf("[RETURN DATA READY] end of return queue\n");
            break; // nothing is ready
        }

        //instruction is not yet valid
        else if(inst->complete_cycle > clk_){
            //printf("[RETURN DATA READY] instruction not valid, goto next\n");
            //inst = next_inst;
            //next_inst = next_inst++;
            inst++;
            continue;
        }

        //if opcode == 0 (SLS opcode)
        if (inst->opcode == Opcode::SUM) {

            //get iterator that points to <addr, float*> in psum_buffer
            auto psum_it = psum.find(index_to_address(
                                    psum_offset[psum_offset_idx] + inst->psum_idx,
                                    PNMRegister::PSUM));

            //print new adder element
            printf("[RETURN DATA READY] new AdderElement\n");
            printf("psum_idx: %u\n", inst->psum_idx);
            printf("psum_data: [");
            for(int i = 0; i < config_.sparse_feature_size; i++){
                printf("%f ",psum_it->second[i]);
            }
            printf("]\n");
            printf("dram_data: [");
            for(int i = 0; i < config_.sparse_feature_size; i++){
                printf("%f ",inst->data[i]);
            }
            printf("]\n");

            //make adder element
            AdderElement adder = AdderElement(inst->psum_idx,
                                            psum_it->second,
                                            inst->data);

            //add to adder_ vector
            adder_.push_back(adder);
        }
        //add support for dense mm
        //else if(inst->opcode == Opcode::DUMMY){

        else if(inst->opcode == Opcode::DENSE){
            //double buffer filled, dont accept anthing
            if(dense_.size() == 2){
                printf("[RETURN DATA READY] dense_ buffer filled up, skip for now\n");
                break;
            }
            
            //current instruction is activation
            if(inst->cache_hit == 0){
                //activation buffer has space
                if(sys_arr_input_act_idx != 256 * 128){
                    if(dense_.size() == 1){
                        printf("[RETURN DATA READY] filling second dense_ buffer while sys array is running\n");
                    }
                    printf("[RETURN DATA READY] DENSE inst addr: 0x%lx\n", inst->hex_addr);
                    printf("[RETURN DATA READY] adding activation to idx: %u, weight idx: %u\n", 
                    sys_arr_input_act_idx, sys_arr_input_wgt_idx);
                    sys_arr_input_act_idx += config_.densemm_feature_size;

                    printf("[RETURN DATA READY] removing from return_queue hex_addr: 0x%lx\n", inst->hex_addr);
                    inst = return_queue_.erase(inst);
                    printf("[RETURN DATA READY] return queue size: %lu\n", return_queue_.size());

                    num_inst_added++;
                }
                //activation buffer no space, move onto next inst
                else{
                    //printf("[RETURN DATA READY] activation buffer all filled\n");
                    inst++;
                }
            }
            
            //current instruction is weight
            else if(inst->cache_hit == 1){
                //weight buffer has space
                if(sys_arr_input_wgt_idx != 256 * 128){
                    if(dense_.size() == 1){
                        printf("[RETURN DATA READY] filling second dense_ buffer while sys array is running\n");
                    }
                    printf("[RETURN DATA READY] DENSE inst addr: 0x%lx\n", inst->hex_addr);
                    printf("[RETURN DATA READY] adding weight to idx: %u, activation idx: %u\n", 
                    sys_arr_input_wgt_idx, sys_arr_input_act_idx);
                    sys_arr_input_wgt_idx += config_.densemm_feature_size;

                    printf("[RETURN DATA READY] removing from return_queue hex_addr: 0x%lx\n", inst->hex_addr);
                    inst = return_queue_.erase(inst);
                    printf("[RETURN DATA READY] return queue size: %lu\n", return_queue_.size());

                    num_inst_added++;
                }
                else{
                    //printf("[RETURN DATA READY] weight buffer all filled\n");
                    inst++;
                }
            }
            
            //if either buffer not filled, fill them
            /*
            if(sys_arr_input_act_idx != 256*128 || sys_arr_input_wgt_idx != 256*128){
                num_inst_added++;
                //if act
                if(inst->cache_hit == 0 && sys_arr_input_act_idx != 256*128){
                    printf("[RETURN DATA READY] activation added to idx: %u\n", sys_arr_input_act_idx);
                    for(int i = 0; i < config_.densemm_feature_size; i++){
                        sys_arr_input_act_buf[sys_arr_input_act_idx + i] = inst->data[i];
                    }
                    sys_arr_input_act_idx += config_.densemm_feature_size;
                }

                //if weight
                else if(inst->cache_hit == 1 && sys_arr_input_wgt_idx != 256*128){
                    printf("[RETURN DATA READY] weight added to idx: %u\n", sys_arr_input_wgt_idx);
                    for(int i = 0; i < config_.densemm_feature_size; i++){
                        sys_arr_input_wgt_buf[sys_arr_input_wgt_idx + i] = inst->data[i];
                    }
                    sys_arr_input_wgt_idx += config_.densemm_feature_size;
                }
            }
            */
            //check if buffers are filled after filling them
            //if sys arr input buffers are full, make dense object

            if(sys_arr_input_act_idx == 256*128 && sys_arr_input_wgt_idx == 256*128){
                printf("[RETURN DATA READY] both buffers filled, making denseMatmulElement and adding to dense_\n");
                printf("[RETURN DATA READY] hardware clk: %lu\n", hardware_clk_);
                DenseMatmulElement element = DenseMatmulElement(
                    inst->densemm_idx,
                    sys_arr_input_act_buf,
                    sys_arr_input_wgt_buf
                );
                dense_.push_back(element);

                sys_arr_input_act_idx = 0;
                sys_arr_input_wgt_idx = 0;
                
            }
        }
        
        //if block32, block16
        else if(inst->opcode == Opcode::SPARSE && config_.sparsemm_blk_size != 1){
            printf("[RETURN DATA READY] BLOCKSPARSE\n");
            if(sparse_.size() == (uint64_t)num_blocksp_kernels){
                printf("[RETURN DATA READY] all %u sparse_ filled, skip\n", num_blocksp_kernels);
                break;
            }
            
            //fill buffer 
            if(block_sp_input_act_idx != num_block_sp_input || block_sp_input_wgt_idx != num_block_sp_input){
                if(inst->cache_hit == 0 && block_sp_input_act_idx != num_block_sp_input){//if act
                    printf("[RETURN DATA READY] add to buffer - activation, idx: %u\n", block_sp_input_act_idx);
                    for(int i = 0; i < config_.sparsemm_feature_size; i++){
                        block_sp_input_act_buf[block_sp_input_act_idx + i] = inst->data[i];
                    }
                    block_sp_input_act_idx += config_.sparsemm_feature_size;

                    printf("[RETURN DATA READY] removing from return_queue hex_addr: 0x%lx\n", inst->hex_addr);
                    inst = return_queue_.erase(inst);
                    printf("[RETURN DATA READY] return queue size: %lu\n", return_queue_.size());
                }

                else if(inst->cache_hit == 1 && block_sp_input_wgt_idx != num_block_sp_input){//if weight
                    printf("[RETURN DATA READY] add to buffer - weight, idx: %u\n", block_sp_input_wgt_idx);
                    for(int i = 0; i < config_.sparsemm_blk_size; i++){
                        block_sp_input_wgt_buf[block_sp_input_wgt_idx + i] = inst->data[i];
                    }
                    block_sp_input_wgt_idx += config_.sparsemm_feature_size;

                    
                    printf("[RETURN DATA READY] removing from return_queue hex_addr: 0x%lx\n", inst->hex_addr);
                    inst = return_queue_.erase(inst);
                    printf("[RETURN DATA READY] return queue size: %lu\n", return_queue_.size());
                }
            }

            //check if buffers filled after 
            if(block_sp_input_act_idx == num_block_sp_input && block_sp_input_wgt_idx == num_block_sp_input){
                printf("[RETURN DATA READY] both buffers filled, making sparseElement, clk: %lu, hardware clk: %lu\n",
                clk_, hardware_clk_);
                
                SparseMatmulElement element = SparseMatmulElement(
                    inst->sparsemm_idx,
                    block_sp_input_act_buf,
                    block_sp_input_wgt_buf
                );
                sparse_.push_back(element);            

                block_sp_input_act_idx = 0;
                block_sp_input_wgt_idx = 0;
            }
        }
        
        //if diffprune, blocksize = 1
        else if(inst->opcode == Opcode::SPARSE && config_.sparsemm_blk_size == 1){
            printf("[RETURN DATA READY] diffprune\n");
            if(sparse_.size() == (uint64_t)num_blocksp_kernels){
                printf("[RETURN DATA READY] all %u sparse_ filled, skip\n", num_blocksp_kernels);
            }

            //if either buffer is empty, fill them!
            if(spgemm_input_act_idx != req_num_inst_act_buf || spgemm_input_wgt_idx != req_num_inst_wgt_buf){
                if(inst->cache_hit == 0){//if activation
                    printf("[RETURN DATA READY] activation, idx: %u\n", spgemm_input_act_idx);
                    //add to act buffer, do later
                    spgemm_input_act_idx += 1;
                    
                    printf("[RETURN DATA READY] removing from return_queue hex_addr: 0x%lx\n", inst->hex_addr);
                    inst = return_queue_.erase(inst);
                    printf("[RETURN DATA READY] return queue size: %lu\n", return_queue_.size());
                }
                else{//if weight
                    printf("[RETURN DATA READY] weight, idx: %u\n", spgemm_input_wgt_idx);
                    //add to wgt buffer, do later
                    spgemm_input_wgt_idx += 1;
                    
                    printf("[RETURN DATA READY] removing from return_queue hex_addr: 0x%lx\n", inst->hex_addr);
                    inst = return_queue_.erase(inst);
                    printf("[RETURN DATA READY] return queue size: %lu\n", return_queue_.size());
                }
            }
            
            //if buffers fulled
            //printf("[RETURN DATA READY] activation, idx: %u\n", spgemm_input_act_idx);
            if(spgemm_input_act_idx == req_num_inst_act_buf && spgemm_input_wgt_idx == req_num_inst_wgt_buf){
                printf("[RETURN DATA READY] both buffers filled, making sparse_ element, clk: %lu, hardware_clk: %lu\n",
                clk_, hardware_clk_);

                SparseMatmulElement element = SparseMatmulElement(
                    inst->sparsemm_idx,
                    spgemm_input_act_buf,
                    spgemm_input_wgt_buf
                );
                sparse_.push_back(element);

                spgemm_input_act_idx = 0;
                spgemm_input_wgt_idx = 0;
            }

        }

        else if(inst->opcode == Opcode::ACTIVATION){

        }

        else {
            std::cerr << "unknown opcode" << (int)inst->opcode << std::endl;
            AbruptExit(__FILE__, __LINE__);
        }
    }

    printf("[RETURN DATA READY] num inst added: %u\n", num_inst_added);
    if(num_inst_added == 0){
        return false;
    }
    else{
        return true;
    }
    
}

void PNM::ExecuteAdder(){
    printf("[EXECUTE ADDER]\n");
    auto adder_it = adder_.begin();
    printf("psum_idx: %u\n", adder_it->psum_idx);
    auto it = psum.find(index_to_address(
                            psum_offset[psum_offset_idx] + adder_it->psum_idx,
                            PNMRegister::PSUM));
    if (it == psum.end()) {
        std::cerr << "unknown address" 
                  << adder_it->psum_idx << "  "
                  << std::hex
                  << index_to_address(adder_it->psum_idx, PNMRegister::PSUM)
                  << std::dec
                  << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    
    for (int i = 0; i < config_.sparse_feature_size; i++) {
       printf("psum_data[i] : %f + dram_data[i] : %f ",
       adder_it->psum_data[i], adder_it->dram_data[i]);
       it->second[i] = adder_it->psum_data[i] + adder_it->dram_data[i];
       printf("result: %f\n", it->second[i]);
    }
    printf("psum_addr: 0x%lx\n", it->first);
    adder_.erase(adder_it);
    num_accum++;

    if (num_accum + num_densemm + num_sparsemm == num_read_inst) {
        printf("channel: %u finished, force stopping for now\n", channel_id_);
        
        num_accum = -1;
        num_densemm = -1;
        num_sparsemm = -1;
        num_read_inst = -1;
        sls_exec = false;
        std::cerr << "FORCE STOP" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
}

void PNM::ExecuteSparseMatmul(){
    auto sparse_it = sparse_.begin();
    int selected_kernel = -1;
    //check if any sparse kernels finished
    for(int i = 0; i < num_blocksp_kernels; i++){
        //kernel finished, 
        if(blocksp_kernel_status[i] == 1 && hardware_clk_ >= (uint64_t)sparse_kern_end_clk[i]){
            printf("[EXECUTE SPARSEMATMUL] sparse kernel %u finished\n",i);
            blocksp_kernel_status[i] = 0;
        }
        //kernel still running
        if(blocksp_kernel_status[i] == 1 && hardware_clk_ < (uint64_t)sparse_kern_end_clk[i]){
            sparse_kern_busy_cnt[i]++;
        }
        //kernel not busy (nothing running in it)
        if(blocksp_kernel_status[i] == 0){
            selected_kernel = i;
        }
    }

    //if no available kernels
    if(selected_kernel == -1){
        printf("[EXECUTE SPARSEMATMUL] all kernels busy\n");
        return;
    }
    printf("[EXECUTE SPARSEMATMUL] %u sparse kernel selected\n", selected_kernel);


    //update status of selected kernel
    blocksp_kernel_status[selected_kernel] = 1;
    //if block32 or block16
    if(config_.sparsemm_blk_size != 1){
        sparse_kern_end_clk[selected_kernel] = hardware_clk_ + config_.sparsemm_blk_size*2;
    }
    //if diffprune
    else{
        sparse_kern_end_clk[selected_kernel] = hardware_clk_ + kern_num_sp_elements;
    }
    sparse_kern_busy_cnt[selected_kernel]++;

    printf("[EXECUTE SPARSEMATMUL] kern %u finishes at clk: %u\n", selected_kernel, sparse_kern_end_clk[selected_kernel]);

    printf("[EXECUTE SPARSEMATMUL] sparsemm_idx: %u\n", sparse_it->sparsemm_idx);
    int output_matrix_size = config_.sparsemm_blk_size * config_.sparsemm_blk_size;
    float* C_tmp = new float[output_matrix_size];
    for(int i = 0; i < output_matrix_size; i++){
        C_tmp[i] = 0;
    }

    //simulate 32 by 32 systolic array
    for(int i = 0; i < config_.sparsemm_blk_size; i++){
        for(int j = 0; j < config_.sparsemm_blk_size; j++){
            for(int k = 0; k < config_.sparsemm_blk_size; k++){
                C_tmp[i*config_.sparsemm_blk_size] +=
                sparse_it->sparsemm_act[i*config_.sparsemm_blk_size+k] *
                sparse_it->sparsemm_wgt[j*config_.sparsemm_blk_size+k];
            }
        }
    }

    //for block32, write 32*32/16 (64)times
    //for block16, write 16*16/16 (32)times
    for(int i = 0; i < output_matrix_size/16; i++){
        auto sparse_wr_it = sparsemm_buf.find(
            index_to_address(
                sparsemm_offset[sparsemm_offset_idx] + sparse_it->sparsemm_idx + i,
                PNMRegister::SPARSEMM
            )
        );

        if(sparse_wr_it == sparsemm_buf.end()){
            std::cerr << "error when writing result to sparsemmbuf" 
                  << sparse_it->sparsemm_idx << "  "
                  << std::hex
                  << index_to_address(sparse_it->sparsemm_idx, PNMRegister::DENSEMM)
                  << std::dec
                  << std::endl;
            AbruptExit(__FILE__, __LINE__);
        }

        for(int j = 0; j < 16; j++){
            sparse_wr_it->second[j] += C_tmp[i*16+j];
        }
    }

    delete [] C_tmp;
    sparse_.erase(sparse_it);

    if(config_.sparsemm_blk_size == 32){
        num_sparsemm += 128;
    }
    else if(config_.sparsemm_blk_size == 16){
        num_sparsemm += 32;
    }
    else if(config_.sparsemm_blk_size == 1){
        num_sparsemm += req_num_inst_act_buf + req_num_inst_wgt_buf;
    }
    else{
        num_sparsemm = 0;
        std::cerr << "[EXECUTE SPARSEMATMUL]unsupported blocksize" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    printf("[EXECUTE SPARSEMATMUL] num_sparsemm/num_read_inst: %u/%u\n", 
    num_sparsemm, num_read_inst);

    if (num_accum + num_densemm + num_sparsemm == num_read_inst) {
        printf("FINISHED channel: %u\n", channel_id_);
        printf("setting sls_exec to false\n");
        printf("num_accum: %u, num_densemm: %u, num_sparsemm: %u, num_read_inst: %u\n",
        num_accum, num_densemm, num_sparsemm, num_read_inst);
        num_accum = -1;
        num_densemm = -1;
        num_sparsemm = -1;
        num_read_inst = -1;
        sls_exec = false;
        end_clk = hardware_clk_;

        //float utilization = 100 * ((double)hardware_busy_clk_cnt)/(end_clk - start_clk);
        //printf("hardware busy clk cnt: %u\n", hardware_busy_clk_cnt);
        //printf("percent hardware utilization: %f\n", utilization);
        std::vector<double> utils;
        for(int i = 0; i < num_blocksp_kernels; i++){
            float utilization = 100 * ((double)sparse_kern_busy_cnt[i])/(end_clk - start_clk);
            printf("kernel %u utilization: %f\n", i, utilization);
            utils.push_back(utilization);
        }
        
        printf("PRINT UTIL STATS\n");
        PrintUtilStats(utils);
        //printf("start clk: %u, end clk: %u, hardware busy clk cnt : %u\n",
        //start_clk, end_clk, hardware_busy_clk_cnt);
        //printf("num cycles that added to buf: %u\n", add_to_buf_cnt);

        //std::ofstream txt_out(config_.txt_stats_name, std::ofstream::out);
        //txt_out << "hardware_utilization: \t" << std::to_string(utilization) << std::endl;
        

    }

}

void PNM::ExecuteDenseMatmul(){
    //printf("[EXECUTE DENSEMATMUL]\n");
    auto dense_it = dense_.begin();
    
    //if sys array currently running
    if(sys_array_busy == true){
        //if sys array finished
        if(hardware_clk_ >= (uint64_t)sys_array_end_clk){
            printf("[EXECUTE DENSEMATMUL] sys array finished, removing from dense_\n");
            sys_array_busy = false;
            dense_.erase(dense_it);
            printf("[EXECUTE DENSEMATMUL] dense_ size: %lu\n", dense_.size());
        }
        //not finished
        else{
            hardware_busy_clk_cnt++;
            printf("[EXECUTE DENSEMATMUL] hardware busy\n");
        }
        return;
    }
    
    printf("[EXECUTE DENSEMATMUL] densemm_idx: %u\n", dense_it->densemm_idx);
    //int sidelength = config_.mm_sidelength;

    //print matrixes before matmul
    //printf("[EXECUTE DENSEMATMUL] densemm_activations (A) - first 16: \n\t");
    //for(int i = 0; i < 16; i++){
    //    if(i % sidelength == 0 && i!=0){
    //        printf("\n\t");
    //    }
    //    printf("%f ", dense_it->densemm_act[i]);
    //}
    //printf("\n");

    //printf("[EXECUTE DENSEMATMUL] densemm_weights (B) - first 16: \n\t");
    //for(int i = 0; i < 16; i++){
    //    if(i % sidelength == 0 && i!=0){
    //        printf("\n\t");
    //    }
    //    printf("%f ", dense_it->densemm_wgt[i]);
    //}
    //printf("\n");

    //float* A_copy = new float[config_.densemm_feature_size];
    float* C_tmp = new float[128*128];

    for(int i = 0; i < config_.densemm_feature_size; i++){
        C_tmp[i] = 0;
    }

    //sys array busy control signals
    sys_array_busy = true;
    sys_array_start_clk = hardware_clk_;
    sys_array_end_clk = hardware_clk_ + 128+ 256;
    hardware_busy_clk_cnt++;

    //simulate 128 by 128 systolic array
    for(int i = 0; i < 128; i++){//activation row, output row
        for(int j = 0; j < 128; j++){//weight col, output col
            for(int k = 0; k < 256; k++){//activation col/weight row
                C_tmp[i*128+j] += dense_it->densemm_act[i*128+k] * dense_it->densemm_wgt[k+128*j];
            }
        }
    }

    //write result to densemm_buf
    //in real life, write while output comes out of sys array
    //each densemmbuf element holds x16 32fp values
    //128x128 / 16 = 1024
    for(int i = 0; i < 1024; i++){

        //find iterator that points to correct densemm
        auto dense_wr_it = densemm_buf.find(
            index_to_address(
                densemm_offset[densemm_offset_idx] + dense_it->densemm_idx + i,
                PNMRegister::DENSEMM
            )
        );

        //check if something gone wrong
        if(dense_wr_it == densemm_buf.end()){
            std::cerr << "error when writing result to densemmbuf" 
                  << dense_it->densemm_idx << "  "
                  << std::hex
                  << index_to_address(dense_it->densemm_idx, PNMRegister::DENSEMM)
                  << std::dec
                  << std::endl;
            AbruptExit(__FILE__, __LINE__);
        }

        //set each 32fp value 
        for(int j = 0; j < 16; j++){
            dense_wr_it->second[j] += C_tmp[i*16+j];
        }
    }

    //printf("[EXECUTE DENSEMATMUL] resultant (C), after matmul: \n\t");
    //for(int i = 0; i < 128*128; i++){
    //    if(i % 128 == 0 && i!=0){
    //        printf("\n\t");
    //    }
    //    printf("%f ", C_tmp[i]);
    //}
    //printf("\n");
    //printf("[EXECUTE DENSEMATMUL] resultant (C) first 16\n\t");
    //for(int i = 0; i < 16; i++){
    //    if(i % sidelength == 0 && i!=0){
    //        printf("\n\t");
    //    }
    //    printf("%f ", C_tmp[i]);
    //}
    //printf("\n");
    

    //delete [] dense_it->densemm_act;
    //delete [] dense_it->densemm_wgt;
    delete [] C_tmp;
    
    //dense_.erase(dense_it);
    //num_densemm+=2;
    num_densemm +=4096;
    printf("[EXECUTE DENSEMATMUL] num_densemm/num_read_inst: %u/%u\n",
    num_densemm, num_read_inst);


    if (num_accum + num_densemm + num_sparsemm == num_read_inst) {
        printf("FINISHED channel: %u\n", channel_id_);
        printf("setting sls_exec to false\n");
        printf("num_accum: %u, num_densemm: %u, num_sparsemm: %u, num_read_inst: %u\n",
        num_accum, num_densemm, num_sparsemm, num_read_inst);
        num_accum = -1;
        num_densemm = -1;
        num_sparsemm = -1;
        num_read_inst = -1;
        sls_exec = false;
        end_clk = hardware_clk_;

        float utilization = 100 * ((double)hardware_busy_clk_cnt)/(end_clk - start_clk);
        printf("hardware busy clk cnt: %u\n", hardware_busy_clk_cnt);
        printf("percent hardware utilization: %f\n", utilization);
        printf("start clk: %u, end clk: %u, hardware busy clk cnt : %u\n",
        start_clk, end_clk, hardware_busy_clk_cnt);
        printf("num cycles that added to buf: %u\n", add_to_buf_cnt);

        std::vector<double>utils;
        utils.push_back(utilization);
        PrintUtilStats(utils);

        //std::ofstream txt_out(config_.txt_stats_name, std::ofstream::out);
        //txt_out << "hardware_utilization: \t" << std::to_string(utilization) << std::endl;
        

    }
    
}

std::pair<uint64_t, int> PNM::ReturnDoneTrans(uint64_t clock) {
    //idk
    auto pair = ctrl_.ReturnDoneTrans(clock);
    if (pair.second == -1) return pair; // nothing
    if (pair.second == 1) return pair; // write

    //how many times this was requested
    auto num_reads = requested_rd_q_.count(pair.first);
    printf("[RETURN DONE TRANS] clock: %lu, hex addr: 0x%lx, num_reads: %lu\n", clock, pair.first, num_reads);
    
    auto rq_it = return_queue_.begin();
    //make deterministic data that was retreived from memory
    float* returned_data;

    if(rq_it->opcode == Opcode::DENSE){
        returned_data = new float[config_.densemm_feature_size];
        for(int i =0; i < config_.densemm_feature_size; i++){
            returned_data[i] = (rand()%10000)/20000.0;
        }
    }
    else if(rq_it->opcode == Opcode::SPARSE){
        returned_data = new float[config_.sparsemm_feature_size];
        for(int i = 0; i < config_.sparsemm_feature_size; i++){
            returned_data[i] = (rand()%10000)/20000.0;
        }
    }
    else{
        std::cerr << "unsupported opcode" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    while (num_reads > 0) {
        //printf("[RETURN DONE TRANS] num_reads: %lu\n", num_reads);
        auto it = requested_rd_q_.find(pair.first);
        int count = 0;
        //find the instruction(s) in return_queue
        //printf("traversing return queue\n");
        while(rq_it->hex_addr != it->second.hex_addr && rq_it != return_queue_.end()){
            rq_it ++;
            count++;
            //printf("idx: %u, hex_addr: 0x%lx\n", count, rq_it->hex_addr);
        }
        if(rq_it == return_queue_.end()){
            printf("something wrong\n");
            AbruptExit(__FILE__, __LINE__);
        }
        //printf("found same instruction in return_queue at index: %u\n", count);
        rq_it->complete_cycle = clock;
        //printf("[RETURN DONE TRANS] updating instruction's complete cycle: %lu\n", rq_it->complete_cycle);
        

        //auto rq_it = return_queue_.find(pair.first);

        if(it->second.opcode == Opcode::SUM){
            it->second.complete_cycle = clock;
            it->second.data = new float[config_.sparse_feature_size];

            //set data as current clock???
            //just accumulating current clock cycles over and over again...?
            for (int i = 0; i < config_.sparse_feature_size; i++) {
                it->second.data[i] = clk_;
            }
            printf("updating instruction data to hold %lu (current clk cycle)\n", clk_);
            printf("psum_idx: 0x%x, hex_addr: 0x%lx, data: [",
            it->second.psum_idx, it->second.hex_addr);
            for(int j = 0; j < config_.sparse_feature_size; j++){
                printf("%f ", it->second.data[j]);
            }
            printf("]\n");
            //add to return queue
            return_queue_.push_back(it->second);

            //erase from requested queue
            requested_rd_q_.erase(it);
            num_reads -= 1;
        }
        
        else if(it->second.opcode == Opcode::DENSE){
            //printf("instruction complete cycle : %lu\n", clock);
            it->second.complete_cycle = clock;
            rq_it->complete_cycle = clock;
            //it->second.data = new float[config_.densemm_feature_size];
            //rq_it->data = new float[config_.densemm_feature_size];
            //rq_it->data = it->second.data;

            //update data for instruction in return queue
            for(int i = 0; i < config_.densemm_feature_size; i++){
                //set data as clk cycle as placeholder
                //it->second.data[i] = clk_/1000.0;
                //it->second.data[i] = (rand()%10000)/20000.0;
                //it->second.data[i] = returned_data[i];
                rq_it->data[i] = returned_data[i];
            }

            if(num_reads == 1){
                //printf("return queue size: %lu\n", return_queue_.size());
                //printf("[RETURN DONE TRANS] print return queue\n");
                //for(unsigned i = 0; i < return_queue_.size(); i++){
                //    printf("\ti: %u, hex_addr: 0x%lx, complete_cycle: %lu, data[0]: %f\n",
                //    i, return_queue_[i].hex_addr, return_queue_[i].complete_cycle, return_queue_[i].data[0]);
                //}
                printf("[RETURN DONE TRANS] return queue size: %lu\n", return_queue_.size());            
            }
            
            //print instruction with updated data
            //printf("[RETURN DONE TRANS] densemm_idx: 0x%x, hex_addr: 0x%lx, data: [",
            //rq_it->densemm_idx, rq_it->hex_addr);
            //for(int j = 0; j < config_.densemm_feature_size; j++){
            //    printf("%f ", rq_it->data[j]);
            //}
            //printf("]\n");

            printf("[RETURN DONE TRANS] add to data cache and update access time\n");
            //add new data to cache
            data_cache_.insert(
                std::pair<uint64_t, float*>(
                    rq_it->hex_addr, rq_it->data
                )
            );
            //update new data access time
            cache_access_time.insert(
                std::pair<uint64_t, int>(
                    it->second.hex_addr, clk_
                )
            );


            //return_queue_.push_back(it->second);
            //if(num_reads == 1){
            
            //printf("inst complete cycle: %lu\n", it->second.complete_cycle);
            //delete [] it->second.data;
            requested_rd_q_.erase(it);
            num_reads -= 1;
        }
        
        else if(it->second.opcode == Opcode::SPARSE){
            it->second.complete_cycle = clock;
            rq_it ->complete_cycle = clock;

            //update data for instruction in return queue
            for(int i = 0; i < config_.sparsemm_feature_size; i++){
                rq_it->data[i] = returned_data[i];
            }

            data_cache_.insert(
                std::pair<uint64_t, float*>(rq_it->hex_addr, rq_it->data)
            );

            cache_access_time.insert(
                std::pair<uint64_t, int>(it->second.hex_addr, clk_)
            );

            requested_rd_q_.erase(it);
            num_reads -= 1;

            //printf("[RETURN DONE TRANS] cache size: %lu\n", data_cache_.size());
        }

        //evict from cache based on access time
        printf("[RETURN DONE TRANS] cache size: %lu\n", data_cache_.size());
        //each data element is one instruction, each instruction holds 64B
        if(data_cache_.size() == DATA_CACHE_BYTE_SIZE/64){
        //if(0){ //infinite cache
            int min = INT_MAX;
            auto min_it = cache_access_time.begin();
            for(auto xs_it = cache_access_time.begin(); xs_it != cache_access_time.end(); xs_it++){
                if(xs_it->second < min){
                    min = xs_it->second;
                    min_it = xs_it;
                }
            }
            printf("[RETURN DONE TRANS] datacache full, evicting hex_addr: %lx\n", min_it->first);
            if(min_it != cache_access_time.end()){
                //deallocate memory
                auto evict_data = data_cache_[min_it->first];
                data_cache_.erase(min_it->first);
                delete [] evict_data;
                cache_access_time.erase(min_it);
            }
        }
        rq_it++;
        count++;
    }
    delete [] returned_data;
    return pair; 
}

void PNM::PrintUtilStats(std::vector<double> utils){
    ctrl_.PrintUtilStats(utils);
}
} // namespace dramsim3
