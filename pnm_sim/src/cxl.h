#ifndef __CXL_H
#define __CXL_H

#include <fstream>
#include <functional>
#include <random>
#include <string>
#include "memory_system.h"

namespace dramsim3 {

#define CXL_LATENCY 7 // Placeholder

class CXL {
   public:
    CXL(const std::string& config_file, const std::string& output_dir)
        : memory_system_(
              config_file, output_dir,
              std::bind(&CXL::ReadCallBack, this, std::placeholders::_1),
              std::bind(&CXL::WriteCallBack, this, std::placeholders::_1)),
          clk_(0) {}
    virtual bool ClockTick() = 0; // return done
    void ReadCallBack(uint64_t addr) { return; }
    void WriteCallBack(uint64_t addr) { return; }
    void PrintStats() { memory_system_.PrintStats(); }

   protected:
    MemorySystem memory_system_;
    uint64_t clk_;
};

class RandomCXL : public CXL {
   public:
    using CXL::CXL;
    bool ClockTick() override;

   private:
    uint64_t last_addr_;
    bool last_write_ = false;
    std::mt19937_64 gen;
    bool get_next_ = true;
};

class StreamCXL : public CXL {
   public:
    using CXL::CXL;
    bool ClockTick() override;

   private:
    uint64_t addr_a_, addr_b_, addr_c_, offset_ = 0;
    std::mt19937_64 gen;
    bool inserted_a_ = false;
    bool inserted_b_ = false;
    bool inserted_c_ = false;
    const uint64_t array_size_ = 2 << 20;  // elements in array
    const int stride_ = 64;                // stride in bytes
};

class TraceBasedCXL : public CXL {
   public:
    TraceBasedCXL(const std::string& config_file, const std::string& output_dir,
                  const std::string& trace_file);
    ~TraceBasedCXL() { trace_file_.close(); }
    bool ClockTick() override;

   private:
    std::ifstream trace_file_;
    Transaction trans_;
    bool get_next_ = true;
};

}  // namespace dramsim3
#endif
