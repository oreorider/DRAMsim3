#include "pnm.h"


namespace dramsim3 {

Instruction::Instruction(uint64_t inst_data, int channel_id,
                         const Config *config_p_) {
    config_p = config_p_;

    opcode         = static_cast<Opcode>((inst_data >> 62) & 0x3);
    if (opcode == Opcode::SUM) {
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

    } else {
        std::cerr << "unknown opcode" << (int)opcode << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
    hex_addr = addr.GetHexAddress();
}

PNM::PNM(int channel, const Config &config, const Timing &timing,
         Controller& ctrl)
    : channel_id_(channel),
      clk_(0),
      config_(config),
      sls_exec(false), program_count(-1),
      num_write_inst(0), num_read_inst(-1), num_accum(-1),
      inst_offset_idx(0), psum_offset_idx(0),
      ctrl_(ctrl)
{
    auto num_inst = config_.instruction_buffer_size / config_.instruction_size;
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

    auto num_psum = config_.psum_buffer_size
                    / config_.sparse_feature_size
                    / config_.data_size;

    for (int i = 0; i < num_psum; i++) {
        float *t_data = new float[config_.sparse_feature_size];
        for (int j = 0; j < config_.sparse_feature_size; j++) {
            t_data[j] = 0;
        }
        psum.insert(std::pair<uint64_t, float *>(
                              index_to_address(i, PNMRegister::PSUM),
                                        t_data));
    }

    read_psum_offset.push_back(0);
    if (config_.psum_double == true) {
        psum_offset.push_back(num_psum / 2);
        read_psum_offset.push_back(index_to_address(num_psum / 2,
                                                    PNMRegister::PSUM) 
                                   - index_to_address(0, PNMRegister::PSUM));
    }
    psum_offset.push_back(0);

}

bool PNM::Done() {
    if ((num_write_inst == 0) &&
        (program_count == -1) &&
        (num_accum == -1)) return true;

    return false;
}

void PNM::ClockTick() {
    if (!sls_exec) {
        clk_++;
        // Nothing 
        return;
    }
    // 1. Adder -> PSUM
    if (!adder_.empty()) {
        ExecuteAdder();
    }

    // 2. Data in return_queue_ -> SLS op
    if (!return_queue_.empty()) {
        ReturnDataReady();
    }

    // 3-1. Cache check
    else if (!cache_q_.empty()) {
        ReadCache();
    }

    // 4. Instruction fetch -> Send to controller
    ScheduleInstruction();

    clk_++;
    return;
}

void PNM::Write(Transaction trans, uint64_t pnm_addr) {

    // instruction
    if ((pnm_addr >= PNM_INST_BUF_START)
         && (pnm_addr <= PNM_INST_BUF_END)) {
        WriteInstruction(trans);
    }
    // control register
    else if ((pnm_addr >= PNM_CONFIG_REG_START)
              && (pnm_addr <= PNM_CONFIG_REG_END)) {
        WriteConfigRegister(trans);
    }
    else {
        std::cerr << " Write: unknown pnm address " << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
    return;
}

void PNM::WriteInstruction(Transaction trans) {

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
    }
    delete[] t_data;

    return ;
}

void PNM::WriteConfigRegister(Transaction trans) {

    if (trans.addr == Address(channel_id_,
                              true,
                              PNM_CONFIG_REG_START + PNM_EXE_REG_OFFSET,
                              &config_).GetHexAddress()) { // SLS EXEC
        if (trans.data_size == 0)
            AbruptExit(__FILE__, __LINE__);


        uint64_t* t_data = (uint64_t*) trans.data;
        sls_exec = t_data[0] == 0xcafe ? true : false;

        if (sls_exec) {
            program_count = 0;
            num_read_inst = 0;
            num_accum = 0;
            inst_offset_idx = (inst_offset_idx + 1)
                              % write_instruction_offset.size();
            psum_offset_idx = (psum_offset_idx + 1)
                              % psum_offset.size();

#ifdef DEBUG_PRINT
            std::cout << clk_ << " sls exec enable " 
                      << channel_id_ << "  "
                      << num_write_inst << "  "
                      << std::endl;
            num_input = 0;
            num_weight = 0;
#endif
        }
    }
    else {
         std::cerr << " unknown config register write " << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    return;
}


float *PNM::Read(uint64_t hex_addr, uint64_t pnm_addr) {

    // psum
    if ((pnm_addr >= PNM_PSUM_BUF_START)
         && (pnm_addr <= PNM_PSUM_BUF_END)) {
        return ReadPsumData(hex_addr);
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

float* PNM::ReadPsumData(uint64_t hex_addr) {

    auto it = psum.find(read_psum_offset[psum_offset_idx] + hex_addr);
    if (it == psum.end()) {
        std::cout << "psum buffer idx: "
                  << psum_offset_idx << "  "
                  << std::hex << hex_addr << std::dec << "  "
                  << read_psum_offset[psum_offset_idx]
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
        default:
            idx_addr = -1;
            std::cerr << "unknown reg_mode" << std::endl;
            AbruptExit(__FILE__, __LINE__);
            break;
    }

    return Address(channel_id_, true, idx_addr, &config_).GetHexAddress();
}


void PNM::ScheduleInstruction() {
    if (program_count == -1)
        return;
    auto it = inst_buf.find(index_to_address(pc_offset[inst_offset_idx]
                                             + program_count,
                                             PNMRegister::INSTRUCTION));
    if (it == inst_buf.end()) {
        std::cerr<< "instruction fetching error " << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    if (it->second.cache_hit == false) {
        Transaction trans = InstructionToTransaction(it->second);
        if (requested_rd_q_.count(it->second.hex_addr) == 0) {
            if (ctrl_.WillAcceptTransaction(trans.addr, trans.is_write)) {
                ctrl_.AddTransaction(trans);
            } else {
                return;
            }
        }
        requested_rd_q_.insert(std::make_pair(it->second.hex_addr, it->second));
    }
    else {
        if (cache_q_.size() < 8) {
            // = max cache queue size
            cache_q_.push_back(it->second);
        }
        else
            return;
    }

    program_count++;
    if (it->second.trace_end == 1){
        // program_cout == num_write_inst
#ifdef DEBUG_PRINT
        std::cout << "trace_end: " 
                  << program_count << " " 
                  << num_write_inst << std::endl;
#endif
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
    auto it_c = weight_cache_.find(it->hex_addr);

    if (it_c == weight_cache_.end()){
        return;
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

    it->complete_cycle = clk_ + 1;
    it->data = new float[config_.sparse_feature_size];
    for (int i = 0; i < config_.sparse_feature_size; i++) {
        it->data[i] = it_c->second[i];
    }

    return_queue_.push_back(*it);
    cache_q_.erase(it);
}

void PNM::ReturnDataReady() {
    auto inst = return_queue_.begin();
    for (; inst != return_queue_.end(); inst++) {
        if (inst->complete_cycle >= clk_)
            continue;
        else // read data ready 
            break;
    }
    if (inst == return_queue_.end()) return; // nothing is ready

    if (inst->opcode == Opcode::SUM) {
        auto psum_it = psum.find(index_to_address(
                                  psum_offset[psum_offset_idx] + inst->psum_idx,
                                  PNMRegister::PSUM));
        AdderElement adder = AdderElement(inst->psum_idx,
                                          psum_it->second,
                                          inst->data);
        adder_.push_back(adder);
    }
    else {
        std::cerr << "unknown opcode" << (int)inst->opcode << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    return_queue_.erase(inst);
}

void PNM::ExecuteAdder(){
    auto adder_it = adder_.begin();
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
       it->second[i] = adder_it->psum_data[i] + adder_it->dram_data[i];
    }
    adder_.erase(adder_it);
    num_accum++;

    if (num_accum == num_read_inst) {
        num_accum = -1;
        num_read_inst = -1;
        sls_exec = false;
    }
}

std::pair<uint64_t, int> PNM::ReturnDoneTrans(uint64_t clock) {
    auto pair = ctrl_.ReturnDoneTrans(clock);
    if (pair.second == -1) return pair; // nothing
    if (pair.second == 1) return pair; // write
    auto num_reads = requested_rd_q_.count(pair.first);
    while (num_reads > 0) {
        auto it = requested_rd_q_.find(pair.first);
        it->second.complete_cycle = clock;
        it->second.data = new float[config_.sparse_feature_size];
        for (int i = 0; i < config_.sparse_feature_size; i++) {
            it->second.data[i] = clk_;
        }
        return_queue_.push_back(it->second);
        requested_rd_q_.erase(it);
        num_reads -= 1;
    }

   return pair; 
}
} // namespace dramsim3
