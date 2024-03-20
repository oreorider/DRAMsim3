#include "config.h"

namespace po = boost::program_options;

Config::Config(int argc, const char* argv[])
{
    po::options_description desc{"Options"};
    desc.add_options()
        ("help, h", "Help")
        ("opcode",                 po::value<unsigned>(&opcode)->default_value(0),                     "opcode")
        ("nepochs",                po::value<int>(&nepochs)->default_value(2),                         "nepochs")
        ("batch_size",             po::value<unsigned>(&batch_size)->default_value(64),                "batch_size")
        ("embedding_table, t",     po::value<string>(&table_list)->default_value("1000000-1000000"),   "embedding table list")
        ("num_tables",             po::value<int>(&num_tables)->default_value(0),                      "num_tables")
        ("sparse_feature_size",    po::value<int>(&sparse_feature_size)->default_value(16),            "sparse_feature_size")
        ("data_type_size",         po::value<int>(&data_type_size)->default_value(4),                  "data_type_size")
        ("max_indices_per_lookup", po::value<int>(&max_indices_per_lookup)->default_value(50),         "max_indices_per_lookup")
        ("pooling_type",           po::value<int>(&pooling_type)->default_value(0),                    "pooling_type")
        ("pooling_prod_list",      po::value<string>(&pooling_prod_list)->default_value("48-122-25-50-30-70-25-25-25-40"), "num_indices_per_lookup_prod")
        ("default_interval",       po::value<int>(&default_interval)->default_value(4),                "default_interval")
        ("miss_ratio",             po::value<int>(&miss_ratio)->default_value(100),                    "miss_ratio")
        ("base_only",              po::value<bool>(&base_only)->default_value(false),                  "base_only")
        ("channel",                po::value<string>(&channel_list)->default_value("1"),               "channel")
        ("rank",                   po::value<string>(&rank_list)->default_value("0-1"),                "rank")
        ("bg_size",                po::value<int>(&bg_size)->default_value(4),                         "bg_size")
        ("ba_size",                po::value<int>(&ba_size)->default_value(4),                         "ba_size")
        ("ro_size",                po::value<int>(&ro_size)->default_value(65536),                     "ro_size")
        ("co_size",                po::value<int>(&co_size)->default_value(1024),                      "co_size")
        ("bus_width",              po::value<int>(&bus_width)->default_value(64),                      "bus_width")
        ("BL",                     po::value<int>(&BL)->default_value(8),                              "BL")
        ("address_mapping",        po::value<string>(&address_mapping)->default_value("rochrababgco"), "address_mapping")
        ("file_name, f",           po::value<string>(&file_name)->default_value("test"),               "Output trace file name")
        ("act_dim",                po::value<string>(&act_dim_list)->default_value("32-64"),           "activation dimension ")
        ("weight_dim",             po::value<string>(&weight_dim_list)->default_value("64-64"),        "weight dimension")
        ("tile_size",              po::value<int>(&tile_size)->default_value(256),                     "tile size")
        ("blk_size",               po::value<int>(&blk_size)->default_value(32),                 "block sparse dimension")
        ("density",                po::value<float>(&density)->default_value(99.0),                    "density")
        ("activation_sparse",      po::value<int>(&activation_sparse)->default_value(0),               "sparse activations")
        ;

    po::variables_map vm;
    //parse CONFIG, save private instance variables
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
        cout << desc << endl;
        assert(false);
    }
    po::notify(vm);

    //for SLS, 16 * 4
    data_size = sparse_feature_size * data_type_size;

    channel = split(channel_list, '-');//0

    rank = split(rank_list, '-');//[0,1]

    ch_size = channel.size();//1

    ra_size = rank.size();//2

    if(blk_size == 1){
        sparse_mode = SparseMode::DIFFPRUNE;
    }
    else if(blk_size == 16){
        sparse_mode = SparseMode::BLOCK16;
    }
    else if(blk_size == 32){
        sparse_mode = SparseMode::BLOCK32;
    }
    else{
        sparse_mode = SparseMode::OTHER;
    }



    //create base, cxl file in current directory
    //will move to /scripts/traces at the end of bash script
    base_out.open(file_name + "_base.trc");
    if (!base_only)
        cxlpnm_out.open(file_name + "_cxlpnm.trc");

    printf("opcode: %u\n", opcode);
    if (opcode == 0 ) {
        tables = split(table_list, '-'); //[1000000, 1000000]

        //num_tables = 0 by default
        if(num_tables != 0) {
            tables.resize(num_tables, tables[0]);
        }

        //accum_table_size.resize(3)
        accum_table_size.resize(tables.size() + 1);
        accum_table_size[0] = 0;
        for(unsigned i = 1; i < accum_table_size.size(); i++) {
            accum_table_size[i] = accum_table_size[i-1] + tables[i-1];
        }
        //accum_table_size: [0, 1000000, 1000000]

        //[48, 112, 25, 50, 30, 70, 25, 25, 25, 40]
        pooling_prod = split(pooling_prod_list, '-');

        //num_indices_per_lookup.size() = 2
        num_indices_per_lookup.resize(tables.size()); // [table]
        indices.resize(nepochs); // [nepochs] [table] [batch] [lookup]
        printf("num epochs: %u\n", (int)indices.size());
        total_lookup = 0;
        int p = 0;

        //create embedding table - indices [epoch][table][batch][maxindex_per_lookup]
        //                         indices   [2]    [2]   [4]     [50]
        //loop through epochs dimension
        for (int c = 0; c < nepochs; c++) {
            indices[c].resize(tables.size());
            printf("num tables: %u\n", (int)indices[c].size());
            
            //loop through table dimension
            for (unsigned i = 0; i < tables.size(); i++) {
                printf("loop through table dimension, i = %u\n", i);
                //if first time
                if (c == 0) {
                    //by default, pooling type = 0
                    switch (static_cast<PoolType>(pooling_type)) {
                        case PoolType::FIXED:
                            num_indices_per_lookup[i] = max_indices_per_lookup;
                            break;
                        case PoolType::RANDOM:
                            num_indices_per_lookup[i] =
                                rand() % max_indices_per_lookup + 1;
                            break;
                        case PoolType::PROD:
                            num_indices_per_lookup[i] = pooling_prod[p];
                            p = (p + 1) % pooling_prod.size();
                            break;
                        default:
                            break;
                    }
                    printf("num indcies per lookup[i] : %u\n", num_indices_per_lookup[i]);

                    //for default case
                    //num_indices_per_lookup[i] = 50
                    assert(num_indices_per_lookup[i] <= tables[i]);

                    total_lookup += num_indices_per_lookup[i];
                    //printf("total lookup value : %d\n", total_lookup);
                    //at end of loops, total_lookup = 100
                }
                indices[c][i].resize(batch_size);
                printf("batch size: %u\n", (int)indices[c][i].size());

                //loop through batch dimension
                for (unsigned j = 0; j < batch_size; j++) {
                    set<unsigned> unique_idx;
                    printf("loop through batch dimension, j = %u\n", j);
                    //for default,
                    //while unique_idx.size() < 50
                    printf("adding values\n");
                    while (unique_idx.size() < num_indices_per_lookup[i]) {
                        //unsigned idx_tt = unique_idx.size();

                        //generate random number
                        //rand() % 1000000
                        unsigned idx_tt = rand() % tables[i];

                        //if random number not in unique_idx set
                        
                        if (unique_idx.find(idx_tt) == unique_idx.end()) {
                            //add number to lookup
                            indices[c][i][j].push_back(idx_tt);
                            unique_idx.insert(idx_tt);
                            printf("%x ", idx_tt);
                        }
                        
                    }
                    
                    printf("\n");
                    printf("lookup table size: %u\n", (int)indices[c][i][j].size());
                    printf("epoch: %d, table: %d, batch: %d\n", c, i, j);
                    //for (auto& element : indices[c][i][j]){
                    //    printf("%x ", element);
                    //}
                    printf("\n");
                }
                

            }
        }
        // batch list
        // # of instruction / batch = total_lookup
        // Inst size = # of instruction x 8B
        // 256 KB / Inst size = batch_s
        int batch_s = INST_BUFFER_BYTE_SIZE / (total_lookup * 8);
        int tmp_batch_size = batch_size;
        //printf("batch_s: %u\n", batch_s);

        while (tmp_batch_size > 0) {
            batch_list.push_back(min(batch_s, tmp_batch_size));
            //printf("pushed %u \n", batch_list.back());
            tmp_batch_size -= batch_s;
        }
        
        //printf("batch list contents\n");
        //for(auto& element : batch_list){
        //    printf("%u ", element);
        //}
        //printf("\n");
    }
    
    else if(opcode == 1){
        weight_dim  = split(weight_dim_list, '-');
        act_dim     = split(act_dim_list, '-');

        int M = act_dim[0];
        int K = act_dim[1];
        int N = weight_dim[1];
        printf("M: %u, K: %u, N: %u\n", M, K, N);
        //[MxK] * [KxN] = [MxN] matrix mult
        //split into 16x16
        int tiledM = M/tile_size;
        int tiledK = K/tile_size;
        int tiledN = N/tile_size;

        int num_inst_cnt = 0;

        int numTiledMult = tiledM * tiledK * tiledN;
        printf("tiledM: %u,  tiledK: %u, tiledN: %u, numTiledMult: %u\n",
        tiledM, tiledK, tiledN, numTiledMult);

        //instantiate activations, weights to hold unique values
        int num_inst_per_tile = tile_size * tile_size / 16;
        printf("num inst per tile: %u\n", num_inst_per_tile);
        //unsigned activations [tiledM][tiledK][num_inst_per_tile];//each tile holds 16 instructions
        //unsigned weights [tiledK][tiledN][num_inst_per_tile];

        //initialize vector, set size
        std::vector<std::vector<std::vector<int>>> activations(
            tiledM, std::vector<std::vector<int>>(
            tiledK, std::vector<int>(
            num_inst_per_tile, 0)));
        std::vector<std::vector<std::vector<int>>> weights(
            tiledK, std::vector<std::vector<int>>(
            tiledN, std::vector<int>(
            num_inst_per_tile, 0)));
        
        set<unsigned> unique_vals;
        int data_num = 0;
        //instantiate activations
        printf("printing instantiated activations\n");
        for(int i = 0; i < tiledM; i++){
            for(int j = 0; j < tiledK; j++){
                printf("i: %u, j: %u\n", i, j);
                //int k = 0;
                for(int k = 0; k < num_inst_per_tile; k++){
                    //unsigned act_val = rand() % 1000000;
                    //if(unique_vals.find(act_val) == unique_vals.end()){
                    //    activations[i][j][k] = act_val;
                    //    k++;
                    //    unique_vals.insert(act_val);
                    //    printf("%u ", act_val);
                    //}
                    activations[i][j][k] = data_num;
                    data_num+=1;
                    //k+=1;
                    printf("%u ", activations[i][j][k]);
                }
                printf("\n");
            }
        }

        //instantiate weights
        printf("printing instantiated weights\n");
        for(int i = 0; i < tiledK; i++){
            for(int j = 0; j < tiledN; j++){
                printf("i: %u, j: %u\n", i, j);
                //int k = 0;
                for(int k = 0; k < num_inst_per_tile; k++){
                //while(k < num_inst_per_tile){
                    //unsigned weight_val = rand() % 1000000;
                    //if(unique_vals.find(weight_val) == unique_vals.end()){
                    //    weights[i][j][k] = weight_val;
                    //    k++;
                    //    unique_vals.insert(weight_val);
                    //    printf("%u ", weight_val);
                    //}
                    weights[i][j][k] = data_num;
                    data_num++;
                    //k++;
                    printf("%u ", weights[i][j][k]);
                }
                printf("\n");
            }
        }

        indices.resize(nepochs);
        //create indices[nepochs][num_mult][batch][max_index]
        //loop through nepochs dimension
        for(int epoch = 0; epoch < nepochs; epoch++){
            printf("epoch: %u\n", epoch);
            indices[epoch].resize(numTiledMult);
            int tiledM_idx = 0;
            int tiledN_idx = 0;
            int tiledK_idx = 0;
            //loop through numTiledMult dimension
            for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){
                printf("tiledMultCount: %u\n", tiledMultCount);
                indices[epoch][tiledMultCount].resize(batch_size);


                //increases weight cache hits
                tiledM_idx = (tiledMultCount / tiledK) % tiledM;
                tiledN_idx = tiledMultCount / (tiledK * tiledM);
                tiledK_idx = tiledMultCount % tiledK;

                //increases activation cache hits (useless)
                //tiledM_idx = tiledMultCount/(tiledK * tiledN);
                //tiledN_idx = (tiledMultCount - tiledM_idx * tiledK * tiledN)/tiledK;
                //tiledK_idx = tiledMultCount % tiledK;

                //printf("tiledMultCount: %u\n", tiledMultCount);
                printf("tiledM_idx: %u, tiledN_idx: %u, tiledK_idx: %u\n",
                tiledM_idx, tiledN_idx, tiledK_idx);

                //loop through batch dimension
                //batch_size = 2
                for(unsigned batch = 0; batch < batch_size; batch++){
                    //indices[epoch][tiledMultCount][batch].resize(32);
                    printf("batch: %u\n",batch);

                    //loop through maxindex dimension
                    //for(int inst_cnt = 0; inst_cnt < num_inst_per_tile; inst_cnt++){
                    //int num_inst_per_batch = 2 * (tile_size / batch_size) * tile_size / 16;
                    for(int inst_cnt = 0; inst_cnt < num_inst_per_tile; inst_cnt++){

                        //add inst_data for fetching activation
                        indices[epoch][tiledMultCount][batch].push_back(
                            activations[tiledM_idx][tiledK_idx][(batch * 2048) + (inst_cnt % 2048)]
                            //activations[tiledM_idx][tiledK_idx][(inst_cnt + batch * num_inst_per_tile/2) % (num_inst_per_tile)]
                        );
    
                        //add inst_data for fetching weight
                        indices[epoch][tiledMultCount][batch].push_back(
                            weights[tiledK_idx][tiledN_idx][inst_cnt]  
                        );

                        printf("{%u [%u], %u [%u]}\t\t", 
                        activations[tiledM_idx][tiledK_idx][(batch * 2048) + (inst_cnt % 2048)],
                        (batch * 2048) + (inst_cnt % 2048),
                        weights[tiledK_idx][tiledN_idx][inst_cnt],
                        inst_cnt);
                        
                        if(inst_cnt%4 == 0){
                            printf("\n");
                        }

                        num_inst_cnt +=2;
                    }
                    printf("\n");
                    printf("inst added: %lu\n", indices[epoch][tiledMultCount][batch].size());
                    printf("accumulate total instructions: %u\n", num_inst_cnt);
                }
            }
        }
        printf("total %u instructions\n", num_inst_cnt);
        batch_list.push_back(2);
        num_inst.push_back(num_inst_cnt);//num inst per epoch

    }
    
    //else if(opcode == 2 && (sparse_mode == SparseMode::BLOCK32 || sparse_mode == SparseMode::BLOCK16)){
    else if(0){
        weight_dim  = split(weight_dim_list, '-');
        act_dim     = split(act_dim_list, '-');

        int M = act_dim[0];
        int K = act_dim[1];
        int N = weight_dim[1];

        int num_inst_cnt = 0;
        int tiledM = 0;
        int tiledK = 0;
        int tiledN = 0;
        int num_inst_per_block = 0;

        set<unsigned> unique_vals;

        int data_num = 0;

        if(sparse_mode == SparseMode::BLOCK32){
            tiledM = M/32;
            tiledK = K/32;
            tiledN = N/32;
            num_inst_per_block = 32 * 32 / 16;
            printf("tiledM: %u, tiledK: %u, tiledN: %u, num_inst_per_block: %u\n",
            tiledM, tiledK, tiledN, num_inst_per_block);
        }

        else if(sparse_mode == SparseMode::BLOCK16){
            tiledM = M/16;
            tiledK = K/16;
            tiledN = N/16;
            num_inst_per_block = 16 * 16 / 16;
            printf("tiledM: %u, tiledK: %u, tiledN: %u, num_inst_per_block: %u\n",
            tiledM, tiledK, tiledN, num_inst_per_block);
        }

        else{
            //sparse mode unspecified
            cerr << "Error: unsupported block size!"<<endl;
        }

        std::vector<std::pair<int, int>> dense_block_idxs;
        std::vector<std::pair<int, int>> tmp_wgt_blk_idxs;

        int dense_blk_cnt = 0;

        //unsigned activations[tiledM][tiledK][num_inst_per_block];
        //unsigned weights[tiledK][tiledN][num_inst_per_block];

        std::vector<std::vector<std::vector<int>>> activations(
            tiledM, std::vector<std::vector<int>>(
            tiledK, std::vector<int>(
            num_inst_per_block, 0)));
        std::vector<std::vector<std::vector<int>>> weights(
            tiledK, std::vector<std::vector<int>>(
            tiledN, std::vector<int>(
            num_inst_per_block, 0)));

        float r = 0.0;

        //create activations (activations are dense)
        printf("activations (dense) \n");
        for(int i = 0; i < tiledM; i++){
            for(int j = 0; j < tiledK; j++){
                r = (rand() % 10000)/100.0;
                for(int k = 0; k < num_inst_per_block; k++){
                    activations[i][j][k] = data_num;
                    data_num +=1;
                    //printf("%u ", activations[i][j][k]);
                }
            }
        }

        int num_activations = data_num;
        printf("weights (block sparse)\n");
        //create weights (weights are sparse)
        srand(time(NULL));
        int expected_dense_blk_cnt = tiledK * tiledN * (density/100.);
        printf("expected dense blk cnt: %u\n", expected_dense_blk_cnt);
        //return;

        while(true){
            for(int i = 0; i < tiledK; i++){
                for(int j = 0; j < tiledN; j++){
                    r = (rand() % 100);

                    //fill in 32x32 block
                    if(r<=density){
                        //printf("r: %u\n", r);
                        //printf("block number %u at idx i:%u, j:%u\n", dense_blk_cnt, i, j);
                        dense_blk_cnt++;
                        //dense_block_idxs.push_back(std::pair<int, int>(i,j));
                        tmp_wgt_blk_idxs.push_back(std::pair<int, int>(i,j));
                        for(int k = 0; k < num_inst_per_block; k++){
                            weights[i][j][k] = data_num;
                            data_num += 1;
                        }
                        //printf("\n");
                    }
                    else{
                        for(int k = 0; k < num_inst_per_block; k++){
                            weights[i][j][k] = 0;
                        }
                    }
                }
            }
            printf("dense blk cnt: %u\n", dense_blk_cnt);
            if(dense_blk_cnt == expected_dense_blk_cnt){
                printf("configuration good\n");
                dense_block_idxs.insert(dense_block_idxs.end(), tmp_wgt_blk_idxs.begin(), tmp_wgt_blk_idxs.end());
                printf("dense block idx size: %lu\n", dense_block_idxs.size());
                break;
            }
            else{
                printf("retrying\n");
                tmp_wgt_blk_idxs.clear();
                dense_blk_cnt = 0;
                data_num = num_activations;
            }
            
        }
        //return;

        //int numTiledMult = tiledM * tiledK * tiledN;
        //printf("number of tiledMults: %u\n", numTiledMult);
        printf("number of dense blocks: %u\n", dense_blk_cnt);
        int numBlockMult = dense_blk_cnt * tiledM;
        num_dense_blk.push_back(dense_blk_cnt);
        printf("number of dense mults total: %u\n", numBlockMult);
        indices.resize(nepochs);
        
        //for(int epoch = 0; epoch < nepochs; epoch++){
        //    
        //}

        for(int epoch = 0; epoch < nepochs; epoch++){
            printf("epoch: %u\n", epoch);
            indices[epoch].resize(numBlockMult);   
            
            int tiledM_idx = 0;
            int tiledK_idx = 0;
            int tiledN_idx = 0;

            for(int blockMultCount = 0; blockMultCount < numBlockMult; blockMultCount++){
                printf("blockMultCount: %u\n", blockMultCount);
                indices[epoch][blockMultCount].resize(1);

                //increment M_idx after doing all block mult for one row of actiation
                int nth_dense_block = blockMultCount%dense_blk_cnt;
                tiledM_idx = blockMultCount/dense_blk_cnt;
                tiledK_idx = dense_block_idxs[nth_dense_block].first;
                tiledN_idx = dense_block_idxs[nth_dense_block].second;

                printf("tiledM_idx: %u, tiledK_idx: %u, tiledN_idx: %u\n",
                tiledM_idx, tiledK_idx, tiledN_idx);

                //not really neccessary
                for(unsigned batch = 0; batch < 1; batch++){
                    printf("batch: %u\n", batch);

                    //instructions that loads 32x32 activations, 32x32 weights
                    for(int inst_cnt = 0; inst_cnt < num_inst_per_block; inst_cnt++){

                        //add instructions that fetch activations
                        indices[epoch][blockMultCount][batch].push_back(
                            activations[tiledM_idx][tiledK_idx][inst_cnt]
                        );

                        //add instructions that fetch weights
                        indices[epoch][blockMultCount][batch].push_back(
                            weights[tiledK_idx][tiledN_idx][inst_cnt]
                        );

                        //printf("{%u [%u], %u [%u]}\t\t",
                        //indices[epoch][blockMultCount][batch].back(), 
                        //batch*2+inst_cnt%2,
                        //indices[epoch][blockMultCount][batch].back(),
                        //inst_cnt);

                        //for formatting
                        //if(inst_cnt % 4 == 0){
                        //    printf("\n");
                        //}

                        num_inst_cnt+=2;
                    }
                    printf("num_inst: %u\n", num_inst_cnt);

                }

            }
        }
        
        printf("total %u instructions\n", num_inst_cnt);
        batch_list.push_back(1);
        num_inst.push_back(num_inst_cnt);
        
        
    }
    
    else if(opcode == 2 && (sparse_mode == SparseMode::BLOCK32 || sparse_mode == SparseMode::BLOCK16)){
    //else if(0){
        weight_dim  = split(weight_dim_list, '-');
        act_dim     = split(act_dim_list, '-');

        int M = act_dim[0];
        int K = act_dim[1];
        int N = weight_dim[1];

        int num_inst_cnt = 0;
        int tiledM = 0;
        int tiledK = 0;
        int tiledN = 0;

        int blockM = 0;
        int blockK = 0;
        int blockN = 0;
        int num_inst_per_block = 0;

        set<unsigned> unique_vals;

        int data_num = 0;

        //tile coordinates (tile size is 256x256)
        tiledM = M/tile_size;
        tiledK = K/tile_size;
        tiledN = N/tile_size;
        int numTiledMult = tiledM * tiledK * tiledN;

        if(blk_size == 16){
            printf("BLOCK16\n");
        }
        else if(blk_size == 32){
            printf("BLOCK32\n");
        }
        else{
            cerr << "error: unsupported block size"<<endl;
        }

        num_inst_per_block = blk_size * blk_size / 16;
        blockM = M / blk_size;
        blockK = K / blk_size;
        blockN = N / blk_size;

        printf("tiledM: %u, tiledK: %u, tiledN: %u, numTiledMult: %u, num_inst_per_block: %u\n",
        tiledM, tiledK, tiledN, numTiledMult, num_inst_per_block);

        //std::vector<std::pair<int, int>> dense_block_idxs;
        //std::vector<std::pair<int, int>> tmp_wgt_blk_idxs;
        std::map<std::pair<int, int>, std::vector<int>> dense_blk_idxs;

        int dense_blk_cnt = 0;
        int expected_dense_blk_cnt = blockK * blockN * (density/100.);
        int blk_per_tile = tile_size * tile_size / (blk_size * blk_size);
        printf("blk_per_tile: %u, expected dense blk cnt: %u\n", 
        blk_per_tile, expected_dense_blk_cnt);

        std::vector<std::vector<std::vector<std::vector<int>>>> activations(tiledM,
            std::vector<std::vector<std::vector<int>>>(tiledK,
                std::vector<std::vector<int>>(blk_per_tile, 
                    std::vector<int>(num_inst_per_block))));

        std::vector<std::vector<std::vector<std::vector<int>>>> weights(tiledK,
            std::vector<std::vector<std::vector<int>>>(tiledN,
                std::vector<std::vector<int>>(blk_per_tile, 
                    std::vector<int>(num_inst_per_block))));

        
        printf("activations (dense)\n");
        for(int i = 0; i < tiledM; i ++){
            for(int j = 0; j < tiledK; j++){
                for(int k = 0; k < blk_per_tile; k++){
                    for(int l = 0; l < num_inst_per_block; l++){
                        activations[i][j][k][l] = data_num;
                        data_num+=1;
                    }
                }
            }
        }
        int num_activations = data_num;
    
        printf("weights (sparse)\n");
        int r = 0;
        srand(time(NULL));
        while(dense_blk_cnt != expected_dense_blk_cnt){
            printf("expected blk cnt: %u\n", expected_dense_blk_cnt);
            for(int i = 0; i < tiledK; i ++){
                for(int j = 0; j < tiledN; j++){
                    for(int k = 0; k < blk_per_tile; k++){
                        r = rand() % 100;
                        if(r <= density){
                            dense_blk_cnt++;
                            dense_blk_idxs[{i, j}].push_back(k);

                            for(int l = 0; l < num_inst_per_block; l++){
                                weights[i][j][k][l] = data_num;
                                data_num+=1;
                            }
                        }
                        else{
                            for(int l = 0; l < num_inst_per_block; l++){
                                weights[i][j][k][l] = 0;
                            }
                        }
                        
                    }
                }
            }
            printf("dense blk cnt: %u\n", dense_blk_cnt);
            if(dense_blk_cnt == expected_dense_blk_cnt){
                printf("configuartion good\n");
            }
            else{
                printf("retrying\n");
                dense_blk_cnt = 0;
                dense_blk_idxs.clear();
            }
        }
        
        //print dense blk idxs
        for (const auto& pair_vec : dense_blk_idxs) {
            printf("tile idx: (%d, %d), blk_idx: ", pair_vec.first.first, pair_vec.first.second);
                for (int value : pair_vec.second) {
                    printf("%d ", value);
                }
            printf("\n");
        }
        int totalBlockMult = 0;
        num_dense_blk.push_back(0);
        indices.resize(nepochs);
        //int num_dense_blk_in_tile = 0;
        for(int epoch = 0; epoch < nepochs; epoch++){
            printf("epoch: %u\n", epoch);
            indices[epoch].resize(numTiledMult);

            int tiledM_idx = 0;
            int tiledK_idx = 0;
            int tiledN_idx = 0;

            for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){   
                //to increase weight cache hits
                tiledM_idx = (tiledMultCount / tiledK) % tiledM;
                tiledN_idx = tiledMultCount / (tiledK * tiledM);
                tiledK_idx = tiledMultCount % tiledK;
                
                //to increase activation cache hits
                //tiledM_idx = tiledMultCount/(tiledK * tiledN);
                //tiledN_idx = (tiledMultCount - tiledM_idx * tiledK * tiledN)/tiledK;
                //tiledK_idx = tiledMultCount % tiledK;
                //printf("tiledMultCount: %u, tiledM_idx: %u, tiledN_idx: %u, tiledK_idx: %u\n",
                //tiledMultCount, tiledM_idx, tiledN_idx, tiledK_idx);             
                
                std::pair<int, int> tile_coord = {tiledK_idx, tiledN_idx};
                
                //number of dense blocks in tile
                int num_dense_blk_in_tile = dense_blk_idxs[tile_coord].size();
                //number of block multiplications that take place in the tile
                int numBlockMult = num_dense_blk_in_tile * (tile_size/blk_size);
                totalBlockMult += numBlockMult;
                

                indices[epoch][tiledMultCount].resize(numBlockMult);

                printf("activation (%u, %u)\n", tiledM_idx, tiledK_idx);    
                printf("weight tile (%u, %u) num dense blk : %u, numBlockMult: %u\n",
                tiledK_idx, tiledN_idx, num_dense_blk_in_tile, numBlockMult);
                
                printf("tiledMultCount: %u, tiledM_idx: %u, tiledN_idx: %u, tiledK_idx: %u\n",
                tiledMultCount, tiledM_idx, tiledN_idx, tiledK_idx);

                //from 0 to total number of blockmults
                for(int blockMultCount = 0; blockMultCount < numBlockMult; blockMultCount++){
                    //nth block in tile
                    int nth_dense_block = blockMultCount%num_dense_blk_in_tile;
                    //number of the block 0~64(BLOCK32) or 0~256(BLOCK16)
                    int blk_idx = dense_blk_idxs[tile_coord][nth_dense_block];
                    printf("blockMultCount: %u, nth_dense_block: %u, blk_idx: %u\n", 
                    blockMultCount, nth_dense_block, blk_idx);

                    for(int inst_cnt = 0; inst_cnt < num_inst_per_block; inst_cnt++){
                        indices[epoch][tiledMultCount][blockMultCount].push_back(
                            activations[tiledM_idx][tiledK_idx][blk_idx][inst_cnt]
                        );
                        
                        indices[epoch][tiledMultCount][blockMultCount].push_back(
                            weights[tiledK_idx][tiledN_idx][blk_idx][inst_cnt]
                        );
                        num_inst_cnt += 2;

                    }
                }
            }
        }
        num_dense_blk.push_back(totalBlockMult);
        printf("total block multiplications: %u\n", totalBlockMult);

        printf("total instructions: %u\n", num_inst_cnt);
        batch_list.push_back(0);
        num_inst.push_back(num_inst_cnt);
        //return;
    }
    
    else if(opcode == 2 && sparse_mode == SparseMode::DIFFPRUNE){
        printf("diffprune\n");
        weight_dim  = split(weight_dim_list, '-');
        act_dim     = split(act_dim_list, '-');

        int M = act_dim[0];
        int K = act_dim[1];
        int N = weight_dim[1];
        printf("M: %u, K: %u, N: %u\n", M, K, N);

        set<unsigned> unique_vals;

        int tiledM = M/tile_size;
        int tiledK = K/tile_size;
        int tiledN = N/tile_size;

        int numTiledMult = tiledM * tiledK * tiledN;
        printf("tiledM: %u,  tiledK: %u, tiledN: %u, numTiledMult: %u\n",
        tiledM, tiledK, tiledN, numTiledMult);

        //int num_inst_per_sp_tile = (tile_size * tile_size / 16) * (density/100.);
        int num_inst_per_sp_tile = 0;
        if(activation_sparse == 1){
            //(256 * 256 / 8) * 0.2 = 1638.4
            num_inst_per_sp_tile = 1638;
        }
        //hardcode to 1%
        else{
            //num_inst_per_sp_tile = (tile_size * tile_size / 8) * (density/100.);
            num_inst_per_sp_tile = 82;
        }
        int num_inst_per_dense_tile = tile_size * tile_size / 16;

        inst_tile_info.insert(std::pair<std::string, int>("dense", num_inst_per_dense_tile));
        inst_tile_info.insert(std::pair<std::string, int>("sparse", num_inst_per_sp_tile));

        
        printf("num inst per sp tile: %u\n", num_inst_per_sp_tile);
        printf("num inst per dense tile: %u\n", num_inst_per_dense_tile);

        //allocate enough memory to store both dense matrixes
        //unsigned activations[tiledM][tiledK][num_inst_per_dense_tile];
        //unsigned weights[tiledK][tiledN][num_inst_per_dense_tile];

        //THE SPARSE MATRIX IS STORED IN CSR FORMAT
        std::vector<std::vector<std::vector<int>>> activations(
            tiledM, std::vector<std::vector<int>>(
            tiledK, std::vector<int>(
            num_inst_per_dense_tile, 0)));
        std::vector<std::vector<std::vector<int>>> weights(
            tiledK, std::vector<std::vector<int>>(
            tiledN, std::vector<int>(
            num_inst_per_dense_tile, 0)));
        int num_inst_cnt = 0;

        //int nnz;

        
        //float r = 0.0;

        //create activations
        printf("activations (diffprune) \t");
        if(activation_sparse){
            printf("activations are sparse, weights are dense\n");
        }
        else{
            printf("activations are dense, weights are sparse\n");
        }

        int data_num = 0;

        //if activation sparse, weight dense
        if(activation_sparse){
            for(int i = 0; i < tiledM; i++){
                for(int j = 0; j < tiledK; j++){
                    for(int k = 0; k < num_inst_per_sp_tile; k++){
                        //stored in csr format
                        activations[i][j][k] = data_num;
                        data_num++;
                    }
                }
            }
            for(int i = 0; i < tiledK; i++){
                for(int j = 0; j < tiledM; j++){
                    for(int k = 0; k < num_inst_per_dense_tile; k++){
                        weights[i][j][k] = data_num;
                        data_num++;
                    }
                }
            }
        }

        //if activation dense, weight sparse
        else{
            for(int i = 0; i < tiledM; i++){
                for(int j = 0; j < tiledK; j++){
                    for(int k = 0; k < num_inst_per_dense_tile; k++){
                        activations[i][j][k] = data_num;
                        data_num++;
                    }
                }
            }
            for(int i = 0; i < tiledK; i++){
                for(int j = 0; j < tiledM; j++){
                    for(int k = 0; k < num_inst_per_sp_tile; k++){
                        //stored in CSR format
                        weights[i][j][k] = data_num;
                        data_num++;
                    }
                }
            }
        }


        /*
        for(int i = 0; i < tiledM; i++){
            for(int j = 0; j < tiledK; j++){

                //if activations are sparse and selected
                if(activation_sparse == 1){
                    int k = 0;
                    while(k != num_inst_per_sp_tile){
                        unsigned act_val = rand() % 1000000;
                        if(unique_vals.find(act_val) == unique_vals.end()){
                            activations[i][j][k] = act_val;
                            unique_vals.insert(act_val);
                            k++;
                            //printf("%u ", act_val);
                        }
                    }
                    //printf("\n");
                }
                else if(activation_sparse == 0){
                    int k = 0;
                    while(k != num_inst_per_dense_tile){
                        unsigned act_val = rand() % 1000000;
                        if(unique_vals.find(act_val) == unique_vals.end()){
                            activations[i][j][k] = act_val;
                            unique_vals.insert(act_val);
                            k++;
                        }
                    }
                }
            }
        }

        printf("weights (diffprune)\t");
        if(activation_sparse == 1){
            printf("weights are dense\n");
        }
        else{
            printf("weights are sparse\n");
        }
        
        
        for(int i = 0; i < tiledK ; i++){
            for(int j = 0; j < tiledN; j++){

                //if weight is sparse and selected
                if(activation_sparse == 0){
                    int k = 0;
                    while(k != num_inst_per_sp_tile){
                        unsigned act_val = rand() % 1000000;
                        if(unique_vals.find(act_val) == unique_vals.end()){
                            //values.push_back(act_val);
                            //colIndices.push_back(j);
                            //nnz++;
                            weights[i][j][k] = act_val;
                            unique_vals.insert(act_val);
                            k++;
                            //printf("%u ", act_val);
                        }
                    }
                    //printf("\n");
                }

                //if weight is dense
                else if(activation_sparse == 1){
                    int k = 0;
                    while(k != num_inst_per_dense_tile){
                        unsigned act_val = rand() % 1000000;
                        if(unique_vals.find(act_val) == unique_vals.end()){
                            weights[i][j][k] = act_val;
                            unique_vals.insert(act_val);
                            k++;
                            //printf("%u ", act_val);
                        }
                    }
                }
            }
        }
        */
        
        indices.resize(nepochs);
        for(int epoch = 0; epoch < nepochs; epoch++){
            printf("epoch: %u\n", epoch);

            indices[epoch].resize(numTiledMult);

            int tiledM_idx = 0;
            int tiledK_idx = 0;
            int tiledN_idx = 0;
            for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){
                printf("tiledMultCount: %u\n", tiledMultCount);

                indices[epoch][tiledMultCount].resize(batch_size);//batch size = 2

                tiledM_idx = tiledMultCount/(tiledK * tiledN);
                tiledN_idx = (tiledMultCount - tiledM_idx * tiledK * tiledN)/tiledK;
                tiledK_idx = tiledMultCount % tiledK;

                printf("tiledM_idx: %u, tiledN_idx: %u, tiledK_idx: %u\n",
                tiledM_idx, tiledN_idx, tiledK_idx);

                for(int batch = 0; batch < (int)batch_size; batch++){
                    printf("batch: %u\n", batch);

                    //if activation sparse, weight dense
                    if(activation_sparse == 1){
                        //add first(or second) half of activations
                        for(int inst_cnt = 819*batch; inst_cnt < 819 + 819*batch; inst_cnt++){
                            indices[epoch][numTiledMult][batch].push_back(
                                activations[tiledM_idx][tiledK_idx][inst_cnt]
                            );
                        }
                        //add first half of weights
                        for(int inst_cnt = 0; inst_cnt < 2048; inst_cnt++){
                            indices[epoch][numTiledMult][batch].push_back(
                                weights[tiledK_idx][tiledN_idx][inst_cnt]
                            );
                        }
                        //add first(or second) half of activations(again)
                        for(int inst_cnt = 819*batch; inst_cnt < 819 + 819*batch; inst_cnt++){
                            indices[epoch][numTiledMult][batch].push_back(
                                activations[tiledM_idx][tiledK_idx][inst_cnt]
                            );
                        }
                        //add second half of weights
                        for(int inst_cnt = 2048; inst_cnt < 4096; inst_cnt++){
                            indices[epoch][numTiledMult][batch].push_back(
                                weights[tiledK_idx][tiledN_idx][inst_cnt]
                            );
                        }
                    }
                    //if activation dense, weight sparse
                    else{
                        //add first(second) half of activations
                        for(int inst_cnt = 2048*batch; inst_cnt < 2048 + 2048*batch; inst_cnt++){
                            indices[epoch][numTiledMult][batch].push_back(
                                activations[tiledM_idx][tiledK_idx][inst_cnt]
                            );
                        }
                        //add first half of weights
                        for(int inst_cnt = 0; inst_cnt < 42; inst_cnt++){
                            indices[epoch][numTiledMult][batch].push_back(
                                weights[tiledK_idx][tiledN_idx][inst_cnt]
                            );
                        }
                        //add first(second) half of activations (again)
                        for(int inst_cnt = 2048*batch; inst_cnt < 2048 + 2048*batch; inst_cnt++){
                            indices[epoch][numTiledMult][batch].push_back(
                                activations[tiledM_idx][tiledK_idx][inst_cnt]
                            );
                        }
                        //add second half of weights
                        for(int inst_cnt = 42; inst_cnt < 82; inst_cnt++){
                            indices[epoch][numTiledMult][batch].push_back(
                                activations[tiledM_idx][tiledK_idx][inst_cnt]
                            );
                        }
                    }
                }

            }
        }


        /*
        indices.resize(nepochs);
        for(int epoch = 0; epoch < nepochs; epoch++){
            printf("epoch: %u\n", epoch);
            
            indices[epoch].resize(numTiledMult);
            int tiledM_idx = 0;
            int tiledN_idx = 0;
            int tiledK_idx = 0;
            //indices[epoch].resize(M);
            for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){
                printf("tiledMultCount: %u\n", tiledMultCount);
                indices[epoch][tiledMultCount].resize(batch_size);//batch size = 2

                tiledM_idx = tiledMultCount/(tiledK * tiledN);
                tiledN_idx = (tiledMultCount - tiledM_idx * tiledK * tiledN)/tiledK;
                tiledK_idx = tiledMultCount % tiledK;

                printf("tiledM_idx: %u, tiledN_idx: %u, tiledK_idx: %u\n",
                tiledM_idx, tiledN_idx, tiledK_idx);

                //batch signals activation/weights
                //batch=0 (activations)         batch=1(weights)
                for(unsigned batch = 0; batch < batch_size; batch++){
                    printf("batch: %u\n",batch);
                    //if writing activations
                    if(batch == 0){
                        printf("writing activations\n");

                        //if activations are sparse
                        if(activation_sparse == 1){
                            for(int inst_cnt = 0; inst_cnt < num_inst_per_sp_tile; inst_cnt++){
                                indices[epoch][tiledMultCount][0].push_back(
                                    activations[tiledM_idx][tiledK_idx][inst_cnt]
                                );
                                num_inst_cnt++;
                            }
                        }
                        //if activations are dense
                        else{
                            for(int inst_cnt = 0; inst_cnt < num_inst_per_dense_tile; inst_cnt++){
                                indices[epoch][tiledMultCount][0].push_back(
                                    activations[tiledM_idx][tiledK_idx][inst_cnt]
                                );
                                num_inst_cnt++;
                            }
                        }
                    }
                    //if writing weights
                    else{
                        printf("writing weights\n");
                        //weights are dense
                        if(activation_sparse == 1){
                            for(int inst_cnt = 0; inst_cnt < num_inst_per_dense_tile; inst_cnt++){
                                indices[epoch][tiledMultCount][1].push_back(
                                    weights[tiledK_idx][tiledN_idx][inst_cnt]
                                );
                                num_inst_cnt++;
                            }
                        }

                        //weights are sparse
                        else{
                            for(int inst_cnt = 0; inst_cnt < num_inst_per_sp_tile; inst_cnt++){
                                indices[epoch][tiledMultCount][1].push_back(
                                    weights[tiledK_idx][tiledN_idx][inst_cnt]
                                );
                                num_inst_cnt++;
                            }
                        }
                    }
                    printf("num_inst_cnt: %u\n", num_inst_cnt);
                }
                
            }
        }
        */
        printf("total %u instructions\n", num_inst_cnt);
        batch_list.push_back(2);
        num_inst.push_back(num_inst_cnt);

    }   
    
    //delta activations, fixed to 20%
    else if(opcode == 3){
        weight_dim  = split(weight_dim_list, '-');
        act_dim     = split(act_dim_list, '-');

        int M = act_dim[0];
        int K = act_dim[1];
        int N = weight_dim[1];
        printf("M: %u, K: %u, N: %u\n", M, K, N);
        //[MxK] * [KxN] = [MxN] matrix mult
        //split into 16x16
        int tiledM = M/tile_size;
        int tiledK = K/tile_size;
        int tiledN = N/tile_size;

        int num_inst_cnt = 0;
        int data_num = 0;

        int numTiledMult = tiledM * tiledK * tiledN;
        printf("tiledM: %u,  tiledK: %u, tiledN: %u, numTiledMult: %u\n",
        tiledM, tiledK, tiledN, numTiledMult);

        //inst per dense tile
        int num_inst_per_dense_tile = tile_size * tile_size / 16;
        printf("num inst per dense tile: %u\n", num_inst_per_dense_tile);

        //inst per sparse tile. sparse matrix stored as CSR, so each instruction corresponds to 8 elements
        int num_inst_per_sp_tile    = (tile_size * tile_size * (density/100.)) / 8;
        printf("num inst per sp tile: %u\n", num_inst_per_sp_tile);

        //activations stored in CSR
        std::vector<std::vector<std::vector<int>>> activations(
            tiledM, std::vector<std::vector<int>>(
            tiledK, std::vector<int>(
            num_inst_per_sp_tile, 0)));

        std::vector<std::vector<std::vector<int>>> weights(
            tiledK, std::vector<std::vector<int>>(
            tiledN, std::vector<int>(
            num_inst_per_dense_tile, 0)));

        //instantiate delta activations
        for(int i = 0; i < tiledM; i++){
            for(int j = 0; j < tiledK; j++){
                for(int k = 0; k < num_inst_per_sp_tile; k++){
                    activations[i][j][k] = data_num;
                    data_num++;
                }
            }
        }

        //instantiate weights
        for(int i = 0; i < tiledK; i++){
            for(int j = 0; j < tiledN; j++){
                for(int k = 0; k < num_inst_per_dense_tile; k++){
                    weights[i][j][k] = data_num;
                    data_num++;
                }
            }
        }

        indices.resize(nepochs);
        for(int epoch = 0; epoch < nepochs; epoch++){
            printf("epoch: %u\n", epoch);
            indices[epoch].resize(numTiledMult);
            
            int tiledM_idx = 0;
            int tiledK_idx = 0; 
            int tiledN_idx = 0; 

            for(int tiledMultCount = 0; tiledMultCount < numTiledMult; tiledMultCount++){
                printf("tiledMultCount: %u\n", tiledMultCount);
                indices[epoch][tiledMultCount].resize(4);
                
                tiledM_idx = (tiledMultCount / tiledK) % tiledM;
                tiledN_idx = tiledMultCount / (tiledK * tiledM);
                tiledK_idx = tiledMultCount % tiledK;

                printf("tiledM_idx: %u, tiledN_idx: %u, tiledK_idx: %u\n",
                tiledM_idx, tiledN_idx, tiledK_idx);

                for(unsigned batch = 0; batch < 4; batch++){
                    printf("batch: %u\n",batch);
                    int act_inst_start = 0;
                    int act_inst_end = 0;
                    int wgt_inst_start = 0;
                    int wgt_inst_end = 0;
                    if(batch == 0){
                        act_inst_start = 0;
                        act_inst_end = num_inst_per_sp_tile/2;
                        wgt_inst_start = 0;
                        wgt_inst_end = num_inst_per_dense_tile/2;                        
                    }

                    if(batch == 1){
                        act_inst_start = 0;
                        act_inst_end = num_inst_per_sp_tile/2;
                        wgt_inst_start = num_inst_per_dense_tile/2;
                        wgt_inst_end = num_inst_per_dense_tile;
                    }

                    if(batch == 2){
                        act_inst_start = num_inst_per_sp_tile/2;
                        act_inst_end = num_inst_per_sp_tile;
                        wgt_inst_start = 0;
                        wgt_inst_end = num_inst_per_dense_tile/2;
                    }

                    if(batch == 3){
                        act_inst_start = num_inst_per_sp_tile/2;
                        act_inst_end = num_inst_per_sp_tile;
                        wgt_inst_start = num_inst_per_dense_tile/2;
                        wgt_inst_end = num_inst_per_dense_tile;
                    }

                    //write activation
                    for(int inst_cnt = act_inst_start; inst_cnt < act_inst_end; inst_cnt++){
                        indices[epoch][tiledMultCount][batch].push_back(
                            activations[tiledM_idx][tiledK_idx][inst_cnt]
                        );
                        num_inst_cnt += 1;
                    }
                    //write weight
                    for(int inst_cnt = wgt_inst_start; inst_cnt < wgt_inst_end; inst_cnt++){
                        indices[epoch][tiledMultCount][batch].push_back(
                            weights[tiledK_idx][tiledN_idx][inst_cnt]
                        );
                        num_inst_cnt += 1;
                    }

                    //printf("\n");
                    printf("inst added: %lu\n", indices[epoch][tiledMultCount][batch].size());
                    printf("accumulate total instructions: %u\n", num_inst_cnt);

                }

            }
        }
        printf("total %u instructions\n", num_inst_cnt);
        batch_list.push_back(4);
        num_inst.push_back(num_inst_cnt);//num inst per epoch

    }
    else{
        assert(false);
    }

    printf("setting address mapping\n");
    SetAddressMapping();
    printf("address mapping finished\n");

    if(opcode == 0){
        uint64_t total_data_size = accum_table_size[tables.size()] * data_size;
        uint64_t memory_size = (uint32_t)1 << (addr_bits - shift_bits - ch_bits);
        if (total_data_size >= memory_size) {
            cerr << "Error: table size exceeds memory size!" << endl;
            exit(1);
        }
        printf("total data size : %lu, memory size : %lu\n", total_data_size, memory_size);
    }
    else if(opcode == 1){
        uint64_t total_data_size = (act_dim[0]*act_dim[1] + weight_dim[0] * weight_dim[1]) * data_type_size;
        uint64_t memory_size = (uint32_t)1 << (addr_bits - shift_bits - ch_bits);
        printf("addr_bits: %u, shift_bits: %u, ch_bits: %u\n", addr_bits, shift_bits, ch_bits);
        printf("total data size : %lu, memory size : %lu\n", total_data_size, memory_size);
        if (total_data_size >= memory_size) {
            cerr << "Error: table size exceeds memory size!" << endl;
            exit(1);
        }

    }
}


