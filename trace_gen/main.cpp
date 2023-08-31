#include <iostream>

#include "config.h"
#include "common.h"
#include "instruction.h"

using namespace std;

void read_psum(int channel, int num_read, Config *config, int &time)
{
    // Check done
    config->cxlpnm_out <<
        "0x"
        << hex << Address(channel, 0, 0, 0, 0, 0, config).GetHexAddress()
        << " DONE "
        << dec << time << endl;

    time += config->default_interval;

    uint64_t psum_addr = Address(
            channel,
            true,
            PNM_PSUM_BUF_START,
            config).GetPNMAddress();

    for(int i = 0; i < num_read; i++) {
        config->cxlpnm_out
            << "0x"
            << hex << Address(channel, true , psum_addr, config).GetHexAddress()
            << " READ "
            << dec << time << endl;

        time += config->default_interval;
        psum_addr += config->data_size;
    }

    config->cxlpnm_out
        << "0x"
        << hex
        << Address(
                channel,
                true,
                PNM_CONFIG_REG_START + PNM_EXE_REG_OFFSET,
                config).GetHexAddress()
        << " MFENCE "
        << dec << time << endl;

    time += config->default_interval;
}

void write_base(Config *config, uint64_t addr, int &time)
{
    if ((rand() % 100) < config->miss_ratio) {
        config->base_out
            << "0x"
            << hex << addr
            << " READ "
            << dec << time << endl;

        time += config->default_interval;
    }
    else {
        time += 1;
    }
}

int main(int argc, const char* argv[])
{
    cout << " --- DRAMsim3 Trace Generator --- " << endl;

    Config *config = new Config(argc, argv);
    Address *addr = new Address();
    Instruction *inst = new Instruction(config);

    int num_trial = 0;
    int ax_time = 0, b_time = 0;
    if (config->opcode == 0) { // SLS
        while (num_trial < config->nepochs) {
            int p_idx = 0;
            int acum_b = 0;
            for (auto batch_s : config->batch_list) {
                p_idx = 0;
                for (auto ch: config->channel) {
                    if ((num_trial + p_idx) >= config->nepochs)
                        continue;

                    inst->init(
                            Address(
                                ch,
                                true,
                                PNM_INST_BUF_START,
                                config).GetPNMAddress(),
                            Address(
                                ch,
                                true,
                                PNM_CONFIG_REG_START
                                + PNM_EXE_REG_OFFSET,
                                config).GetHexAddress(),
                            batch_s * config->total_lookup);

                    int output_idx = 0;
                    for (unsigned b_id = 0; b_id < batch_s; b_id++) {
                        for (unsigned t_id = 0;
                                t_id < config->tables.size();
                                t_id++) {
                            for (unsigned ll = 0;
                                    ll < config->num_indices_per_lookup[t_id];
                                    ll++) {
                                addr->reset(
                                        ch,
                                        (t_id * config->accum_table_size[t_id]
                                         + config->indices[num_trial
                                         + p_idx][t_id][acum_b + b_id][ll])
                                        * config->data_size,
                                        config);
                                write_base(
                                        config,
                                        addr->GetHexAddress(),
                                        b_time);

                                inst->write_instruction(
                                        config->opcode,
                                        false,
                                        output_idx,
                                        addr,
                                        config->cxlpnm_out,
                                        ax_time);
                            }
                            output_idx++;
                        }
                    }
                    p_idx++;
                }

                p_idx = 0;
                for (auto ch: config->channel) {
                    if ((num_trial + p_idx) >= config->nepochs)
                        continue;

                    read_psum(
                            ch,
                            batch_s * config->tables.size(),
                            config, ax_time);

                    p_idx++;
                }

                acum_b += batch_s;
            }
            num_trial += p_idx;
        }
    }
    else{
        assert(false);
    }

    cout << " --- End ---" << endl;

    return 0;
}
