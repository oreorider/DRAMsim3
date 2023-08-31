#ifndef __CONFIG_H
#define __CONFIG_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <boost/program_options.hpp>

#define INST_BUFFER_BYTE_SIZE (256 * 1024)
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

using namespace std;

class Config {
    enum class PoolType {
        FIXED, 
        RANDOM, 
        PROD, 
        SIZE
    }; 

 public:
    Config(int argc, const char* argv[]);     
    ~Config();
    
    vector<unsigned> split(string str, char delimiter); 

    void SetAddressMapping();
    
    unsigned opcode;  
    int nepochs;
    unsigned batch_size;
    vector<unsigned> batch_list; 
    string table_list;
    int num_tables; 
    vector<unsigned> tables;
    vector<unsigned> accum_table_size;
    int sparse_feature_size; 
    int data_type_size; 
    int data_size; 

    int max_indices_per_lookup; 
    int pooling_type;
    string pooling_prod_list;
    vector<unsigned> pooling_prod;  
    vector<unsigned> num_indices_per_lookup; // [table]
    int total_lookup; // per batch

    // [nepochs] [table] [lookup]  
    vector<vector<vector<vector<unsigned>>>> indices; 

    int default_interval;
    int miss_ratio;
    bool base_only; 

    string channel_list, rank_list;  
    vector<unsigned> channel, rank;
    int ch_size, ra_size, bg_size, ba_size, ro_size, co_size;
    int ch_bits, ra_bits, bg_bits, ba_bits, ro_bits, co_bits;
    int ch_pos, ra_pos, bg_pos, ba_pos, ro_pos, co_pos;
    uint64_t ch_mask, ra_mask, bg_mask, ba_mask, ro_mask, co_mask;
    int addr_bits;
    int shift_bits;
    int bus_width, BL;
    string address_mapping;

    string file_name; 
    ofstream base_out, cxlpnm_out;  
}; 

int LogBase2(int power_of_two);

#endif
