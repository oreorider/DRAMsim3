#include "dram_system.h"

#include <assert.h>

namespace dramsim3 {

// alternative way is to assign the id in constructor but this is less
// destructive
int BaseDRAMSystem::total_channels_ = 0;

BaseDRAMSystem::BaseDRAMSystem(Config &config, const std::string &output_dir,
                               std::function<void(uint64_t)> read_callback,
                               std::function<void(uint64_t)> write_callback)
    : read_callback_(read_callback),
      write_callback_(write_callback),
      last_req_clk_(0),
      config_(config),
      timing_(config_),
#ifdef THERMAL
      thermal_calc_(config_),
#endif  // THERMAL
      clk_(0),
      pnm_clk_(0) {
    total_channels_ += config_.channels;

#ifdef ADDR_TRACE
    std::string addr_trace_name = config_.output_prefix + "addr.trace";
    address_trace_.open(addr_trace_name);
#endif
}

int BaseDRAMSystem::GetChannel(uint64_t hex_addr) const {
    hex_addr >>= config_.shift_bits;
    return (hex_addr >> config_.ch_pos) & config_.ch_mask;
}

void BaseDRAMSystem::PrintEpochStats() {
    // first epoch, print bracket
    if (clk_ - config_.epoch_period == 0) {
        std::ofstream epoch_out(config_.json_epoch_name, std::ofstream::out);
        epoch_out << "[";
    }
    for (size_t i = 0; i < decoders_.size(); i++) {
        decoders_[i]->PrintEpochStats();
        std::ofstream epoch_out(config_.json_epoch_name, std::ofstream::app);
        epoch_out << "," << std::endl;
    }
#ifdef THERMAL
    thermal_calc_.PrintTransPT(clk_);
#endif  // THERMAL
    return;
}

void BaseDRAMSystem::PrintStats() {
    // Finish epoch output, remove last comma and append ]
    std::ofstream epoch_out(config_.json_epoch_name, std::ios_base::in |
                                                         std::ios_base::out |
                                                         std::ios_base::ate);
    epoch_out.seekp(-2, std::ios_base::cur);
    epoch_out.write("]", 1);
    epoch_out.close();

    std::ofstream json_out(config_.json_stats_name, std::ofstream::out);
    json_out << "{";

    // close it now so that each channel can handle it
    json_out.close();
    for (size_t i = 0; i < decoders_.size(); i++) {
        decoders_[i]->PrintFinalStats();
        if (i != decoders_.size() - 1) {
            std::ofstream chan_out(config_.json_stats_name, std::ofstream::app);
            chan_out << "," << std::endl;
        }
    }
    json_out.open(config_.json_stats_name, std::ofstream::app);
    json_out << "}";

#ifdef THERMAL
    thermal_calc_.PrintFinalPT(clk_);
#endif  // THERMAL
}

void BaseDRAMSystem::ResetStats() {
    for (size_t i = 0; i < decoders_.size(); i++) {
        decoders_[i]->ResetStats();
    }
}

void BaseDRAMSystem::RegisterCallbacks(
    std::function<void(uint64_t)> read_callback,
    std::function<void(uint64_t)> write_callback) {
    // TODO this should be propagated to controllers
    read_callback_ = read_callback;
    write_callback_ = write_callback;
}

JedecDRAMSystem::JedecDRAMSystem(Config &config, const std::string &output_dir,
                                 std::function<void(uint64_t)> read_callback,
                                 std::function<void(uint64_t)> write_callback)
    : BaseDRAMSystem(config, output_dir, read_callback, write_callback) {
    if (config_.IsHMC()) {
        std::cerr << "Initialized a memory system with an HMC config file!"
                  << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    decoders_.reserve(config_.channels);
    printf("%u CHANNELS CREATED\n", config_.channels);
    for (auto i = 0; i < config_.channels; i++) {
#ifdef THERMAL
        std::cerr << "CXL-PNM system does not support termal mode!"
                  << std::endl;
        AbruptExit(__FILE__, __LINE__);
#else
        decoders_.push_back(new AddrDecoder(i, config_, timing_));
#endif  // THERMAL
    }
}

JedecDRAMSystem::~JedecDRAMSystem() {
    for (auto it = decoders_.begin(); it != decoders_.end(); it++) {
        delete (*it);
    }
}

bool JedecDRAMSystem::WillAcceptTransaction(uint64_t hex_addr, bool is_write,
                                            FenceMode fence_mode) const {
    int channel = GetChannel(hex_addr);
    return decoders_[channel]->WillAcceptTransaction(hex_addr,
                                                     is_write, fence_mode);
}

bool JedecDRAMSystem::AddTransaction(uint64_t hex_addr, bool is_write) {
    // not used 
    int channel = GetChannel(hex_addr);
    bool ok = decoders_[channel]->WillAcceptTransaction(hex_addr, is_write);

    // what if it is not ok???
    assert(ok);
    if (ok) {
        Transaction trans = Transaction(hex_addr, is_write);
        decoders_[channel]->AddTransaction(trans);
    }
    last_req_clk_ = clk_; 
    return ok;
}

bool JedecDRAMSystem::AddTransaction(uint64_t hex_addr, bool is_write,
                                     void* data, int data_size,
                                     FenceMode fence_mode) {
// Record trace - Record address trace for debugging or other purposes
#ifdef ADDR_TRACE
    address_trace_ << std::hex << hex_addr << std::dec << " "
                   << (is_write ? "WRITE " : "READ ") << clk_ << std::endl;
#endif

    int channel = GetChannel(hex_addr);
    bool ok = decoders_[channel]->WillAcceptTransaction(hex_addr, is_write);

    // what if it is not ok???
    assert(ok);
    if (ok) {
        Transaction trans = Transaction(hex_addr, is_write,
                                        data, data_size, fence_mode);
        decoders_[channel]->AddTransaction(trans);
    }
    last_req_clk_ = clk_; 
    return ok;
}

void JedecDRAMSystem::ClockTick() {
    if(clk_ % 1 == 0){
        for (size_t i = 0; i < decoders_.size(); i++) {
            // look ahead and return earlier
            while (true) {
                auto pair = decoders_[i]->ReturnDoneTrans(pnm_clk_);
                if (pair.second == 1) {
                    write_callback_(pair.first);
                } else if (pair.second == 0) {
                    read_callback_(pair.first);
                } else {
                    break;
                }
            }
        }
        pnm_clk_++;
    }

    for (size_t i = 0; i < decoders_.size(); i++) {
        decoders_[i]->ClockTick();
    }
    clk_++;

    if (clk_ % config_.epoch_period == 0) {
        PrintEpochStats();
    }
    return;
}

bool JedecDRAMSystem::IsAllQueueEmpty() {
    bool empty = true;
    for (size_t i = 0; i < decoders_.size(); i++)
        empty = empty && decoders_[i]->IsAllQueueEmpty();

    return empty;
}

IdealDRAMSystem::IdealDRAMSystem(Config &config, const std::string &output_dir,
                                 std::function<void(uint64_t)> read_callback,
                                 std::function<void(uint64_t)> write_callback)
    : BaseDRAMSystem(config, output_dir, read_callback, write_callback),
      latency_(config_.ideal_memory_latency) {}

IdealDRAMSystem::~IdealDRAMSystem() {}

bool IdealDRAMSystem::AddTransaction(uint64_t hex_addr, bool is_write) {
    auto trans = Transaction(hex_addr, is_write);
    trans.added_cycle = clk_;
    infinite_buffer_q_.push_back(trans);
    return true;
}

bool IdealDRAMSystem::AddTransaction(uint64_t hex_addr, bool is_write,
                                     void* data, int data_size,
                                     FenceMode fence_mode) {
    auto trans = Transaction(hex_addr, is_write, data, data_size, fence_mode);
    trans.added_cycle = clk_;
    infinite_buffer_q_.push_back(trans);
    return true;
}

void IdealDRAMSystem::ClockTick() {
    for (auto trans_it = infinite_buffer_q_.begin();
         trans_it != infinite_buffer_q_.end();) {
        if (clk_ - trans_it->added_cycle >= static_cast<uint64_t>(latency_)) {
            if (trans_it->is_write) {
                write_callback_(trans_it->addr);
            } else {
                read_callback_(trans_it->addr);
            }
            trans_it = infinite_buffer_q_.erase(trans_it++);
        }
        if (trans_it != infinite_buffer_q_.end()) {
            ++trans_it;
        }
    }

    clk_++;
    return;
}

bool IdealDRAMSystem::IsAllQueueEmpty() {
    return infinite_buffer_q_.empty();
}

}  // namespace dramsim3
