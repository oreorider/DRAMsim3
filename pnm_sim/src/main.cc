#include <iostream>
#include "./../ext/headers/args.hxx"
#include "cxl.h"

using namespace dramsim3;

int main(int argc, const char **argv) {
    args::ArgumentParser parser(
        "CXL-PNM SIMULATOR using Dramsim3.",
        "Examples: \n."
        "./build/dramsim3main configs/DDR4_8Gb_x8_3200.ini -c 100 -t sample_trace.txt\n");
    args::HelpFlag help(parser, "help", "Display the help menu", {'h', "help"});
    args::ValueFlag<uint64_t> num_cycles_arg(parser, "num_cycles",
                                             "Number of cycles to simulate",
                                             {'c', "cycles"}, 100000);
    args::ValueFlag<std::string> output_dir_arg(
        parser, "output_dir", "Output directory for stats files",
        {'o', "output-dir"}, ".");
    args::ValueFlag<std::string> trace_file_arg(
        parser, "trace",
        "Trace file, CXL-PNM only support trace mode (mandatory)",
        {'t', "trace"});
    args::Positional<std::string> config_arg(
        parser, "config", "The config file name (mandatory)");

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help) {
        std::cout << parser;
        return 0;
    } catch (args::ParseError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    std::string config_file = args::get(config_arg);
    if (config_file.empty()) {
        std::cerr << parser;
        return 1;
    }

    uint64_t cycles = args::get(num_cycles_arg);
    std::string output_dir = args::get(output_dir_arg);
    std::string trace_file = args::get(trace_file_arg);

    CXL *cxl;
    if (!trace_file.empty()) {
        cxl = new TraceBasedCXL(config_file, output_dir, trace_file);
    } else {
        std::cerr << "Trace file does not exist" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }

    uint64_t clk = 0;
    bool done = false;
    while ((cycles && (clk < cycles)) || ((!cycles) && (!done))) {
        done = cxl->ClockTick();
        clk++;
    }
    cxl->PrintStats();

    delete cxl;

    return 0;
}
