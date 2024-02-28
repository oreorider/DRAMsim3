#include "pnm.h"
#include "cxl.h"
#include <iostream>

using namespace dramsim3;

int main(){
    std::cout<<"testbench"<<std::endl;
    std::string config_dir = "/home/kimth/PNMLibrary/upstream/DRAMsim3/configs/DDR4_8Gb_x16_1866.ini";
    std::string output_dir = "/home/kimth/PNMLibrary/upstream/DRAMsim3/tb_output";
    int channel = 0;
    Config* cfg = new Config(config_dir, output_dir);
    Timing* tmg = new Timing(*cfg);
    Controller* ctrl = new Controller(channel, *cfg, *tmg);
    PNM* test_pnm = new PNM(channel, *cfg, *tmg, *ctrl);

    //Transaction* dummy_trans = new Transaction(
    //    
    //);
    int cycles = 0;
    while(1){
        if(cycles == 100){
            break;
        }
        cycles+=1;
        test_pnm->ClockTick();
    }
}