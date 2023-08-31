#!/bin/bash

loc_check="$(basename ${PWD})"
if [ "${loc_check}" != "scripts_real_app_mode" ]; then
    echo "[warning] Run this script in <PNMSimulator_path>/scripts directory!"
    exit
fi

# Configuration file path
config_file="../configs/real_app_mode.ini"

curr_ts="$(date +%Y%m%d_%H%M%S)"
res_dir="../results/res_${curr_ts}"
mkdir -p ${res_dir}/cxlpnm

# Run with CXL-PNM trace
../pnm_sim/build/pnmsim ${config_file} \
    -t converted.trc \
    -c 0 \
    > ${res_dir}/cxlpnm/pnm_sim_cxlpnm.log 2>&1
mv pnm_*.trace ${res_dir}/cxlpnm
mv dramsim3* ${res_dir}/cxlpnm
