#ifndef __COMMON_H
#define __COMMON_H

#include <stdint.h>
#include <iostream>
#include <vector>

#include "configuration.h"
#define INST_BUFFER_BYTE_SIZE (1024 * 1024 * 20)
#define PNM_INST_BUF_START 0
#define PNM_INST_BUF_END (PNM_INST_BUF_START + INST_BUFFER_BYTE_SIZE - 1)

#define CONFIG_REG_SIZE 1024
#define PNM_CONFIG_REG_START (PNM_INST_BUF_END + 1 + 1024)
#define PNM_CONFIG_REG_END (PNM_CONFIG_REG_START + CONFIG_REG_SIZE - 1)
#define PNM_EXE_REG_OFFSET 0
#define PNM_STATUS_REG_OFFSET 64

#define PSUM_BUFFER_BYTE_SIZE (256 * 1024)
#define PNM_PSUM_BUF_START (PNM_CONFIG_REG_END + 1 + 1024)
#define PNM_PSUM_BUF_END (PNM_PSUM_BUF_START + PSUM_BUFFER_BYTE_SIZE - 1)

//define address space (probably will need to change later)
#define DENSEMM_BUFFER_BYTE_SIZE (10*1024*1024) //5MB buffer
#define PNM_DENSEMM_BUF_START   (PNM_PSUM_BUF_END + 1 + 1024)
#define PNM_DENSEMM_BUF_END     (PNM_DENSEMM_BUF_START + DENSEMM_BUFFER_BYTE_SIZE - 1)

#define SPARSEMM_BUFFER_BYTE_SIZE   (256*1024)
#define PNM_SPARSEMM_BUF_START      (PNM_DENSEMM_BUF_END + 1 + 1024)
#define PNM_SPARSEMM_BUF_END        (PNM_SPARSEMM_BUF_START + SPARSEMM_BUFFER_BYTE_SIZE - 1)

//for 128x128 sys array
#define DATA_CACHE_BYTE_SIZE        (256*256*16)//1MB cache

//caches just large enough to hold 128x256 32fp numbers
#define ACT_INPUT_BUF_BYTE_SIZE     (128*256*4)//16kB cache
#define WGT_INPUT_BUF_BYTE_SIZE     (128*256*4)//16kB cache

namespace dramsim3 {

enum class FenceMode {NONE, MFENCE, LFENCE, SFENCE, DONE};

class Config;

struct Address {
    Address()
        : channel(-1), rank(-1), bankgroup(-1), bank(-1), row(-1), column(-1),
          channel_addr(-1), is_pnm(false), pnm_addr(-1), config_p(NULL) {}

    Address(int channel, int rank, int bankgroup, int bank,
            int row, int column, const Config *config_p)
        : channel(channel),
          rank(rank),
          bankgroup(bankgroup),
          bank(bank),
          row(row),
          column(column),
          is_pnm(false),
          pnm_addr(-1),
          config_p(config_p) { GetChannelAddress();}

    Address(const Address& addr)
        : channel(addr.channel),
          rank(addr.rank),
          bankgroup(addr.bankgroup),
          bank(addr.bank),
          row(addr.row),
          column(addr.column),
          channel_addr(addr.channel_addr),
          is_pnm(addr.is_pnm),
          pnm_addr(addr.pnm_addr),
          config_p(addr.config_p) {}

    Address(int ch, uint64_t channel_addr_, const Config *config_p_) {
        reset(ch, channel_addr_, config_p_); 
    }

    Address(int ch, bool is_pnm_, uint64_t addr_, const Config *config_p_) {
        channel = ch;
        rank = -1;
        bankgroup = -1;
        bank = -1;
        row = -1;
        column = -1;
        channel_addr = -1;
        is_pnm = is_pnm_;
        pnm_addr = addr_;
        config_p = config_p_;
    }

    void reset(int ch, int ra, int bg, int ba, int ro, int col,
               const Config *config_p_);
    void reset(int ch, uint64_t channel_addr_, const Config *config_p_);
    uint64_t GetHexAddress();
    uint64_t GetChannelAddress();
    uint64_t GetPNMAddress();

    int channel;
    int rank;
    int bankgroup;
    int bank;
    int row;
    int column;

