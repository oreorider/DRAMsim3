#include "common.h"
#include "fmt/format.h"
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <sys/stat.h>

namespace dramsim3 {

void Address::reset(int ch, int ra, int bg, int ba, int ro, int col,
                    const Config *config_p_){
    channel = ch;
    rank = ra;
    bankgroup = bg;
    bank = ba;
    row = ro;
    column = col;
    is_pnm = false;
    pnm_addr = -1;
    config_p = config_p_;
}

void Address::reset(int ch, uint64_t channel_addr_, const Config *config_p_) {

    channel = ch;
    config_p = config_p_;
    channel_addr = (channel_addr_) >> config_p->shift_bits; 
    is_pnm = false;

    if (config_p->ra_pos > config_p->ch_pos) {
        rank = 
            (channel_addr_
             >> (config_p->shift_bits + config_p->ra_pos - config_p->ch_bits))
            & config_p->ra_mask;
    } else {
        rank =
            (channel_addr_ >> (config_p->shift_bits + config_p->ra_pos))
            & config_p->ra_mask;
    }

    if (config_p->bg_pos > config_p->ch_pos) {
        bankgroup =
            (channel_addr_ 
             >> (config_p->shift_bits + config_p->bg_pos + config_p->ch_bits))
            & config_p->bg_mask;
    } else {
        bankgroup =
            (channel_addr_ >> (config_p->shift_bits + config_p->bg_pos))
            & config_p->bg_mask;
    }

    if (config_p->ba_pos > config_p->ch_pos) {
        bank =
            (channel_addr_ >>
             (config_p->shift_bits + config_p->ba_pos + config_p->ch_bits))
            & config_p->ba_mask;
    } else {
        bank =
            (channel_addr_ >> (config_p->shift_bits + config_p->ba_pos))
            & config_p->ba_mask;
    }

    if (config_p->ro_pos > config_p->ch_pos) {
        row =
            (channel_addr_
             >> (config_p->shift_bits + config_p->ro_pos + config_p->ch_bits))
            & config_p->ro_mask;
    } else {
        row =
            (channel_addr_ >> (config_p->shift_bits + config_p->ro_pos))
            & config_p->ro_mask;
    }

    if (config_p->co_pos > config_p->ch_pos) {
        column =
            (channel_addr_
             >> (config_p->shift_bits + config_p->co_pos + config_p->ch_bits))
            & config_p->co_mask;
    } else {
        column =
            (channel_addr_
             >> (config_p->shift_bits + config_p->co_pos)) & config_p->co_mask;
    }
}

uint64_t Address::GetHexAddress() {
    uint64_t addr = 0;

    if (is_pnm == false) {
        addr += (uint64_t)channel << (config_p->shift_bits + config_p->ch_pos);
        addr += (uint64_t)rank << (config_p->shift_bits + config_p->ra_pos);
        addr += (uint64_t)bankgroup << (config_p->shift_bits + config_p->bg_pos);
        addr += (uint64_t)bank << (config_p->shift_bits + config_p->ba_pos);
        addr += (uint64_t)row << (config_p->shift_bits + config_p->ro_pos);
        addr += (uint64_t)column << (config_p->shift_bits + config_p->co_pos);
    } else {
        uint64_t low_eraser =
            ~(((uint64_t)1 
               << (config_p->shift_bits + config_p->ch_pos + config_p->ch_bits))
             - 1);
        addr += (pnm_addr << config_p->ch_bits) & low_eraser;
        addr += (uint64_t)channel << (config_p->shift_bits + config_p->ch_pos);
        addr += pnm_addr & 
            (((uint64_t)1 << (config_p->shift_bits + config_p->ch_pos)) - 1);
    }

    return addr; 
}

uint64_t Address::GetChannelAddress() {
    uint64_t addr = 0;

    if (config_p->ra_pos > config_p->ch_pos) {
        addr += (uint64_t)rank 
               << (config_p->shift_bits + config_p->ra_pos - config_p->ch_bits);
    } else {
        addr += (uint64_t)rank << (config_p->shift_bits + config_p->ra_pos);
    }

    if (config_p->bg_pos > config_p->ch_pos) {
        addr += (uint64_t)bankgroup
               << (config_p->shift_bits + config_p->bg_pos - config_p->ch_bits);
    } else {
        addr += (uint64_t)bankgroup
               << (config_p->shift_bits + config_p->bg_pos);
    }

    if (config_p->ba_pos > config_p->ch_pos) {
        addr += (uint64_t)bank
               << (config_p->shift_bits + config_p->ba_pos - config_p->ch_bits);
    } else {
        addr += (uint64_t)bank << (config_p->shift_bits + config_p->ba_pos);
    }

    if (config_p->ro_pos > config_p->ch_pos) {
        addr += (uint64_t)row
               << (config_p->shift_bits + config_p->ro_pos - config_p->ch_bits);
    } else {
        addr += (uint64_t)row << (config_p->shift_bits + config_p->ro_pos);
    }

    if (config_p->co_pos > config_p->ch_pos) {
        addr += (uint64_t)column
               << (config_p->shift_bits + config_p->co_pos - config_p->ch_bits);
    } else {
        addr += (uint64_t)column << (config_p->shift_bits + config_p->co_pos);
    }

    channel_addr = addr >> config_p->shift_bits;

    return addr;
}

uint64_t Address::GetPNMAddress() {
    return pnm_addr;
}

std::ostream& operator<<(std::ostream& os, const Command& cmd) {
    std::vector<std::string> command_string = {
        "read",
        "read_p",
        "write",
        "write_p",
        "activate",
        "precharge",
        "refresh_bank",  // verilog model doesn't distinguish bank/rank refresh
        "refresh",
        "self_refresh_enter",
        "self_refresh_exit",
        "WRONG"};
    os << fmt::format("{:<20} {:>3} {:>3} {:>3} {:>3} {:>#8x} {:>#8x}",
                      command_string[static_cast<int>(cmd.cmd_type)],
                      cmd.Channel(), cmd.Rank(), cmd.Bankgroup(), cmd.Bank(),
                      cmd.Row(), cmd.Column());
    return os;
}

std::ostream& operator<<(std::ostream& os, const Transaction& trans) {
    const std::string trans_type = trans.is_write ? "WRITE" : "READ";
    os << fmt::format("{:<20} {:>#8x}", trans_type, trans.addr);
    return os;
}

std::istream& operator>>(std::istream& is, Transaction& trans) {
    std::unordered_map<std::string, FenceMode> 
        fence_types = {{"MFENCE", FenceMode::MFENCE},
                       {"mfence", FenceMode::MFENCE},
                       {"LFENCE", FenceMode::LFENCE},
                       {"lfence", FenceMode::LFENCE},
                       {"SFENCE", FenceMode::SFENCE},
                       {"sfence", FenceMode::SFENCE},
                       {"DONE", FenceMode::DONE},
                       {"done", FenceMode::DONE}};
    std::unordered_set<std::string> write_types = {"WRITE", "write", "P_MEM_WR",
                                                   "SFENCE", "sfence"
                                                   "BOFF"};
    std::string mem_op;

    std::string line;
    size_t pos, end;
    getline(is, line);
    if (is.eof()){
        trans.added_cycle = -1;
        return is;
    }

    trans.addr = std::stoul(line, &pos, 16);

    pos = line.find_first_not_of(' ', pos+1);
    end = line.find(' ', pos+1);
    mem_op = line.substr(pos, end-pos);
    trans.is_write = write_types.count(mem_op) == 1;
    auto it_f = fence_types.find(mem_op);
    trans.fence_mode = (it_f == fence_types.end()) ?
                       FenceMode::NONE : it_f->second;

    pos = line.find_first_not_of(' ', end+1);
    trans.added_cycle = std::stoul(line.substr(pos), &end, 10);

    if (trans.is_write) {
        pos = line.find_first_not_of(' ', pos+end);
        std::vector<uint64_t> tmp;
        while (pos != std::string::npos) {
            tmp.push_back(std::stoul(line.substr(pos), &end, 16));
            pos = line.find_first_not_of(' ', pos+end);
        }
        trans.data_size = tmp.size();

        uint64_t *t_tmp = new uint64_t [trans.data_size];
        for (unsigned i = 0; i < tmp.size(); i++){
            t_tmp[i] = tmp[i];
        }
        trans.data = (void *) t_tmp;
    }

    return is;
}

int GetBitInPos(uint64_t bits, int pos) {
    // given a uint64_t value get the binary value of pos-th bit
    // from MSB to LSB indexed as 63 - 0
    return (bits >> pos) & 1;
}

int LogBase2(int power_of_two) {
    int i = 0;
    while (power_of_two > 1) {
        power_of_two /= 2;
        i++;
    }
    return i;
}

std::vector<std::string> StringSplit(const std::string& s, char delim) {
    std::vector<std::string> elems;
    StringSplit(s, delim, std::back_inserter(elems));
    return elems;
}

template <typename Out>
void StringSplit(const std::string& s, char delim, Out result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            *(result++) = item;
        }
    }
}

void AbruptExit(const std::string& file, int line) {
    std::cerr << "Exiting Abruptly - " << file << ":" << line << std::endl;
    std::exit(-1);
}

bool DirExist(std::string dir) {
    // courtesy to stackoverflow
    struct stat info;
    if (stat(dir.c_str(), &info) != 0) {
        return false;
    } else if (info.st_mode & S_IFDIR) {
        return true;
    } else {  // exists but is file
        return false;
    }
}

}  // namespace dramsim3
