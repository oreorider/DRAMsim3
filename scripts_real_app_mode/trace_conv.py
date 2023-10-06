#!/usr/bin/python3

import sys, re

CLK = 0
CLK_INTERVAL = 4

CH_NUM = 2

BUF_SIZE = 0x3FFC0
INST_SIZE = 0x40

FS_CH_BIT = 34
FS_CH_MASK = 0x400000000
FS_ADDR_MASK = 0x3FFFFFFFF
QEMU_BASE_ADDR = 0x100000000

FS_INST_BASE = 0x3E0000000
FS_CFGR_BASE = 0x3E1000000
FS_PSUM_BASE = 0x3E4000000


PS_INST_BASE = [0x0, 0x40000]
PS_PSUM_BASE = [0x80C00, 0xc0C00]
PS_CFGR_BASE = [0x80400, 0xc0400]
PS_DONE_BASE = [0x0, 0x40000]

# [ch][addr_space]
inst_addr_space = [[-1 for i in range(BUF_SIZE)] for j in range(CH_NUM)]

fs_trace_w_p = re.compile("WRITE addr: ([0-9a-z]+), size: ([0-9]+), val: ([0-9a-z\(\)]+)")
fs_trace_r_p = re.compile("READ addr: ([0-9a-z]+), size: ([0-9]+)")

# set 0xcafe
is_set = [False for i in range(CH_NUM)]

# simulator sets when it's done 0xfffffff
is_sim_done = [False for i in range(CH_NUM)]

# for debug
cnt_write_inst = [0 for i in range(CH_NUM)]
cnt_read_psum = [0 for i in range(CH_NUM)]
cnt_write_cfgr = [0 for i in range(CH_NUM)]

# address_mapping = rochrababgco
ps_ch_bits = 1
ps_ra_bits = 1
ps_bg_bits = 2
ps_ba_bits = 2
ps_ro_bits = 17
ps_co_bits = 10
shift_bits = 3

ps_ch_pos = 18
ps_ra_pos = 17
ps_bg_pos = 13
ps_ba_pos = 15
ps_ro_pos = 19
ps_co_pos = 3

inst_list = [[], []]
trace_list = []

class Address:
    ch = -1
    ra = -1
    bg = -1
    ba = -1
    ro = -1
    co = -1

def trace_conv(fs_addr, fs_size, fs_val, ch, is_write):
    global is_set
    global is_sim_done
    global CLK
    global trace_list

    if (is_write):
        if fs_addr >= FS_INST_BASE  and fs_addr < FS_INST_BASE + BUF_SIZE:
            conv_inst(ch, fs_addr, fs_size, fs_val)
            if is_set[ch] == True:
                trace_list.append(hex(PS_CFGR_BASE[ch]) + " MFENCE " + str(CLK))
                CLK += CLK_INTERVAL
            is_set[ch] = False
        elif fs_addr == FS_CFGR_BASE + 0x40 and fs_val != 0:
            conv_cfgr(ch, fs_addr)
        elif fs_addr == FS_CFGR_BASE + 0x444:
            is_sim_done[ch] = True if fs_val == 0xffffffff else False
        """
        else:
            sys.stdout.write("[debug] Don't care\n")
        """
    elif (is_sim_done[ch]):
        if fs_addr >= FS_PSUM_BASE and fs_addr < FS_PSUM_BASE + BUF_SIZE:
            conv_psum(ch, fs_addr)
        """
        else:
            sys.stdout.write("[debug] Don't care\n")
        """

def parse_qemu_trace(match, is_write):
    fs_addr = int(match.group(1), 0) - QEMU_BASE_ADDR
    fs_size = int(match.group(2))

    ch = (fs_addr & FS_CH_MASK) >> FS_CH_BIT
    fs_addr = fs_addr & FS_ADDR_MASK

    fs_val = 0
    if (is_write):
        fs_val = match.group(3)
        if fs_val == "(nil)":
            fs_val = 0
        else:
            fs_val = int(fs_val, 0)

    return fs_addr, fs_size, fs_val, ch

def conv_inst(ch, fs_addr, fs_size, fs_val):
    global CLK
    global cnt_write_inst
    global inst_list
    addr = fs_addr - FS_INST_BASE
    inst_id = int(addr / INST_SIZE)
    inst_str = ""

    for i in range(fs_size):
        inst_addr_space[ch][addr + i] = get_byte(fs_val, i)

    # check 64 bytes instructions
    if (not is_inst_full(ch, inst_id)):
        return

    ps_addr = get_inst_addr(ch, inst_id)
    inst_str += str(hex(ps_addr)) + " WRITE " + str(CLK)
    cnt_write_inst[ch] += 1

    for data_idx in range(8):
        ps_data = get_inst_data(ch, inst_id, data_idx)
        inst_str += " " + str(hex(ps_data))
    CLK += CLK_INTERVAL

    inst_list[ch].append(inst_str)

    for i in range(inst_id * 64, inst_id * 64 + INST_SIZE):
        inst_addr_space[ch][i] = -1

def get_byte(val, i):
    return (val & (0xff << (i * 8))) >> (i * 8)