    uint64_t channel_addr;
    bool is_pnm;
    uint64_t pnm_addr;

    const Config *config_p;
};

inline uint32_t ModuloWidth(uint64_t addr, uint32_t bit_width, uint32_t pos) {
    addr >>= pos;
    auto store = addr;
    addr >>= bit_width;
    addr <<= bit_width;
    return static_cast<uint32_t>(store ^ addr);
}

// extern std::function<Address(uint64_t)> AddressMapping;
int GetBitInPos(uint64_t bits, int pos);
// it's 2017 and c++ std::string still lacks a split function, oh well
std::vector<std::string> StringSplit(const std::string& s, char delim);
template <typename Out>
void StringSplit(const std::string& s, char delim, Out result);

int LogBase2(int power_of_two);
void AbruptExit(const std::string& file, int line);
bool DirExist(std::string dir);

enum class CommandType {
    READ,
    READ_PRECHARGE,
    WRITE,
    WRITE_PRECHARGE,
    ACTIVATE,
    PRECHARGE,
    REFRESH_BANK,
    REFRESH,
    SREF_ENTER,
    SREF_EXIT,
    SIZE
};

struct Command {
    Command() : cmd_type(CommandType::SIZE), hex_addr(0) {}
    Command(CommandType cmd_type, const Address& addr, uint64_t hex_addr)
        : cmd_type(cmd_type), addr(addr), hex_addr(hex_addr) {}

    bool IsValid() const { return cmd_type != CommandType::SIZE; }
    bool IsRefresh() const {
        return cmd_type == CommandType::REFRESH ||
               cmd_type == CommandType::REFRESH_BANK;
    }
    bool IsRead() const {
        return cmd_type == CommandType::READ ||
               cmd_type == CommandType ::READ_PRECHARGE;
    }
    bool IsWrite() const {
        return cmd_type == CommandType ::WRITE ||
               cmd_type == CommandType ::WRITE_PRECHARGE;
    }
    bool IsReadWrite() const { return IsRead() || IsWrite(); }
    bool IsRankCMD() const {
        return cmd_type == CommandType::REFRESH ||
               cmd_type == CommandType::SREF_ENTER ||
               cmd_type == CommandType::SREF_EXIT;
    }
    CommandType cmd_type;
    Address addr;
    uint64_t hex_addr;

    int Channel() const { return addr.channel; }
    int Rank() const { return addr.rank; }
    int Bankgroup() const { return addr.bankgroup; }
    int Bank() const { return addr.bank; }
    int Row() const { return addr.row; }
    int Column() const { return addr.column; }

    friend std::ostream& operator<<(std::ostream& os, const Command& cmd);
};

struct Transaction {
    Transaction() {}
    Transaction(uint64_t addr, bool is_write)
        : addr(addr),
          added_cycle(0),
          complete_cycle(0),
          is_write(is_write),
          data(NULL),
          data_size(0),
          fence_mode(FenceMode::NONE) {}
    Transaction(const Transaction& tran)
        : addr(tran.addr),
          added_cycle(tran.added_cycle),
          complete_cycle(tran.complete_cycle),
          is_write(tran.is_write),
          data(tran.data),
          data_size(tran.data_size),
          fence_mode(tran.fence_mode)  {}
    Transaction(uint64_t addr, bool is_write, void *data, int data_size,
                FenceMode fence_mode = FenceMode::NONE)
        : addr(addr),
          added_cycle(0),
          complete_cycle(0),
          is_write(is_write),
          data(data),
          data_size(data_size),
          fence_mode(fence_mode) {}
    bool IsPNM(uint64_t hex_addr) const {
        if ((hex_addr >= PNM_INST_BUF_START) && (hex_addr <= PNM_SPARSEMM_BUF_END))
            return true;
        return false;
    }
    uint64_t addr;
    uint64_t added_cycle;
    uint64_t complete_cycle;
    bool is_write;
    void *data;
    int data_size;
    FenceMode fence_mode;

    friend std::ostream& operator<<(std::ostream& os, const Transaction& trans);
    friend std::istream& operator>>(std::istream& is, Transaction& trans);
};

}  // namespace dramsim3
#endif
