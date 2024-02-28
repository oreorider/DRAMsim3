#!/bin/bash

loc_check="$(basename ${PWD})"
if [ "${loc_check}" != "scripts" ]; then
    echo "[warning] Run this script in <PNMSimulator_path>/scripts directory!"
    exit
fi

# Configuration file path
config_file="../configs/DDR4_8Gb_x16_3200.ini"

curr_ts="$(date +%Y%m%d_%H%M%S)"
res_dir="../results/res_blocksparse_${curr_ts}"
mkdir -p ${res_dir}/cxlpnm
mkdir -p ${res_dir}/base

# Run with CXL-PNM trace
../pnm_sim/build/pnmsim ${config_file} \
    -t blocksparse_traces/test_cxlpnm.trc \
    -c 0 \
    > ${res_dir}/cxlpnm/pnm_sim_cxlpnm.log 2>&1

# DEBUG WITH GDB
#gdb -q -ex=r --args ../pnm_sim/build/pnmsim \
#    ${config_file} \
#    -t blocksparse_traces/test_cxlpnm.trc \
#    -c 0 \

mv pnm_*.trace ${res_dir}/cxlpnm
mv dramsim3* ${res_dir}/cxlpnm

# Run with base trace
../pnm_sim/build/pnmsim ${config_file} \
    -t blocksparse_traces/test_base.trc \
    -c 0 \
    > ${res_dir}/base/pnm_sim_base.log 2>&1
mv pnm_*.trace ${res_dir}/base
mv dramsim3* ${res_dir}/base
