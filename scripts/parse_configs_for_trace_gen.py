#!/usr/bin/python3

import sys, re

ch_pattern = re.compile("channels = ([0-9]+)")
bg_pattern = re.compile("bankgroups = ([0-9]+)")
ba_pattern = re.compile("banks_per_group = ([0-9]+)")
ro_pattern = re.compile("rows = ([0-9]+)")
co_pattern = re.compile("columns = ([0-9]+)")
device_width_pattern = re.compile("device_width = ([0-9]+)")
BL_pattern = re.compile("BL = ([0-9]+)")

channel_size_pattern = re.compile("channel_size = ([0-9]+)")
bus_width_pattern = re.compile("bus_width = ([0-9]+)")
address_mapping_pattern = re.compile("address_mapping = ([a-z]+)")

ch = 0
ra = 0
bg = 0
ba = 0
ro = 0
co = 0

device_width = 0
BL = 0

channel_size = 0
bus_width = 0
address_mapping = "rochrababgco"

ch_list = "0"
ra_list = "0"

total_banks = 0
devices_per_rank = 0
megs_per_bank = 0
megs_per_rank = 0

configs = ""

config_file = open(sys.argv[1], "r")
config_line = config_file.readline()
while config_line:
    match = re.match(ch_pattern, config_line)
    if (match):
        ch = int(match.group(1))

    match = re.match(bg_pattern, config_line)
    if (match):
        bg = int(match.group(1))

    match = re.match(ba_pattern, config_line)
    if (match):
        ba = int(match.group(1))

    match = re.match(ro_pattern, config_line)
    if (match):
        ro = int(match.group(1))

    match = re.match(co_pattern, config_line)
    if (match):
        co = int(match.group(1))

    match = re.match(device_width_pattern, config_line)
    if (match):
        device_width = int(match.group(1))

    match = re.match(BL_pattern, config_line)
    if (match):
        BL = int(match.group(1))

    match = re.match(channel_size_pattern, config_line)
    if (match):
        channel_size = int(match.group(1))

    match = re.match(bus_width_pattern, config_line)
    if (match):
        bus_width = int(match.group(1))

    match = re.match(address_mapping_pattern, config_line)
    if (match):
        address_mapping = str(match.group(1))

    config_line = config_file.readline()

total_banks = bg * ba
devices_per_rank = bus_width / device_width
page_size = co * device_width / 8
megs_per_bank = page_size * (ro / 1024) / 1024
megs_per_rank = megs_per_bank * total_banks * devices_per_rank

ra = int(channel_size / megs_per_rank)

for i in range(1, ch):
    ch_list += "-" + str(i)

for i in range(1, ra):
    ra_list += "-" + str(i)

configs += " --channel " + ch_list
configs += " --rank " + ra_list
configs += " --bg_size " + str(bg)
configs += " --ba_size " + str(ba)
configs += " --ro_size " + str(ro)
configs += " --co_size " + str(co)
configs += " --bus_width " + str(bus_width)
configs += " --BL " + str(BL)
configs += " --address_mapping " + address_mapping

sys.stdout.write("%s" %(configs))
