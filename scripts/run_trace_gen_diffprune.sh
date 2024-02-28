#!/bin/bash

loc_check="$(basename ${PWD})"
if [ "${loc_check}" != "scripts" ]; then
    echo "[warning] Run this script in <PNMSimulator_path>/scripts directory!"
    exit
fi

# Configuration file path
config_file="../configs/DDR4_8Gb_x16_3200.ini"

configs="$(python3 parse_configs_for_trace_gen.py ${config_file})"

# Parameters
params="
    --opcode 2
    --nepochs 1
    --batch_size 2
    --embedding_table 1000000-1000000
    --sparse_feature_size 16
    --data_type_size 4
    --pooling_type 0
    --default_interval 4
    --miss_ratio 100
    --base_only false
    --file_name test
    --act_dim 256-512
    --weight_dim 512-512
    --tile_size 256
    --blk_sparse_dim 1
    --density 18.875
    --activation_sparse 1
"

#block sparse
#params="
#    --opcode 2
#    --nepochs 1
#    --batch_size 2
#    --embedding_table 1000000-1000000
#    --sparse_feature_size 16
#    --data_type_size 4
#    --pooling_type 0
#    --default_interval 4
#    --miss_ratio 100
#    --base_only false
#    --file_name test
#    --act_dim 256-512
#    --weight_dim 512-512
#    --blk_sparse_dim 32
#    --density 1.0
#"


../trace_gen/trace_gen ${configs} ${params} > trace_gen_diffprune.log 2>&1


mkdir -p diffprune_traces
mv *.trc diffprune_traces