#ifndef __PNM_H
#define __PNM_H

#include <unordered_map>
#include <iomanip>

#include "controller.h"
#include "configuration.h"

namespace dramsim3 {

enum class Opcode {SUM, DENSE, SPARSE, ACTIVATION, SIZE, DUMMY};
enum class PNMRegister {INSTRUCTION, PSUM, DENSEMM, SPARSEMM};
enum class DataType {INPUT, WEIGHT, SIZE};

class Instruction {
  public:
    Instruction(const Config *config_p_)
        : opcode(Opcode::SIZE),
          trace_end(false),
          psum_idx(-1),
          densemm_idx(-1),
          sparsemm_idx(-1),
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
    //original constructor
    Instruction(Opcode opcode, bool trace_end, int psum_idx,
                Address addr, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          densemm_idx(-1),
          sparsemm_idx(-1),
          addr(addr),
          hex_addr(addr.GetHexAddress()),
          type(DataType::SIZE),
          complete_cycle(-1),
          input_idx(-1),
          cache_hit(false),
          config_p(config_p_) {}
    //with densemm/sparsemm_idx
    Instruction(Opcode opcode, bool trace_end, int psum_idx, int densemm_idx, int sparsemm_idx,
                Address addr, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          densemm_idx(densemm_idx),
          sparsemm_idx(sparsemm_idx),
          addr(addr),
          hex_addr(addr.GetHexAddress()),
          type(DataType::SIZE),
          complete_cycle(-1),
          input_idx(-1),
          cache_hit(false),
          config_p(config_p_) {}
    //original constructor
    Instruction(Opcode opcode, bool trace_end, int psum_idx, Address addr,
                DataType type, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          densemm_idx(-1),
          sparsemm_idx(-1),
          addr(addr),
          hex_addr(addr.GetHexAddress()),
          type(type),
          complete_cycle(-1),
          input_idx(-1),
          cache_hit(false),
          config_p(config_p_) {}
    //with densemm/sparsemm_idx
    Instruction(Opcode opcode, bool trace_end, int psum_idx, int densemm_idx, int sparsemm_idx,
                Address addr, DataType type, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          densemm_idx(densemm_idx),
          sparsemm_idx(sparsemm_idx),
          addr(addr),
          hex_addr(addr.GetHexAddress()),
          type(type),
          complete_cycle(-1),
          input_idx(-1),
          cache_hit(false),
          config_p(config_p_) {}
    //original
    Instruction(Opcode opcode, bool trace_end, int psum_idx, int input_idx,
                bool cache_hit, DataType type, uint32_t channel_addr,
                int channel_id, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          densemm_idx(-1),
          sparsemm_idx(-1),
          addr(Address(channel_id, 
                      (channel_addr << config_p_->shift_bits),
                      config_p_)),
          hex_addr(addr.GetHexAddress()),
          type(type),
          complete_cycle(-1),
          input_idx(input_idx),
          cache_hit(cache_hit),
          config_p(config_p_) {}
    //with densemm/sparsemm_idx
    Instruction(Opcode opcode, bool trace_end, int psum_idx, int densemm_idx, int sparsemm_idx, int input_idx,
                bool cache_hit, DataType type, uint32_t channel_addr,
                int channel_id, const Config *config_p_)
        : opcode(opcode),
          trace_end(trace_end),
          psum_idx(psum_idx),
          densemm_idx(densemm_idx),
          sparsemm_idx(sparsemm_idx),
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
    int       densemm_idx;
    int       sparsemm_idx;
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

    //assignment operator
    Instruction& operator= (const Instruction &inst) {
        opcode           = inst.opcode;
        trace_end        = inst.trace_end;
        psum_idx         = inst.psum_idx;
        densemm_idx      = inst.densemm_idx;
        sparsemm_idx     = inst.sparsemm_idx;
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
    AdderElement():psum_idx(-1), psum_data(NULL), dram_data(NULL) {}//default
    AdderElement(int idx, float *a, float *b)
            : psum_idx(idx), psum_data(a), dram_data(b) {}
    int psum_idx;
    float* psum_data;
    float* dram_data;
};

class DenseMatmulElement{
  public:
    DenseMatmulElement():densemm_idx(-1), densemm_act(NULL), densemm_wgt(NULL){}
    DenseMatmulElement(int idx, float* a, float* b): 
      densemm_idx(idx), densemm_act(a), densemm_wgt(b) {}

    int densemm_idx;
    float* densemm_act;
    float* densemm_wgt;
};

class SparseMatmulElement{
  public:
    SparseMatmulElement():sparsemm_idx(-1), sparsemm_act(NULL), sparsemm_wgt(NULL){}
    SparseMatmulElement(int idx, float* a, float* b):
      sparsemm_idx(idx), sparsemm_act(a), sparsemm_wgt(b){}

    int sparsemm_idx;
    float* sparsemm_act;
    float* sparsemm_wgt;
};

class DeltaActMatmulElement{
  public: 
    DeltaActMatmulElement(): sparsemm_idx(-1), sparsemm_act(NULL), sparsemm_wgt(NULL){}
    DeltaActMatmulElement(int idx, float* a, float* b):
      sparsemm_idx(idx), sparsemm_act(a), sparsemm_wgt(b){}
    
    int sparsemm_idx;
    float* sparsemm_act;
    float* sparsemm_wgt;
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
    double getHardwareUtil();

  private:
    int channel_id_;
    uint64_t clk_;
    uint64_t hardware_clk_;
    const Config &config_;

    std::vector<Instruction> cache_q_;

    std::multimap<uint64_t, Instruction> requested_rd_q_;
    std::vector<Instruction> return_queue_;
    std::vector<Instruction> act_return_queue_;

    std::vector<AdderElement> adder_;
    std::vector<DenseMatmulElement> dense_;
    std::vector<SparseMatmulElement> sparse_;
    std::vector<DeltaActMatmulElement> delta_act_;

    std::vector<float*> input_cache_;

    //ordered map
    std::unordered_map<uint64_t, float* > data_cache_;
    //keeps track of data cache access times
    std::map<uint64_t, int> cache_access_time;

    // instruction buffer addr - instruction
    // map 64bit(8B) address to a instruction
    std::unordered_map<uint64_t, Instruction> inst_buf;
    // psum buffer
    // map 64Byte address to 16 x 4B data
    std::unordered_map<uint64_t, float*> psum;

    //densemm buffer, stores matmul output
    // <addr, data_value>
    std::unordered_map<uint64_t, float*> densemm_buf;
    std::vector<Instruction> densemm_q;

    //buffers to sys array
    //std::vector<float> sys_arr_input_act_buf;
    //std::vector<float> sys_arr_input_wgt_buf;
    float* sys_arr_input_act_buf;
    float* sys_arr_input_wgt_buf;
    int sys_arr_input_act_idx;
    int sys_arr_input_wgt_idx;

    float* block_sp_input_act_buf;
    float* block_sp_input_wgt_buf;
    int block_sp_input_act_idx;
    int block_sp_input_wgt_idx;

    float* spgemm_input_act_buf;
    float* spgemm_input_wgt_buf;
    int spgemm_input_act_idx;
    int spgemm_input_wgt_idx;

    //number of inputs to block sparse arrays that feed into systollic array
    int num_block_sp_input;
    //status of blocksparse systolic arrays (kernels)
    //0:nothing       1:busy        2:double buffer full
    std::vector<int> blocksp_kernel_status;
    int num_blocksp_kernels;
    int num_spgemm_kernels;
    int selected_kernel_buffer;
    //keep track of all sparse systolic array end clocks
    std::vector<int> sparse_kern_end_clk;
    std::vector<int> sparse_kern_busy_cnt;

    //control signals for DIFFPRUNE
    int activation_sparse;
    //number instructions required to be in buffer for sparse_ element to be made
    int req_num_inst_act_buf;
    int req_num_inst_wgt_buf;
    int req_num_delta_act;
    int kern_num_sp_elements;

    //sparsemm buffer
    // <addr, data_value>
    std::unordered_map<uint64_t, float*> sparsemm_buf;

    bool sls_exec;
    int program_count, num_write_inst, num_read_inst, num_accum;
    int add_to_buf_cnt;

    //sys array status signals for densemm
    int sys_array_start_clk;
    int sys_array_end_clk;

    //delta act kernel status signals
    int delta_act_kern_start_clk;
    int delta_act_kern_end_clk;


    int start_clk;
    int end_clk;
    int hardware_busy_clk_cnt;
    int cycles_stalled;


    //used to show which iteration of matmul we are on
    //from 0 ~ 15
    int sys_array_busy;
    int delta_act_kern_busy;
    int num_densemm, num_sparsemm;

    // offset choose instruction or psum
    uint64_t index_to_address(int idx, PNMRegister reg_mode);

    void WriteInstruction(Transaction Trans);
    void WriteConfigRegister(Transaction Trans);

    float *ReadConfigRegister(uint64_t hex_addr);
    float *ReadPsumData(uint64_t hex_addr);
    float *ReadDensemmData(uint64_t hex_addr);
    float *ReadSparsemmData(uint64_t hex_addr);

    void PrintUtilStats(std::vector<double> utils);

    void ScheduleInstruction();
    Transaction InstructionToTransaction(const Instruction inst);

    void ReadCache();

    bool ReturnDataReady();
    void ExecuteAdder();
    void ExecuteDenseMatmul();
    void ExecuteSparseMatmul();
    //finegrain sparse matmul
    void ExecuteDeltaActMatMul();

    int inst_offset_idx, psum_offset_idx, densemm_offset_idx, sparsemm_offset_idx;
    std::vector<uint64_t> write_instruction_offset;
    std::vector<uint64_t> pc_offset;
    std::vector<uint64_t> psum_offset;
    std::vector<uint64_t> densemm_offset;
    std::vector<uint64_t> sparsemm_offset;
    std::vector<uint64_t> read_psum_offset;
    std::vector<uint64_t> read_densemm_offset;
    std::vector<uint64_t> read_sparsemm_offset;

#ifdef DEBUG_PRINT
    int num_input, num_weight;
#endif

   protected:
    Controller& ctrl_;
};

}  // namespace dramsim3
#endif
