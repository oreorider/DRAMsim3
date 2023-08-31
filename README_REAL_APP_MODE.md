# Real Application Trace Mode

## Repositories

- [PNM QEMU](https://github.samsungds.net/SAITPublic/PNMqemu)
- [PNM Linux](https://github.samsungds.net/SAITPublic/PNMlinux)
- [PNM Library](https://github.samsungds.net/SAITPublic/PNMLibrary)
- [PNM PyTorch](https://github.samsungds.net/SAITPublic/PNMpytorch)
- [PNM DLRM](https://github.samsungds.net/SAITPublic/PNMdlrm)

## PNM QEMU

### Install Prerequisites

```
sudo apt install libglib2.0-dev libpng-dev
sudo apt install libslirp-dev
```

### Build PNM QEMU

```
cd <PNMqemu_path>
mkdir build
cd build
../configure --enable-trace-backends=simple --enable-avx2 --enable-avx512f
make
```

### Create QEMU Image

```
cd <PNMqemu_path>/build
./qemu-img create ./pnm-qemu.img 100G
```

### Get OS Image

Download Ubuntu 22.04 server image.
- https://ubuntu.com/

### Run PNM QEMU

```
cd <PNMSimulator_path>/scripts_real_app_mode
bash run_qemu.sh
```

Set `INSTALL="1"` for the first run (OS installation).
Change `CPU_PARAM`.

### Install Ubuntu by Using VNC

```
cd <PNMSimulator_path>/scripts_real_app_mode
bash vnc_qemu.sh
```

### Access QEMU with SSH

```
cd <PNMSimulator_path>/scripts_real_app_mode
bash ssh_qemu.sh
```

Change `HOST_IP` and `USER`.

### Install Packages on QEMU

```
sudo apt update
sudo apt upgrade
sudo apt install build-essential
```

## Prerequisites for PNM

Run the following commands on QEMU.

```
# PNM library dependencies
$ sudo apt install ndctl libndctl-dev daxctl libdaxctl-dev numactl
 
# Linux kernel build dependencies
$ sudo apt install fakeroot ncurses-dev xz-utils libssl-dev bc flex libelf-dev bison rsync kmod cpio dpkg-dev ccache
 
# Pytorch build dependencies
$ sudo apt install python3-pip libopenblas-dev
 
# Pytorch python dependencies
$ pip3 install -U protobuf==3.13 future yappi numpy==1.23.5 pydot typing_extensions ninja onnx scikit-learn tqdm tensorboard
```

## PNM Linux

Install PNM Linux kernel on QEMU.

```
cd <PNMlinux_path>
rm -f .config; cp -v config-pnm .config
rm -fr debian/ vmlinux-gdb.py
make -j `getconf _NPROCESSORS_ONLN` bindeb-pkg
```

```
...
*
* Restart config...
*
*
* DAX: direct access to differentiated memory
*
DAX: direct access to differentiated memory (DAX) [Y/?] y
  Device DAX: direct access mapping device (DEV_DAX) [M/n/y/?] m
    PMEM DAX: direct access to persistent memory (DEV_DAX_PMEM) [M/n/?] m
  HMEM DAX: direct access to 'specific purpose' memory (DEV_DAX_HMEM) [M/n/y/?] m
  DEV_SLS support (DEV_SLS) [Y/n/?] y
  KMEM DAX: volatile-use of persistent memory (DEV_DAX_KMEM) [M/n/?] m
  A base address of SLS range (DEV_SLS_BASE_ADDR) [4] (NEW) 4
  A scale of SLS memory range (DEV_SLS_MEMORY_SCALE) [1] (NEW) 0
  Choose PNM SLS type
  > 1. AXDIMM (DEV_SLS_AXDIMM) (NEW)
    2. CXL (DEV_SLS_CXL) (NEW)
  choice[1-2?]: 2
*
* Device Drivers
*
Parallel port LCD/Keypad Panel support (OLD OPTION) (PANEL) [M/n/?] m
Data acquisition support (comedi) (COMEDI) [M/n/y/?] m
  Comedi debugging (COMEDI_DEBUG) [N/y/?] n
  Comedi default initial asynchronous buffer size in KiB (COMEDI_DEFAULT_BUF_SIZE_KB) [2048] 2048
  Comedi default maximum asynchronous buffer size in KiB (COMEDI_DEFAULT_BUF_MAXSIZE_KB) [20480] 20480
  Standalone 8255 support (COMEDI_8255_SA) [M/n/?] m
  Comedi kcomedilib (COMEDI_KCOMEDILIB) [M/?] m
  Comedi unit tests (COMEDI_TESTS) [N/m/?] n
Primary to Sideband (P2SB) bridge access support (P2SB) [Y/?] y
Database Accelerator (IMDB) [Y/n/?] y
SLS Resource Manager (SLS_RESOURCE) [M/n/y/?] m
CXL-zSwap Devices Support (PNM_ZSWAP) [N/y/?] (NEW) N
*
* IMDB Resource Manager
*
IMDB Resource Manager (IMDB_RESOURCE) [M/n/y/?] m
  A scale of IMDB memory range (IMDB_MEMORY_SCALE) [0] (NEW) 0
...
```

```
./install_debs.sh
sudo reboot
```

```
sudo vi /etc/default/grub
====
GRUB_CMDLINE_LINUX="memmap=64G!4G"
====
 
sudo update-grub
sudo reboot
```

## PNM Library

Install PNM library on QEMU.

```
sudo apt install clang cmake ninja-build
```

```
cd <PNMLibrary_path>
./scripts/build.sh -r -cb --psim -j 32
```

``` 
sudo modprobe sls_resource device=CXL
sudo ./build/tools/pnm_ctl setup-dax
```

## PNM PyTorch

Install PNM PyTorch on QEMU.

```
cd <PNMpytorch_path>
git submodule init
git submodule update --init --recursive
rm -fr build
CMAKE_C_COMPILER_LAUNCHER=ccache CMAKE_CXX_COMPILER_LAUNCHER=ccache CFLAGS="-g -fno-omit-frame-pointer" BLAS=OpenBLAS BUILD_TEST=0 BUILD_CAFFE2=1 USE_CUDA=0 USE_PNM=1 PNM_INSTALL_DIR=<PNMLibrary_path>/build/ python3 ./setup.py install --user
```

## PNM DLRM

Run DLRM on QEMU.

### Install Python Packages

```
pip3 install -U sympy filelock networkx
```

### Generate Tables

```
cd <PNMLibrary_path>/build/tools
export PATH=$PATH:`pwd`
cd ../../
./scripts/create_test_dataset.sh --dlrm --root test_tables/
```

### Run

```
sudo modprobe sls_resource device=CXL
sudo <PNMLibrary_path>/build/tools/pnm_ctl setup-dax
```

#### Console #1 (QEMU Guest)

```
export GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2_Usable,-AVX_Fast_Unaligned_Load # to make memcpy trace stable
./run_test.sh --weights-path <path_to_table>/DLRM_FLOAT/embedded.bin --use-pnm simple --sls_device_interface CXL --num_batches 1 --user-triggers-inference
... wait until DLRM prompts "Waiting for users trigger, please press Enter to start inference..."
... enable tracing in Console #2
... press "Enter"
```

#### Console #2 (Host)

```
sudo socat - unix-connect:qemu-monitor-sock
(qemu) pnm-trace-file set pnm_trace-1.txt
... wait until DLRM prompts "Waiting for users trigger, please press Enter to start inference..." in Console #1
(qemu) pnm-trace-file on
pnm-trace-file on
        [PNM] trace_start
... wait until DLRM completes execution
(qemu) pnm-trace-file off
pnm-trace-file off
        [PNM] trace_end (pnm_trace-1.txt file is generated!)
```

## Trace Converter

```
cd <PNMSimulator_path>/scripts_real_app_mode
python3 trace_conv.py pnm_trace-1.txt > converted.trc
```

## Run with Real Application Trace

```
cd <PNMSimulator_path>/scripts_real_app_mode
bash run_pnm_sim_real_app_mode.sh
```
