#!/bin/bash

curr_ts=$(date +%Y%m%d_%H%M%S)

ID="1"
INSTALL="0"

CURR_DIR="${PWD}"
#QEMU_DIR="${CURR_DIR}/../PNMqemu"
QEMU_DIR="${CURR_DIR}/../PNMqemu"
QEMU_BUILD_DIR="${QEMU_DIR}/build"
QEMU_IMG="${QEMU_BUILD_DIR}/pnm-qemu.img"
ISO="${CURR_DIR}/../ubuntu-22.04.2-live-server-amd64.iso"

if [ "${INSTALL}" == "1" ]; then
    CDROM_PARAM="-cdrom ${ISO}"
else
    CDROM_PARAM=""
fi

# Use host CPU for setup and emulated CPU for tracing
CPU_PARAM="-cpu host -enable-kvm -smp 36"
#CPU_PARAM="-cpu max,+avx,+avx2 --accel tcg -smp 16"

PARAMS=""
PARAMS="${PARAMS} -drive file=${QEMU_IMG},if=virtio,format=raw"
PARAMS="${PARAMS} ${CDROM_PARAM}"
PARAMS="${PARAMS} ${CPU_PARAM}"
PARAMS="${PARAMS} -m 96G,slots=4,maxmem=128G"
PARAMS="${PARAMS} -boot d"
PARAMS="${PARAMS} -nographic"
PARAMS="${PARAMS} -monitor unix:qemu-monitor-sock,server,nowait"
PARAMS="${PARAMS} -serial mon:stdio"
PARAMS="${PARAMS} -net nic -net user,hostfwd=tcp::$((2028 + ${ID}))-:22"
PARAMS="${PARAMS} -vnc 127.0.0.1:$((1 + ${ID}))"
PARAMS="${PARAMS} -echr 0x2" # Escape letter for monitor mode: Ctrl + b
PARAMS="${PARAMS} "

sudo ${QEMU_BUILD_DIR}/qemu-system-x86_64 ${PARAMS}
