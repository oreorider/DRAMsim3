#ifndef __ADDRDECODER_H
#define __ADDRDECODER_H

#include "common.h"
#include "configuration.h"
#include "timing.h"
#include "pnm.h"
#include "controller.h"

namespace dramsim3 {
#define PNM_PSUM_READ_LATENCY 14 // Placeholder

class AddrDecoder {
   public:
    AddrDecoder(int channel, const Config &config, const Timing &timing);
    ~AddrDecoder() { delete pnm_; delete ctrl_; }
    void ClockTick();
    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write,
                               FenceMode fence_mode = FenceMode::NONE) const;
    bool AddTransaction(Transaction trans);
    // Stats output
    void PrintEpochStats();
    void PrintFinalStats();
    void ResetStats();
    std::pair<uint64_t, int> ReturnDoneTrans(uint64_t clock);
    void ReturnReadData();
    bool IsAllQueueEmpty() const;

    Controller* ctrl_;
    PNM* pnm_;

   private:
    int channel_id_;
    uint64_t clk_;
    const Config &config_;

    std::vector<Transaction> trans_queue_;
    std::vector<Transaction> return_queue_;

#ifdef CMD_TRACE
    std::ofstream pnm_trace_;
#endif  // CMD_TRACE

    void SendTransaction();
    bool IsAllReadCommandDone() const;
    bool IsAllWriteCommandDone() const;
    bool IsPNMDone() const;
    bool IsAllTransSent() const;
    uint64_t HexToPNMAddress(uint64_t hex_addr, const Config *config_p_);
};
}  // namespace dramsim3
#endif
