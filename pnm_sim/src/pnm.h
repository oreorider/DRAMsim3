#ifndef __PNM_H
#define __PNM_H

#include <unordered_map>
#include <iomanip>

#include "controller.h"
#include "configuration.h"

namespace dramsim3 {

enum class Opcode {SUM, SIZE};
enum class PNMRegister {INSTRUCTION, PSUM};
enum class DataType {INPUT, WEIGHT, SIZE};

class Instruction {
  public:
    Instruction(const Config *config_p_)
        : opcode(Opcode::SIZE),
          trace_end(false),
          psum_idx(-1),
          addr(Address()),
          hex_addr(-1),
          data(NULL),
          type(DataType::SIZE),
          complete_cycle(-1),
          input_idx(-1),
          cache_hit(false),
          data_size(0),
          batch_size(0),
          channel_addr(0),
          config_p(config_p_) {}
    Instruction(Opcode opcode, bool trace_end, int psum_idx,
                Address addr, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          addr(addr),
          hex_addr(addr.GetHexAddress()),
          type(DataType::SIZE),
          complete_cycle(-1),
          input_idx(-1),
          cache_hit(false),
          config_p(config_p_) {}
    Instruction(Opcode opcode, bool trace_end, int psum_idx, Address addr,
                DataType type, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          addr(addr),
          hex_addr(addr.GetHexAddress()),
          type(type),
          complete_cycle(-1),
          input_idx(-1),
          cache_hit(false),
          config_p(config_p_) {}
    Instruction(Opcode opcode, bool trace_end, int psum_idx, int input_idx,
                bool cache_hit, DataType type, uint32_t channel_addr,
                int channel_id, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          addr(Address(channel_id, 
                      (channel_addr << config_p_->shift_bits),
                      config_p_)),
          hex_addr(addr.GetHexAddress()),
          type(type),
          complete_cycle(-1),
          input_idx(input_idx),
          cache_hit(cache_hit),
          config_p(config_p_) {}

    Instruction(uint64_t inst_data, int channel_id, const Config *config_p_);

    Opcode    opcode;
    bool      trace_end;
    int       psum_idx;
    Address   addr;
    uint64_t  hex_addr;
    float*    data;
    DataType  type;
    uint64_t  complete_cycle;
    int       input_idx;
    bool      cache_hit;

    uint32_t       data_size; // (input or output size) / 8
    uint32_t       batch_size;
    uint32_t       channel_addr;

    const Config *config_p;

    Instruction& operator= (const Instruction &inst) {
        opcode           = inst.opcode;
        trace_end        = inst.trace_end;
        psum_idx         = inst.psum_idx;
        addr             = inst.addr;
        hex_addr         = inst.hex_addr;
        data             = inst.data;
        type             = inst.type;
        complete_cycle   = inst.complete_cycle;
        input_idx        = inst.input_idx;
        cache_hit        = inst.cache_hit;
        data_size        = inst.data_size;
        batch_size       = inst.batch_size;
        channel_addr     = inst.channel_addr;
        config_p         = inst.config_p;
        return *this;
    }
};

class AdderElement {
  public:
    AdderElement():psum_idx(-1), psum_data(NULL), dram_data(NULL) {}
    AdderElement(int idx, float *a, float *b)
            : psum_idx(idx), psum_data(a), dram_data(b) {}
    int psum_idx;
    float* psum_data;
    float* dram_data;
};

class PNM {
  public:
    PNM(int channel, const Config &config, const Timing &timing,
        Controller& ctrl);
    bool Done();
    void ClockTick();
    void Write(Transaction trans, uint64_t pnm_addr);
    float *Read(uint64_t hex_addr, uint64_t pnm_addr);
    std::pair<uint64_t, int> ReturnDoneTrans(uint64_t clock);

  private:
    int channel_id_;
    uint64_t clk_;
    const Config &config_;

    std::vector<Instruction> cache_q_;

    std::multimap<uint64_t, Instruction> requested_rd_q_;
    std::vector<Instruction> return_queue_;

    std::vector<AdderElement> adder_;

    std::vector<float*> input_cache_;
    std::map<uint64_t, float* > weight_cache_;

    // instruction buffer addr - instruction
    // map 64bit(8B) address to a instruction
    std::unordered_map<uint64_t, Instruction> inst_buf;
    // psum buffer
    // map 64Byte address to 16 x 4B data
    std::unordered_map<uint64_t, float*> psum;

    bool sls_exec;
    int program_count, num_write_inst, num_read_inst, num_accum;

    // offset choose instruction or psum
    uint64_t index_to_address(int idx, PNMRegister reg_mode);

    void WriteInstruction(Transaction Trans);
    void WriteConfigRegister(Transaction Trans);

    float *ReadConfigRegister(uint64_t hex_addr);
    float *ReadPsumData(uint64_t hex_addr);

    void ScheduleInstruction();
    Transaction InstructionToTransaction(const Instruction inst);

    void ReadCache();

    void ReturnDataReady();
    void ExecuteAdder();

    int inst_offset_idx, psum_offset_idx;
    std::vector<uint64_t> write_instruction_offset;
    std::vector<uint64_t> pc_offset;
    std::vector<uint64_t> psum_offset;
    std::vector<uint64_t> read_psum_offset;

#ifdef DEBUG_PRINT
    int num_input, num_weight;
#endif

   protected:
    Controller& ctrl_;
};

}  // namespace dramsim3
#endif