Config::~Config()
{
    base_out.close();
    cxlpnm_out.close();
}

vector<unsigned> Config::split(string str, char delimiter)
{
    vector<unsigned> internal;
    stringstream ss(str);
    string temp;

    while (getline(ss, temp, delimiter)) {
        internal.push_back(stoi(temp));
    }
    return internal;
}

void Config::SetAddressMapping()
{
    // memory addresses are byte addressable, but each request comes with
    // multiple bytes because of bus width, and burst length
    int request_size_bytes = bus_width / 8 * BL;
    shift_bits = LogBase2(request_size_bytes);
    int col_low_bits = LogBase2(BL);
    int actual_col_bits = LogBase2(co_size) - col_low_bits;

    ch_bits = LogBase2(ch_size);
    ra_bits = LogBase2(ra_size);
    bg_bits = LogBase2(bg_size);
    ba_bits = LogBase2(ba_size);
    ro_bits = LogBase2(ro_size);
    co_bits = actual_col_bits;

    // has to strictly follow the order of chan, rank, bg, bank, row, col
    std::map<std::string, int> field_widths;
    field_widths["ch"] = LogBase2(ch_size);
    field_widths["ra"] = LogBase2(ra_size);
    field_widths["bg"] = LogBase2(bg_size);
    field_widths["ba"] = LogBase2(ba_size);
    field_widths["ro"] = LogBase2(ro_size);
    field_widths["co"] = actual_col_bits;

    if (address_mapping.size() != 12) {
        std::cerr << "Unknown address mapping (6 fields each 2 chars required)"
                  << std::endl;
    }

    // // get address mapping position fields from config
    // // each field must be 2 chars
    std::vector<std::string> fields;
    for (size_t i = 0; i < address_mapping.size(); i += 2) {
        std::string token = address_mapping.substr(i, 2);
        fields.push_back(token);
    }

    std::map<std::string, int> field_pos;
    int pos = 0;
    while (!fields.empty()) {
        auto token = fields.back();
        fields.pop_back();
        if (field_widths.find(token) == field_widths.end()) {
            std::cerr << "Unrecognized field: " << token << std::endl;
        }
        field_pos[token] = pos;
        pos += field_widths[token];
    }

    ch_pos = field_pos.at("ch");
    ra_pos = field_pos.at("ra");
    bg_pos = field_pos.at("bg");
    ba_pos = field_pos.at("ba");
    ro_pos = field_pos.at("ro");
    co_pos = field_pos.at("co");

    ch_mask = (1 << field_widths.at("ch")) - 1;
    ra_mask = (1 << field_widths.at("ra")) - 1;
    bg_mask = (1 << field_widths.at("bg")) - 1;
    ba_mask = (1 << field_widths.at("ba")) - 1;
    ro_mask = (1 << field_widths.at("ro")) - 1;
    co_mask = (1 << field_widths.at("co")) - 1;

    addr_bits = 
        ch_bits + ra_bits + bg_bits + ba_bits + ro_bits + co_bits + shift_bits;
}

int LogBase2(int power_of_two)
{
    int i = 0;
    while (power_of_two > 1) {
        power_of_two /= 2;
        i++;
    }
    return i;
}