def is_inst_full(ch, inst_id):
    inst_full = True

    for i in range(inst_id * 64, (inst_id * 64) + 64):
        if inst_addr_space[ch][i] == -1:
            inst_full = False
            break

    return inst_full

def get_inst_addr(ch, inst_id):
    tmp_addr = inst_id * 8 * 8
    tmp_addr_lower = tmp_addr & 0b11111111111111111
    tmp_addr_upper = tmp_addr - tmp_addr_lower
    ps_addr = (tmp_addr_upper << ps_ch_bits) + tmp_addr_lower + PS_INST_BASE[ch]

    return ps_addr

def get_inst_data(ch, inst_id, data_idx):
    fs_data = 0
    for j in range(8):
        fs_data += inst_addr_space[ch][inst_id * INST_SIZE + data_idx * 8 + j] << j*8

    fs_data_addr = get_fs_data_addr(fs_data)
    fs_data_etc = fs_data - fs_data_addr
    ps_data_addr = conv_data_addr(fs_data_addr)
    ps_data = fs_data_etc + ps_data_addr

    return ps_data

def get_fs_data_addr(fs_data):
    return fs_data & 0x7ffffffff

def conv_data_addr(fs_data_addr):
    addr = Address()

    addr.ch = 0

    addr.ra = (fs_data_addr >> 34) & 0x1

    addr.bg = ((fs_data_addr >> 13) & 0x1) << 1
    addr.bg += (fs_data_addr >> 12) & 0x1

    addr.ba = (fs_data_addr >> 19) & 0x3
    
    addr.ro = ((fs_data_addr >> 29) & 0x1f) << 12
    addr.ro += ((fs_data_addr >> 21) & 0x7f) << 5
    addr.ro += ((fs_data_addr >> 28) & 0x1) << 4
    addr.ro += ((fs_data_addr >> 11) & 0x1) << 3
    addr.ro += ((fs_data_addr >> 8) & 0x7)

    addr.co = ((fs_data_addr >> 14) & 0x1f) << 5
    addr.co += (fs_data_addr >> 3) & 0x1f

    ps_data_addr = addr.ro << ps_ch_pos
    ps_data_addr = addr.ra << ps_ra_pos
    ps_data_addr = addr.ba << ps_ba_pos
    ps_data_addr = addr.bg << ps_bg_pos
    ps_data_addr = addr.co << ps_co_pos

    return ps_data_addr

def conv_psum(ch, fs_addr):
    global CLK
    global cnt_read_psum
    global trace_list

    if ((fs_addr % 0x40) != 0): return

    if (fs_addr == FS_PSUM_BASE):
        trace_list.append(hex(PS_DONE_BASE[ch]) + " DONE " + str(CLK))
        CLK += CLK_INTERVAL

    ps_addr = fs_addr - FS_PSUM_BASE + PS_PSUM_BASE[ch]
    trace_list.append(hex(ps_addr) + " READ " + str(CLK))
    cnt_read_psum[ch] += 1
    CLK += CLK_INTERVAL

def conv_cfgr(ch, fs_addr):
    global CLK
    global cnt_write_cfgr
    global is_set
    global inst_list
    global trace_list

    last_addr = -1
    last_idx = -1
    for i in range(len(inst_list[ch])):
        inst_split = inst_list[ch][i].split()
        inst_addr = int(inst_split[0], 0)
        if inst_addr > last_addr:
            last_addr = inst_addr
            last_idx = i

    last_inst = inst_list[ch][last_idx]
    last_inst_split = last_inst.rsplit(" ", 1)
    ended = hex(int(last_inst_split[1], 0) | (1 << 48))
    inst_list[ch][last_idx] = last_inst_split[0] + " " + ended

    for inst in inst_list[ch]:
        trace_list.append(inst)

    inst_list[ch].clear()

    trace_list.append(hex(PS_CFGR_BASE[ch]) + " SFENCE " + str(CLK))
    CLK += CLK_INTERVAL
    trace_list.append(hex(PS_CFGR_BASE[ch]) + " WRITE " + str(CLK) + " 0xcafe")
    CLK += CLK_INTERVAL
    if fs_addr == (FS_CFGR_BASE + 0x40):
        cnt_write_cfgr[ch] += 1
        is_set[ch] = True

if __name__ == "__main__":
    trace_file = open(sys.argv[1], "r")

    trace_line = trace_file.readline()

    while trace_line:
        is_write = False

        match = re.match(fs_trace_w_p, trace_line)
        if match:
            is_write = True
            fs_addr, fs_size, fs_val, ch = parse_qemu_trace(match, is_write)

        else:
            match = re.match(fs_trace_r_p, trace_line)
            if match:
                fs_addr, fs_size, fs_val, ch = parse_qemu_trace(match, is_write)
            else:
                sys.stdout.write("[ERROR] Undefined trace line\n")

        trace_conv(fs_addr, fs_size, fs_val, ch, is_write)

        trace_line = trace_file.readline()

    trace_list.sort(key=lambda x: int(x.split()[2]))
    for i in range(len(trace_list)):
        print(trace_list[i])

    # for debug
    #for i in range(CH_NUM):
    #    sys.stdout.write("CH%d INST %d PSUM %d CFGR %d\n" %(i, cnt_write_inst[i], cnt_read_psum[i], cnt_write_cfgr[i]))

