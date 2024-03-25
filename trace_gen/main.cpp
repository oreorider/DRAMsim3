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

    if(config->opcode == 0){
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
    }

    if(config->opcode == 1){
        uint64_t densemm_addr = Address(
                channel,
                true,
                PNM_DENSEMM_BUF_START,
                config).GetPNMAddress();

        for(int i = 0; i < num_read; i++) {
            config->cxlpnm_out
                << "0x"
                << hex << Address(channel, true , densemm_addr, config).GetHexAddress()
                << " READ "
                << dec << time << endl;

            time += config->default_interval;
            densemm_addr += config->data_size;
        }
    }
    
    if(config->opcode == 2){
        uint64_t sparsemm_addr = Address(
                channel,
                true,
                PNM_SPARSEMM_BUF_START,
                config).GetPNMAddress();

        for(int i = 0; i < num_read; i++) {
        config->cxlpnm_out
            << "0x"
            << hex << Address(channel, true , sparsemm_addr, config).GetHexAddress()
            << " READ "
            << dec << time << endl;

        time += config->default_interval;
        sparsemm_addr += config->data_size;
        }
    }

    if(config->opcode == 3){
        uint64_t sparsemm_addr = Address(
                channel,
                true,
                PNM_SPARSEMM_BUF_START,
                config).GetPNMAddress();

        for(int i = 0; i < num_read; i++) {
        config->cxlpnm_out
            << "0x"
            << hex << Address(channel, true , sparsemm_addr, config).GetHexAddress()
            << " READ "
            << dec << time << endl;

        time += config->default_interval;
        sparsemm_addr += config->data_size;
        }
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
        //printf("0x%lx READ %d\n", addr, time);
        time += config->default_interval;
        
    }
    else {
        time += 1;
    }
}

