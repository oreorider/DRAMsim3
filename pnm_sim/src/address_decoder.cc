#include "address_decoder.h"

namespace dramsim3 {

AddrDecoder::AddrDecoder(int channel, const Config &config,
                         const Timing &timing)
    : channel_id_(channel),
      clk_(0),
      config_(config) {

    trans_queue_.reserve(config_.trans_queue_size);
    return_queue_.reserve(config_.trans_queue_size); // output_buffer
    ctrl_ = new Controller(channel_id_, config_, timing);
    pnm_ = new PNM(channel_id_, config_, timing, *ctrl_);
#ifdef CMD_TRACE
    std::string trace_file_name = "./pnm_ch" +
                                  std::to_string(channel_id_) + "_cmd.trace";
    std::cout << "Command Trace write to " << trace_file_name << std::endl;
    pnm_trace_.open(trace_file_name, std::ofstream::out);
#endif  // CMD_TRACE
}

bool AddrDecoder::WillAcceptTransaction(uint64_t hex_addr, bool is_write,
                                        FenceMode fence_mode) const {
    if (fence_mode == FenceMode::NONE) {
        // just for transfer the transaction.
        return trans_queue_.size() < trans_queue_.capacity();
    }
    if (fence_mode == FenceMode::MFENCE) {
        return IsAllReadCommandDone()
               && IsAllWriteCommandDone()
               && IsAllTransSent();
    }
    if (fence_mode == FenceMode::LFENCE) {
        return IsAllReadCommandDone()
               && IsAllTransSent();
    }
    if(fence_mode == FenceMode::SFENCE) {
        return IsAllWriteCommandDone()
               && IsAllTransSent();
    }
    if (fence_mode == FenceMode::DONE) {
        return IsPNMDone()
               && IsAllTransSent();
    }
    std::cerr << " not supported " << std::endl;
    exit(1);
    return false;
}

bool AddrDecoder::AddTransaction(Transaction trans) {
    trans_queue_.push_back(trans);
    return true;
}

void AddrDecoder::SendTransaction() {
    // add transaction to controller or pnm.
    auto trans = trans_queue_.begin();
    if (trans == trans_queue_.end()) return;

    auto pnm_addr = HexToPNMAddress(trans->addr, &config_); 
    if (trans->IsPNM(pnm_addr)) {
        if (trans->is_write) {
            // write instruction, config register
            pnm_->Write(*trans, pnm_addr);
#ifdef CMD_TRACE
            pnm_trace_ << std::left << std::setw(18)
                       << clk_ << " PNM " << *trans << std::endl;
#endif  // CMD_TRACE
        } else {
            // read psum
            trans->data = (void*) pnm_->Read(trans->addr, pnm_addr);
            trans->complete_cycle = clk_ + PNM_PSUM_READ_LATENCY;
            return_queue_.push_back(*trans);
#ifdef CMD_TRACE
            pnm_trace_ << std::left << std::setw(18)
                       << clk_ << " PNM " << *trans << std::endl;
#endif  // CMD_TRACE
        }
        trans_queue_.erase(trans);
    }
    // normal DRAM transaction
    else {
        if (ctrl_->WillAcceptTransaction(trans->addr, trans->is_write)) {
            ctrl_->AddTransaction(*trans);
            trans_queue_.erase(trans);
        }
    }
}

void AddrDecoder::ClockTick() {
    // Return PSUM
    if (!return_queue_.empty())
        ReturnReadData();

    
    ctrl_->ClockTick();
    if(clk_ % 1 == 0){
        pnm_->ClockTick();
    }

    // send transaction to PNM/controller
    SendTransaction();

    clk_++;
}

void AddrDecoder::PrintEpochStats() {
    ctrl_->PrintEpochStats();
}

void AddrDecoder::PrintFinalStats() {
    ctrl_->PrintFinalStats();
}

void AddrDecoder::ResetStats() {
    ctrl_->ResetStats();
}

std::pair<uint64_t, int> AddrDecoder::ReturnDoneTrans(uint64_t clk) {
    return pnm_->ReturnDoneTrans(clk);
}

void AddrDecoder::ReturnReadData() {
    auto it = return_queue_.begin();
    for (; it != return_queue_.end(); it++) {
        if (it->complete_cycle < clk_) {
            // return read data
            break;
        }
    }
    // there is nothing to return
    if (it == return_queue_.end()) return;
    // return 'it'
    return_queue_.erase(it);
    return;
}

bool AddrDecoder::IsAllQueueEmpty() const {
    return (ctrl_->IsAllQueueEmpty() && return_queue_.empty());
}

bool AddrDecoder::IsAllReadCommandDone() const {
    return ctrl_->IsReadQueueEmpty();
}

bool AddrDecoder::IsAllWriteCommandDone() const {
    return ctrl_->IsWriteQueueEmpty(); 
}

bool AddrDecoder::IsPNMDone() const {
    return pnm_->Done();
}

bool AddrDecoder::IsAllTransSent() const {
    return (trans_queue_.size() == 0); 
}

uint64_t AddrDecoder::HexToPNMAddress(uint64_t hex_addr,
                                      const Config* config_p_) {
    printf("[ADDR DECODER - hex_to_pnm_addr]\t hex_addr: 0x%lx\n", hex_addr);
    uint64_t pnm_addr = 0;
    uint64_t addr_mask = ((uint64_t)1 << config_p_->addr_bits) - 1;
    uint64_t low_mask = ((uint64_t)1 
                        << (config_p_->shift_bits + config_p_->ch_pos)) - 1;
    uint64_t low_eraser = ~low_mask;
    pnm_addr += ((hex_addr & addr_mask) >> config_p_->ch_bits) & low_eraser;
    pnm_addr += hex_addr & low_mask;

    return pnm_addr;
}
}  // namespace dramsim3
