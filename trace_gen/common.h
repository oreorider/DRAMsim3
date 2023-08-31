#ifndef __COMMON_H
#define __COMMON_H

#include "config.h"

struct Address {
    Address()
        : channel(-1),
          rank(-1),
          bankgroup(-1),
          bank(-1),
          row(-1),
          column(-1),
          channel_addr(-1),
          is_pnm(false),
          pnm_addr(-1),
          config_p(NULL) {}

    Address(int channel, int rank, int bankgroup, int bank, int row, int column,
            const Config *config_p)
        : channel(channel),
          rank(rank),
          bankgroup(bankgroup),
          bank(bank),
          row(row),
          column(column),
          is_pnm(false),
          pnm_addr(-1),
          config_p(config_p)
    {
        GetChannelAddress();
    }

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

    Address(int ch, uint64_t channel_addr_, const Config *config_p_)
    {
        reset(ch, channel_addr_, config_p_); 
    }

    Address(int ch, bool is_pnm_, uint64_t pnm_addr_, const Config *config_p_)
    {
        channel = ch;
        rank = -1;
        bankgroup = -1;
        bank = -1;
        row = -1;
        column = -1;
        channel_addr = -1;
        is_pnm = is_pnm_;
        pnm_addr = pnm_addr_;
        config_p = config_p_;
    }

    void reset(int ch, int ra, int bg, int ba, int ro, int col,
            const Config *config_p_)
    {
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

    void reset(int ch, uint64_t channel_addr_, const Config *config_p_)
    {
        const Config *cp = config_p_;
        channel = ch;
        config_p = cp;
        channel_addr = (channel_addr_) >> cp->shift_bits; 

        if (cp->ra_pos > cp->ch_pos) {
            rank =
                (channel_addr_ >> (cp->shift_bits + cp->ra_pos - cp->ch_bits))
                & cp->ra_mask;
        } else {
            rank =
                (channel_addr_ >> (cp->shift_bits + cp->ra_pos)) & cp->ra_mask;
        }

        if (cp->bg_pos > cp->ch_pos) {
            bankgroup =
                (channel_addr_ >> (cp->shift_bits + cp->bg_pos + cp->ch_bits))
                & cp->bg_mask;
        } else {
            bankgroup =
                (channel_addr_ >> (cp->shift_bits + cp->bg_pos)) & cp->bg_mask;
        }

        if (cp->ba_pos > cp->ch_pos) {
            bank =
                (channel_addr_ >> (cp->shift_bits + cp->ba_pos + cp->ch_bits))
                & cp->ba_mask;
        } else {
            bank =
                (channel_addr_ >> (cp->shift_bits + cp->ba_pos)) & cp->ba_mask;
        }

        if (cp->ro_pos > cp->ch_pos) {
            row =
                (channel_addr_ >> (cp->shift_bits + cp->ro_pos + cp->ch_bits))
                & cp->ro_mask;
        } else {
            row =
                (channel_addr_ >> (cp->shift_bits + cp->ro_pos)) & cp->ro_mask;
        }

        if (cp->co_pos > cp->ch_pos) {
            column =
                (channel_addr_ >> (cp->shift_bits + cp->co_pos + cp->ch_bits))
                & cp->co_mask;
        } else {
            column =
                (channel_addr_ >> (cp->shift_bits + cp->co_pos)) & cp->co_mask;
        }
    }

    uint64_t GetHexAddress()
    {
        const Config *cp = config_p;
        uint64_t addr = 0;

        if (is_pnm == false) {
            addr += (uint64_t)channel << (cp->shift_bits + cp->ch_pos);
            addr += (uint64_t)rank << (cp->shift_bits + cp->ra_pos);
            addr += (uint64_t)bankgroup << (cp->shift_bits + cp->bg_pos);
            addr += (uint64_t)bank << (cp->shift_bits + cp->ba_pos);
            addr += (uint64_t)row << (cp->shift_bits + cp->ro_pos);
            addr += (uint64_t)column << (cp->shift_bits + cp->co_pos);
        } else {
            int low_bits = cp->shift_bits + cp->ch_pos + cp->ch_bits;
            uint64_t low_eraser = ~(((uint64_t)1 << (low_bits)) - 1);
            addr += (pnm_addr << cp->ch_bits) & low_eraser;
            addr += (uint64_t)channel << (cp->shift_bits + cp->ch_pos);
            addr += pnm_addr & (((uint64_t)1 << (cp->shift_bits + cp->ch_pos)) - 1);
        }

        return addr; 
    }

    uint64_t GetChannelAddress()
    {
        const Config *cp = config_p;
        uint64_t addr = 0;

        if (cp->ra_pos > cp->ch_pos) {
            addr +=
                (uint64_t)rank << (cp->shift_bits + cp->ra_pos - cp->ch_bits);
        } else {
            addr += (uint64_t)rank << (cp->shift_bits + cp->ra_pos);
        }

        if (cp->bg_pos > cp->ch_pos) {
            addr +=
                (uint64_t)bankgroup
                << (cp->shift_bits + cp->bg_pos - cp->ch_bits);
        } else {
            addr += (uint64_t)bankgroup << (cp->shift_bits + cp->bg_pos);
        }

        if (cp->ba_pos > cp->ch_pos) {
            addr +=
                (uint64_t)bank << (cp->shift_bits + cp->ba_pos - cp->ch_bits);
        } else {
            addr += (uint64_t)bank << (cp->shift_bits + cp->ba_pos);
        }

        if (cp->ro_pos > cp->ch_pos) {
            addr +=
                (uint64_t)row << (cp->shift_bits + cp->ro_pos - cp->ch_bits);
        } else {
            addr += (uint64_t)row << (cp->shift_bits + cp->ro_pos);
        }

        if (cp->co_pos > cp->ch_pos) {
            addr +=
                (uint64_t)column << (cp->shift_bits + cp->co_pos - cp->ch_bits);
        } else {
            addr += (uint64_t)column << (cp->shift_bits + cp->co_pos);
        }

        channel_addr = addr >> cp->shift_bits;

        return addr;
    }

    uint64_t GetPNMAddress()
    {
        return pnm_addr;
    }

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

#endif