int main(int argc, const char* argv[])
{
    cout << " --- DRAMsim3 Trace Generator --- " << endl;
    for(int i = 1; i < argc; i++){
        printf("%s\n", argv[i]);
    }
    Config *config = new Config(argc, argv);
    printf("checkpoint1\n");
    Address *addr = new Address();
    printf("checkpoint2\n");
    Instruction *inst = new Instruction(config);
    printf("checkpoint3\n");

    //return 0;

    int num_trial = 0;
    int ax_time = 0, b_time = 0;
    if (config->opcode == 0) { // SLS
        printf("SLS instructions\n");
        while (num_trial < config->nepochs) {
            printf("\nnum_trial: %u\n", num_trial);
            int p_idx = 0;
            int acum_b = 0;
            for (auto batch_s : config->batch_list) {
                printf("batch_s: %u\n", batch_s);
                p_idx = 0;
                for (auto ch: config->channel) {
                    printf("channel: %u\n", ch);
                    printf("num_trial: %u, p_idx: %u\n", num_trial, p_idx);
                    if ((num_trial + p_idx) >= config->nepochs){
                        printf("entered\n");
                        continue;
                    }

                    printf("create instruction\n");
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
                            //printf("total instruction: %u\n", batch_s * config->total_lookup);

                    int output_idx = 0;
                    for (unsigned b_id = 0; b_id < batch_s; b_id++) {
                        for (unsigned t_id = 0;
                                t_id < config->tables.size();
                                t_id++) {
                            for (unsigned ll = 0;
                                    ll < config->num_indices_per_lookup[t_id];
                                    ll++) {
                                printf("b_id: %u, t_id: %u, ll: %u\n", b_id, t_id, ll);
                                addr->reset(
                                        ch,
                                        (t_id * config->accum_table_size[t_id]
                                         + config->indices[num_trial
                                         + p_idx][t_id][acum_b + b_id][ll])
                                        * config->data_size,
                                        config);
                                addr->to_string();

                                //write baseline instruction
                                write_base(
                                        config,
                                        addr->GetHexAddress(),
                                        b_time);

                                //write CXL-PNM instruction
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
    //new densemm configuration
    else if(config->opcode == 1){//DENSEMM
        printf("DENSEMM instructions\n");
        bool cache_hit = false;
        int numTiledMult = 0;
        int output_idx = 0;
        int num_cache_hit_inst = 0;

        int M = config->act_dim[0];
        int K = config->act_dim[1];
        int N = config->weight_dim[1];

        int tiledM = M/config->tile_size;
        int tiledK = K/config->tile_size;
        int tiledN = N/config->tile_size;

        unsigned num_batches = config->batch_list.back();
        unsigned num_inst_per_batch = config->indices[0][0][0].size();
        //printf("DEUBG: num_batches: %u, num_inst_per_batch: %u\n",
        //num_batches, num_inst_per_batch);

        for(int epoch = 0; epoch < config->nepochs; epoch++){
            for (auto ch: config -> channel){
                printf("channel: %u\n", ch);

                for(auto num_inst : config->num_inst){
                    //printf("num inst: %u\n", num_inst);
                    num_cache_hit_inst = 0;
                    numTiledMult = tiledM * tiledK * tiledN;
                    //printf("numtiledmult: %u\n", numTiledMult);
                    printf("init instruction\n");
                    inst->init(
                        Address(
                            ch,
                            true,
                            PNM_INST_BUF_START,
                            config
                            ).GetPNMAddress(),
                        Address(
                            ch,
                            true,
                            PNM_CONFIG_REG_START + PNM_EXE_REG_OFFSET,
                            config).GetHexAddress(),
                        num_inst
                    );
                    for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){
                        for(unsigned b_id = 0; b_id < num_batches; b_id++){
                            for(unsigned ll = 0; ll < num_inst_per_batch; ll++){
                                cache_hit = false;
                                addr -> reset(
                                    ch,
                                    (config->indices[epoch][tiledMultCount]
                                    [b_id][ll]) * config->data_size,
                                    config
                                );
                                //addr->to_string();

                                write_base(
                                    config,
                                    addr->GetHexAddress(),
                                    b_time
                                );
                                
                                //make cache_hit indicate weight or activation
                                //if cache_hit = false --> activation
                                if(ll%2 == 0){
                                    cache_hit = false;
                                }

                                //if cache_hit = true, weight.
                                //else just in case
                                else{
                                    cache_hit = true;
                                }

                                //determine output idx
                                //output_idx = ll + b_id * 8192 + (tiledMultCount/tiledK) * 16384;
                                //make temporary dummy output_idx since doesn't matter in cycle calculation
                                output_idx = 0;
                                
                                //printf("output idx: %u, cache hit: %u\n", 
                                //output_idx, cache_hit);
                                //printf("tile local idx: %u\n", ll / 8 + b_id * 4);
                                //printf("ll: %u, b_id: %u, tile number: %u\n", 
                                //ll, b_id, tiledMultCount/tiledK);
                                //if(ll % 2 == 0){
                                //    printf("ACTIVATION\n");
                                //}
                                //else{
                                //    printf("WEIGHT\n");
                                //}
                                inst->write_instruction(
                                    config->opcode,
                                    cache_hit,
                                    output_idx,
                                    addr,
                                    config->cxlpnm_out,
                                    ax_time
                                );

                                if(cache_hit){
                                    num_cache_hit_inst++;
                                }
                            }
                        }
                    }      
                    printf("percent inst with cache hit: %f\n", (100.0 * num_cache_hit_inst)/num_inst);     
                }
            }

            //write READ instructions
            for(auto ch: config->channel){
                read_psum(
                    ch,
                    tiledM * tiledN * 4096,
                    config,
                    ax_time
                );
            }
            
        }
    }
    else if (config->opcode == 2) { // blockMM
        //return 0;

        //DIFFPRUNE
        if(config->sparse_mode == Config::SparseMode::DIFFPRUNE){
            int M = config->act_dim[0];
            int K = config->act_dim[1];
            int N = config->weight_dim[1];

            //int num_inst_per_block = 0;

            int tiledM = M/config->tile_size;
            int tiledK = K/config->tile_size;
            int tiledN = N/config->tile_size;
            int numTiledMult = tiledM * tiledK * tiledN;

            printf("tiledM: %u,  tiledK: %u, tiledN: %u, numTiledMult: %u\n",
            tiledM, tiledK, tiledN, numTiledMult);

            unsigned num_batches = config->batch_list.back();
            int num_inst_per_sp_tile = config->inst_tile_info["sparse"];
            int num_inst_per_dense_tile = config->inst_tile_info["dense"];

            int output_idx;
            //0 for activation, 1 for weight
            int act_or_wgt = 0;

            printf("DIFFPRUNE instructions\n");
            for(int epoch = 0; epoch < config->nepochs; epoch++){
                for(auto ch: config->channel){
                    printf("channel: %u\n", ch);

                    for(auto num_inst: config->num_inst){
                        inst->init(
                        Address(
                            ch,
                            true,
                            PNM_INST_BUF_START,
                            config
                            ).GetPNMAddress(),
                        Address(
                            ch,
                            true,
                            PNM_CONFIG_REG_START + PNM_EXE_REG_OFFSET,
                            config).GetHexAddress(),
                        num_inst
                        );
                        for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){
                            printf("tiledMultCount: %u\n", tiledMultCount);
                            for(unsigned b_id = 0; b_id < num_batches; b_id++){
                                //if batch = 0 (activations)
                                if(b_id == 0) act_or_wgt = 0;
                                //if batch = 1 (weights)
                                else act_or_wgt = 1;

                                int num_inst_in_tile = 0;
                                //if activations are sparse
                                if(config->activation_sparse == 1){
                                    //if b_id = 0 (activations, and activations are sparse)
                                    if(b_id == 0){
                                        num_inst_in_tile = num_inst_per_sp_tile;
                                    }

                                    //if b_id = 1 (weights, and weights are dense)
                                    else if(b_id == 1){
                                        num_inst_in_tile = num_inst_per_dense_tile;
                                    }
                                }
                                //
                                else if(config->activation_sparse == 0){
                                    //if b_id = 0 (activations, and activations are dense)
                                    if(b_id == 0){
                                        num_inst_in_tile = num_inst_per_dense_tile;
                                    }
                                    //if b_id = 1 (weights, and weights are sparse)
                                    else if(b_id == 1){
                                        num_inst_in_tile = num_inst_per_sp_tile;
                                    }
                                }
                                printf("batch_id: %u, num inst written: %u\n", b_id, num_inst_in_tile);
                                for(int ll = 0; ll < num_inst_in_tile; ll++){
                                    
                                    addr -> reset(
                                        ch,
                                        (config->indices[epoch][tiledMultCount]
                                        [b_id][ll]) * config->data_size,
                                        config
                                    );

                                    write_base(
                                        config,
                                        addr->GetHexAddress(),
                                        b_time
                                    );

                                    //one day, change this to be functionally correct
                                    output_idx = 0;

                                    inst->write_instruction(
                                        config->opcode,
                                        //b_id also works (0 for activation, 1 for weight)
                                        act_or_wgt,
                                        output_idx,
                                        addr,
                                        config->cxlpnm_out,
                                        ax_time
                                    );
                                }                        
                            }
                        }
                    }
                }
            }
            //return 0;
            for(auto ch: config->channel){
                int num_output_tiles = tiledM * tiledN;
                int num_inst_per_tile = config->tile_size * config->tile_size / 16;
                read_psum(
                    ch,
                    num_output_tiles * num_inst_per_tile,
                    config,
                    ax_time
                );
            }

        }

        //BLOCKSPARSE
        else{
            int M = config->act_dim[0];
            int K = config->act_dim[1];
            int N = config->weight_dim[1];

            int num_inst_per_block = 0;

            int tiledM = M/config->tile_size;
            int tiledK = K/config->tile_size;
            int tiledN = N/config->tile_size;

            printf("tiledM: %u,  tiledK: %u, tiledN: %u\n",
            tiledM, tiledK, tiledN);

            int numTiledMult = tiledM * tiledK * tiledN;
            //int numBlockMult = config->num_dense_blk[0];
            bool cache_hit = 0;
            int output_idx = 0;
            
            unsigned num_batches = config->batch_list.back();
            //num instructions per block mult (is constant)
            unsigned num_inst_per_blk_mult = config->indices[0][0][0].size();
            //num block mult in a specific tile (is not constant)
            int numBlockMult = 0;
            printf("DEUBG: num_batches: %u, num_inst_per_blk_mult: %u\n",
            num_batches, num_inst_per_blk_mult);
            
            printf("BLOCKSPARSE instructions\n");
            for(int epoch = 0; epoch < config->nepochs; epoch++) {
                for(auto ch: config->channel){
                    printf("channel: %u\n", ch);

                    for(auto num_inst: config->num_inst){
                        inst->init(
                        Address(
                            ch,
                            true,
                            PNM_INST_BUF_START,
                            config
                            ).GetPNMAddress(),
                        Address(
                            ch,
                            true,
                            PNM_CONFIG_REG_START + PNM_EXE_REG_OFFSET,
                            config).GetHexAddress(),
                        num_inst
                        );
                        for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){
                            numBlockMult = config->indices[epoch][tiledMultCount].size();
                            printf("tiledMultCount: %u, numBlockMult: %u\n",
                            tiledMultCount, numBlockMult);
                            for(int blockMultCount = 0; blockMultCount < numBlockMult; blockMultCount++){
                                for(int inst_cnt = 0; inst_cnt < (int)num_inst_per_blk_mult; inst_cnt++){
                                    cache_hit = false;
                                    addr -> reset(
                                        ch,
                                        (config->indices[epoch][tiledMultCount][blockMultCount][inst_cnt])
                                        * config->data_size,
                                        config
                                    );
                                    
                                    write_base(
                                        config,
                                        addr->GetHexAddress(),
                                        b_time
                                    );

                                    if(inst_cnt % 2 == 0){
                                        cache_hit = false;
                                    }
                                    else{
                                        cache_hit = true;
                                    }

                                    //change this to be functionally correct... someday
                                    output_idx = 0;

                                    inst->write_instruction(
                                        config->opcode,
                                        cache_hit,
                                        output_idx,
                                        addr,
                                        config->cxlpnm_out,
                                        ax_time
                                    );
                                }
                            }
                        }
                    }
                }
            }

            //write READ instructions
            for(auto ch: config->channel){
                int blockM = M / config->blk_size;
                int blockN = N / config->blk_size;
                num_inst_per_block = config->blk_size*config->blk_size/16;
                read_psum(
                    ch,
                    blockM * blockN * num_inst_per_block,
                    config,
                    ax_time
                );
            }
        
        }
    }
    else if (config->opcode == 3){// delta_act
        printf("delta ACT instructions\n");
        int M = config->act_dim[0];
        int K = config->act_dim[1];
        int N = config->weight_dim[1];

        int tiledM = M/config->tile_size;
        int tiledK = K/config->tile_size;
        int tiledN = N/config->tile_size;

        unsigned num_batches = config->batch_list.back();
        unsigned num_inst_per_batch = config->indices[0][0][0].size();
        printf("num inst per batch: %u, num inst per sp tile: %u, num inst per dense tile: %u\n",
        num_inst_per_batch, config->num_inst_per_sp_tile, config->num_inst_per_dense_tile);

        int output_idx = 0;
        int numTiledMult = 0;
        bool is_weight = false;

        for(int epoch = 0; epoch < config->nepochs; epoch++){
            for(auto ch: config->channel){
                for(auto num_inst : config->num_inst){
                    numTiledMult = tiledM * tiledK * tiledN;
                    
                    printf("init instruction\n");
                    inst->init(
                        Address(
                            ch,
                            true,
                            PNM_INST_BUF_START,
                            config
                            ).GetPNMAddress(),
                        Address(
                            ch,
                            true,
                            PNM_CONFIG_REG_START + PNM_EXE_REG_OFFSET,
                            config).GetHexAddress(),
                        num_inst
                    );
                    for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){
                        for(unsigned b_id = 0; b_id < num_batches; b_id++){
                            for(unsigned ll = 0; ll < num_inst_per_batch; ll++){
                                addr -> reset(
                                    ch,
                                    (config->indices[epoch][tiledMultCount]
                                    [b_id][ll]) * config->data_size,
                                    config
                                );

                                write_base(
                                    config,
                                    addr->GetHexAddress(),
                                    b_time
                                );

                                output_idx = 0;

                                //activations
                                if(ll < ((unsigned)config->num_inst_per_sp_tile)/2){
                                    is_weight = false;
                                }
                                else{
                                    is_weight = true;
                                }

                                inst->write_instruction(
                                    config->opcode,
                                    is_weight,
                                    output_idx,
                                    addr,
                                    config->cxlpnm_out,
                                    ax_time
                                );

                            }
                        }
                    }
                }
            }
            for(auto ch: config->channel){
                read_psum(
                    ch,
                    tiledM * tiledN * 4096,
                    config,
                    ax_time
                );
            }
        }
    }
    else{
        assert(false);
    }

    cout << " --- End ---" << endl;

    return 0;
}
