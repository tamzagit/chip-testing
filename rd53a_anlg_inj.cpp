#include <iostream>
#include <chrono>
#include <unistd.h>
#include "SpecController.h"
#include "Rd53a.h"

#define EN_RX2 0x1
#define EN_RX1 0x2
#define EN_RX4 0x4
#define EN_RX3 0x8
#define EN_RX6 0x10
#define EN_RX5 0x20
#define EN_RX8 0x40
#define EN_RX7 0x80

#define EN_RX10 0x100
#define EN_RX9 0x200
#define EN_RX12 0x400
#define EN_RX11 0x800
#define EN_RX14 0x1000
#define EN_RX13 0x2000
#define EN_RX16 0x4000
#define EN_RX15 0x8000

#define EN_RX18 0x10000
#define EN_RX17 0x20000
#define EN_RX20 0x40000
#define EN_RX19 0x80000
#define EN_RX22 0x100000
#define EN_RX21 0x200000
#define EN_RX24 0x400000
#define EN_RX23 0x800000

void decode(RawData *data) {
    if (data != NULL) {
        for (unsigned i=0; i<data->words; i++) {
            if (data->buf[i] != 0xFFFFFFFF) {
                if ((data->buf[i] >> 25) & 0x1) {
                    unsigned l1id = 0x1F & (data->buf[i] >> 20);
                    unsigned l1tag = 0x1F & (data->buf[i] >> 15);
                    unsigned bcid = 0x7FFF & data->buf[i];
                    std::cout << "[Header] : L1ID(" << l1id << ") L1Tag(" << l1tag << ") BCID(" <<  bcid << ")" << std::endl;
                } else {
                    unsigned core_col = 0x3F & (data->buf[i] >> 26);
                    unsigned core_row = 0x3F & (data->buf[i] >> 17);
                    unsigned parity = 0x1 & (data->buf[i] >> 16);
                    unsigned tot0 = 0xF & (data->buf[i] >> 0);
                    unsigned tot1 = 0xF & (data->buf[i] >> 4);
                    unsigned tot2 = 0xF & (data->buf[i] >> 8);
                    unsigned tot3 = 0xF & (data->buf[i] >> 12);
                    std::cout << "[Data] : COL(" << core_col << ") ROW(" << core_row << ") PAR(" << parity
                        << ") TOT(" << tot3 << "," << tot2 << "," << tot1 << "," << tot0 << ")" << std::endl;
                }
            }
        }
    }
}

void step_I_comp(int VcalHigh, int VcalMed, Rd53a *fe){
    int inputDiff;
    int VcalCenter = 2048;
    while(true){
        std::cout<<std::endl<<"Enter the injection voltage in DAC steps: ";
        std::cin>>inputDiff;
        
        VcalHigh = VcalCenter + inputDiff/2;
        VcalMed = VcalCenter - inputDiff/2;  
        fe->writeRegister(&Rd53a::InjVcalHigh, VcalHigh);
        fe->writeRegister(&Rd53a::InjVcalMed, VcalMed);
        for(int n=0;  n<512*10; n++){
            fe->cal(0, 0, 0, 4, 0, 0); //Bring cal_edge high
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
            fe->cal(0, 1, 0, 0, 0, 0); //Arm cal_edge to low
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
            fe->writeRegister(&Rd53a::LinComp, (int)n/10);
            std::cout << "\r" << (int)n/10 << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(3)); 
        }
    }
}

void step_VFF(int VcalHigh, int VcalMed, Rd53a *fe){
    int Vdiff;
    int VFFLimit;
    int VFF = 1;
    int VcalCenter = 2048;
    VcalHigh = VcalCenter;
    VcalMed = VcalCenter;
    bool validVFF;
    int stepSize;
    int direction = 1;
    int repeats;
    while(true){
        std::cout<<std::endl<<"Enter the injection in DAC steps: ";
        std::cin>>Vdiff;
        fe->writeRegister(&Rd53a::InjVcalHigh, VcalHigh+Vdiff/2);
        fe->writeRegister(&Rd53a::InjVcalMed, VcalMed-Vdiff/2);
        
        validVFF = false;
        while (not validVFF){
            std::cout<<std::endl<<"Enter the maximum VFF in DAC steps: ";
            std::cin>>VFFLimit;
            validVFF = VFFLimit <= 1023 && 1 <= VFFLimit;
            if (not VFFLimit){
                std::cout << "Valid range for VFF is 1 to 1023.";
            }     
        }  
        std::cout<<std::endl<<"Enter the VFF stepsize in DAC steps: ";
        std::cin>>stepSize;
        while (true){
            VFF += direction*stepSize;
            if (VFF > 1023){VFF = 1023;}
            if (VFF < 1){VFF = 1;}    
            if(1>=VFF){
                direction = 1;
                break;
            }
            if(VFF >= VFFLimit){
                direction = -1;
                fe->writeRegister(&Rd53a::DiffVff, VFF); 
                repeats = 40*2;
            }else{
                fe->writeRegister(&Rd53a::DiffVff, VFF); 
                repeats = 40;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(stepSize*2)); 
            std::cout << "\r" << "VFF: " << VFF << std::flush;
            
            for(int n=0; n<repeats; n++){
                fe->cal(0, 0, 0, 4, 0, 0); //Bring cal_edge high
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
                fe->cal(0, 1, 0, 0, 0, 0); //Arm cal_edge to low
                std::this_thread::sleep_for(std::chrono::milliseconds(4)); 
            }
            
        }

        
    }
}

void const_V_cal(int VcalHigh, int VcalMed, Rd53a *fe){
    //Constant V_Cal injection
    int Vdiff;
    int VcalCenter = 2048;
    bool validDiff;
    while(true){
        validDiff = false;
        while (not validDiff){
            std::cout<<std::endl<<"Enter your desired V_cal_diff in DAC steps: ";
            std::cin>>Vdiff;
            validDiff = Vdiff <= 4095 && 0 <= Vdiff;
            if (not validDiff){
                std::cout << "Valid range for V_cal_diff is 1 to 4095.";
            }     
        }  
        VcalHigh = VcalCenter + Vdiff/2;
        VcalMed = VcalCenter - Vdiff/2;
        fe->writeRegister(&Rd53a::InjVcalHigh, VcalHigh);
        fe->writeRegister(&Rd53a::InjVcalMed, VcalMed);
        for(int n=0;  n<10000; n++){
            fe->cal(0, 0, 0, 4, 0, 0); //Bring cal_edge high
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
            fe->cal(0, 1, 0, 0, 0, 0); //Arm cal_edge to low
            std::this_thread::sleep_for(std::chrono::milliseconds(4)); 

        }
    }
}

void const_V_cal_and_VFF(int VcalHigh, int VcalMed, Rd53a *fe){
    //Constant V_Cal injection
    int Vdiff;
    int VFF;
    int VcalCenter = 2048;
    bool validDiff;
    bool validVFF;
    while(true){
        validDiff = false;
        while (not validDiff){
            std::cout<<std::endl<<"Enter your desired V_cal_diff in DAC steps: ";
            std::cin>>Vdiff;
            validDiff = Vdiff <= 4095 && 0 <= Vdiff;
            if (not validDiff){
                std::cout << "Valid range for V_cal_diff is 1 to 4095.";
            }     
        }  
        validVFF = false;
        while (not validVFF){
            std::cout<<std::endl<<"Enter your desired VFF in DAC steps: ";
            std::cin>>VFF;
            validVFF = VFF <= 1023 && 0 <= Vdiff;
            if (not validVFF){
                std::cout << "Valid range for VFF is 0 to 1023.";
            }     
        }  
        VcalHigh = VcalCenter + Vdiff/2;
        VcalMed = VcalCenter - Vdiff/2;
        fe->writeRegister(&Rd53a::InjVcalHigh, VcalHigh);
        fe->writeRegister(&Rd53a::InjVcalMed, VcalMed);
        fe->writeRegister(&Rd53a::DiffVff, VFF); 
        for(int n=0;  n<10000; n++){
            fe->cal(0, 0, 0, 4, 0, 0); //Bring cal_edge high
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
            fe->cal(0, 1, 0, 0, 0, 0); //Arm cal_edge to low
            std::this_thread::sleep_for(std::chrono::milliseconds(4)); 

        }
    }
}

void step_Vcal_mid(int VcalHigh, int VcalMed, Rd53a *fe){
    
}

void step_rows(int fe_flavor, int gain, int clmn, int rowInit, int rowEnd, bool at_once, Rd53a *fe){
    for (int k = 0; k<2; k++){
        std::cout << "k = " << k << std::endl;
        for (int n = rowInit; n<=rowEnd; n++){
            if (not at_once){
                std::cout << "Row " << n << std::endl;
            }
            //enable pixel
            fe->writeRegister(&Rd53a::PixRegionCol, clmn);
            fe->writeRegister(&Rd53a::PixRegionRow, n);
            std::cout << "Enabling pixel" << std::endl;
            if (gain || fe_flavor != 1){
                if (k==0){
                    fe->writeRegister(&Rd53a::PixPortal, 0x0007); 
                }else{
                    fe->writeRegister(&Rd53a::PixPortal, 0x0700);            
                }
            }else{
                if (k==0){
                    fe->writeRegister(&Rd53a::PixPortal, 0x007F); 
                }else{
                    fe->writeRegister(&Rd53a::PixPortal, 0x7F00);            
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));  
            std::cout << "Injecting." << std::endl;
            if (not at_once){
                for (int l=0; l<8; l++){
                    //inject
                    fe->cal(0, 0, 0, 4, 0, 0); //Bring cal_edge high
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
                    fe->cal(0, 1, 0, 0, 0, 0); //Arm cal_edge to low
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));  
                //disable pixel
                for (int i=0; i<4; i++){
                    std::cout << "Disabling pixel." << std::endl;
                    fe->writeRegister(&Rd53a::PixPortal, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));  
                    std::cout << "Enabling pixel." << std::endl;
                    if (gain || fe_flavor != 1){
                        if (k==0){
                            fe->writeRegister(&Rd53a::PixPortal, 0x0007); 
                        }else{
                            fe->writeRegister(&Rd53a::PixPortal, 0x0700);            
                        }
                    }else{
                        if (k==0){
                            fe->writeRegister(&Rd53a::PixPortal, 0x007F); 
                        }else{
                            fe->writeRegister(&Rd53a::PixPortal, 0x7F00);            
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));  
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
        }
        if (at_once){
            for (int l=0; l<8; l++){
                //inject
                fe->cal(0, 0, 0, 4, 0, 0); //Bring cal_edge high
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
                fe->cal(0, 1, 0, 0, 0, 0); //Arm cal_edge to low
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
            }
            //disable pixels
            for (int n = rowInit; n<=rowEnd; n++){
                fe->writeRegister(&Rd53a::PixRegionCol, clmn);
                fe->writeRegister(&Rd53a::PixRegionRow, n);
                fe->writeRegister(&Rd53a::PixPortal, 0); 
                std::this_thread::sleep_for(std::chrono::microseconds(10));  
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
    }
}

void step_clmns(int fe_flavor, int gain, Rd53a *fe){
    //Constant V_Cal = 0 injection into individual coloumns
    int clmnInit;
    int clmnEnd;
    int rowInit;
    int rowEnd;
    fe->writeRegister(&Rd53a::InjVcalHigh, 2048); //2304 for Vdiff = 512
    fe->writeRegister(&Rd53a::InjVcalMed, 1048); //1792 for Vdiff = 512
    while(true){
        std::cout<<std::endl<<"Enter the first coloumn to inject into: ";
        std::cin>>clmnInit;
        std::cout<<std::endl<<"Enter the last coloumn to inject into: ";
        std::cin>>clmnEnd;
        std::cout<<std::endl<<"Enter the first row to inject into: ";
        std::cin>>rowInit;
        std::cout<<std::endl<<"Enter the last row to inject into: ";
        std::cin>>rowEnd;
               
        for (int n = clmnInit; n<=clmnEnd; n++){
            std::cout << "Coloumn " << n << std::endl;
            step_rows(fe_flavor, gain, n, rowInit, rowEnd, false, fe);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));  
        }
    }
}


void const_V_cal_double(int VcalHigh, int VcalMed, Rd53a *fe){
    //Constant V_cal_high and V_cal_med double injection.
    bool validVal;
    while(true){
        validVal = false;
        while (not validVal){
            std::cout<<std::endl<<"Enter your desired V_cal_high in DAC steps: ";
            std::cin>>VcalHigh;
            validVal = VcalHigh <= 4095 && 0 <= VcalHigh;
            if (not validVal){
                std::cout << "Valid range for V_cal_high is 0 to 4095.";
            }     
        }  
        validVal = false;
        while (not validVal){
            std::cout<<std::endl<<"Enter your desired V_cal_med in DAC steps: ";
            std::cin>>VcalMed;
            validVal = VcalMed <= 4095 && 0 <= VcalMed;
            if (not validVal){
                std::cout << "Valid range for V_cal_med is 0 to 4095.";
            }     
        } 
        fe->writeRegister(&Rd53a::InjVcalHigh, VcalHigh);
        fe->writeRegister(&Rd53a::InjVcalMed, VcalMed);
        for(int n=0;  n<1000; n++){
            fe->cal(0, 1, 0, 0, 0, 0); //Bring cal_aux low
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
            fe->cal(0, 1, 0, 0xFF, 1, 100); //Bring cald_edge high, bring cal_aux high.
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); 

        }
    }
}


void step_V_cal(int VcalHigh, int VcalMed, Rd53a *fe){
    int Vdiff;
    int VdiffLimit;
    int VcalCenter = 2048;
    bool validDiff;
    int stepSize = 512;
    int direction = 1;
    int repeats;
    while(true){
        validDiff = false;
        while (not validDiff){
            std::cout<<std::endl<<"Enter the maximum V_cal_diff in DAC steps: ";
            std::cin>>VdiffLimit;
            validDiff = VdiffLimit <= 4095 && 0 <= VdiffLimit;
            if (not validDiff){
                std::cout << "Valid range for V_cal_diff is 1 to 4095.";
            }     
        }  
        VcalHigh = VcalCenter;
        VcalMed = VcalCenter;
        while (true){
            VcalHigh += direction*stepSize/2;
            VcalMed -= direction*stepSize/2;
            if (VcalHigh > 4095){VcalHigh = 4095;}
            if (VcalHigh < 0){VcalHigh = 0;}
            if (VcalMed > 4095){VcalMed = 4095;}
            if (VcalMed < 0){VcalMed = 0;}            
            Vdiff = VcalHigh - VcalMed;
            if(0>Vdiff){
                direction = 1;
                break;
            }
            if(Vdiff >= VdiffLimit){
                direction = -1;
                fe->writeRegister(&Rd53a::InjVcalHigh, VcalCenter + VdiffLimit/2);
                fe->writeRegister(&Rd53a::InjVcalMed, VcalCenter - VdiffLimit/2);
                repeats = 128*2;
            }else{
                fe->writeRegister(&Rd53a::InjVcalHigh, VcalHigh);
                fe->writeRegister(&Rd53a::InjVcalMed, VcalMed);
                repeats = 128;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(stepSize*2)); 
            std::cout << "\r" << "VcalHigh: " << VcalHigh << ", VcalMed: " << VcalMed << ", Vdiff: " << Vdiff << std::flush;
            
            for(int n=0; n<repeats; n++){
                fe->cal(0, 0, 0, 4, 0, 0); //Bring cal_edge high
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
                fe->cal(0, 1, 0, 0, 0, 0); //Arm cal_edge to low
                std::this_thread::sleep_for(std::chrono::milliseconds(4)); 
            }
            
        }

        
    }
}

int main(void) {
    
    //Parse config file
    //std::istringstream is_file(path);
    //std::string line;
    //std::string fe_select = "fe_select";
    //int fe_val = 0;
    //std::string row_select = "row_select";
    //int row_val = 0;
    //std::string gain_select = "gain_select";
    //int gain_val = 0;
    //while( std::getline(is_file, line) )
    //{
    //  std::istringstream is_line(line);
    //  std::string key;
    //  if( std::getline(is_line, key, '=') )
    //  {
    //    std::string value;
    //    if( std::getline(is_line, value) ) 
    //      if key.compare(fe_select){
    //        
    //      }
    //  }
    //}
           
    SpecController spec;
    spec.init(0);
    
    //Send IO config to active FMC
    spec.writeSingle(0x6<<14 | 0x0, EN_RX1 | EN_RX3 | EN_RX4 | EN_RX5);
    spec.writeSingle(0x6<<14 | 0x1, 0xF);
    spec.setCmdEnable(0x1);
    spec.setRxEnable(0x0);
    
    Rd53a fe(&spec);
    fe.setChipId(0);
    std::cout << ">>> Configuring chip with default config ..." << std::endl;
    fe.configure();
    std::cout << " ... done." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // TODO check link sync
    spec.setRxEnable(0x1);

    std::cout << ">>> Trigger test:" << std::endl;
    for (unsigned i=1; i<16; i++) {
        std::cout << "Trigger: " << i << std::endl;
        fe.trigger(i, i);
        RawData *data = NULL;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (data != NULL)
                delete data;
            data = spec.readData();
            decode(data);
        } while (data != NULL);
    }
    
    //======CHIP SETTINGS========
    int fe_flavor = 2; //0: Synchronous, 1: Linear, 2: Differential
    int gain = 1; //Linear front end gain. 0: low, 1: high
    int VcalCenter = 4096/2;
    int initVcalDiff = 0;
    int initVcalHigh = VcalCenter + initVcalDiff/2;
    int initVcalMed = VcalCenter - initVcalDiff/2;
    
    //=====SYNCHRONOUS FRONT END SETTINGS=====
    bool set_IBIASP1_SYNC = false;
    int IBIASP1_SYNC = 100;
    bool set_IBIASP2_SYNC = false;
    int IBIASP2_SYNC = 150;
    bool set_IBIAS_SF_SYNC = false;
    int IBIAS_SF_SYNC = 100;
    bool set_IBIAS_KRUM_SYNC = false;
    int IBIAS_KRUM_SYNC = 140;
    bool set_IBIAS_DISC_SYNC = false;
    int IBIAS_DISC_SYNC = 200;
    bool set_ICTRL_SYNCT_SYNC = false;
    int ICTRL_SYNCT_SYNC = 100;
    bool set_VBL_SYNC = false;
    int VBL_SYNC = 450;
    bool set_VTH_SYNC = false;
    int VTH_SYNC = 300;
    bool set_VREF_KRUM_SYNC = false;
    int VREF_KRUM_SYNC = 490;
    bool set_AUTO_ZERO = false;
    int AUTO_ZERO = 0;
    bool set_SELC2F = false;
    int SELC2F = 1;
    bool set_SELC4F = false;
    int SELC4F = 0;
    bool set_FASTTOT = false;
    int FASTTOT = 0;

    //=====LINEAR FRONT END SETTINGS=====
    bool set_PA_IN_BIAS_LIN = false;
    int PA_IN_BIAS_LIN = 300;
    bool set_FC_BIAS_LIN = false;
    int FC_BIAS_LIN = 20;
    bool set_KRUM_CURR_LIN = false;
    int KRUM_CURR_LIN = 50;
    bool set_LDAC_LIN = false;
    int LDAC_LIN = 80;
    bool set_COMP_LIN = false;
    int COMP_LIN = 511;
    bool set_REF_KRUM_LIN = false;
    int REF_KRUM_LIN = 300;
    bool set_VTH_LIN = false;
    int VTH_LIN = 500;
    
    //=====DIFFERENTIAL FRONT END SETTINGS=====
    bool set_PRMP_DIFF = false;
    int PRMP_DIFF = 755;
    bool set_FOL_DIFF = false;
    int FOL_DIFF = 542;
    bool set_PRECOMP_DIFF = false;
    int PRECOMP_DIFF =551;
    bool set_COMP_DIFF = false;
    int COMP_DIFF = 528;
    bool set_VFF_DIFF = true;
    int VFF_DIFF = 100;
    bool set_VTH1_DIFF = true;
    int VTH1_DIFF = 200;
    bool set_VTH2_DIFF = true;
    int VTH2_DIFF = 0;
    bool set_LCC_DIFF = false;
    int LCC_DIFF = 20;
    bool set_LCC_ENABLE = false;
    int LCC_ENABLE = 0;
    bool set_FEEDBACK_CAP = false;
    int FEEDBACK_CAP = 1;
    
    if (fe_flavor == 0){
        std::cout << "============SYNCHRONOUS FRONT END FLAVOR============"<<std::endl;
        if (set_IBIASP1_SYNC){
            std::cout << ">>> Setting cascode main branch bias current to " << IBIASP1_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncIbiasp1, IBIASP1_SYNC);         
        }
        if (set_IBIASP2_SYNC){
            std::cout << ">>> Setting input device main bias current to " << IBIASP2_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncIbiasp1, IBIASP2_SYNC);           
        }
        if (set_IBIAS_SF_SYNC){
            std::cout << ">>> Setting follower bias current to " << IBIAS_SF_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncIbiasSf, IBIAS_SF_SYNC);           
        }
        if (set_IBIAS_KRUM_SYNC){
            std::cout << ">>> Setting Krummenacher feedback bias current to " << IBIAS_KRUM_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncIbiasKrum, IBIAS_KRUM_SYNC);               
        }
        if (set_IBIAS_DISC_SYNC){
            std::cout << ">>> Setting comparator bias current to " << IBIAS_DISC_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncIbiasDisc, IBIAS_DISC_SYNC);          
        }
        if (set_ICTRL_SYNCT_SYNC){
            std::cout << ">>> Setting oscillator bias current to " << ICTRL_SYNCT_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncIctrlSynct, ICTRL_SYNCT_SYNC);            
        }
        if (set_VBL_SYNC){
            std::cout << ">>> Setting baseline voltage for offset compensation to " << VBL_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncVbl, VBL_SYNC);               
        }
        if (set_VTH_SYNC){
            std::cout << ">>> Setting discriminator threshold voltage to " << VTH_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncVth, VTH_SYNC);        
        }
        if (set_VREF_KRUM_SYNC){
            std::cout << ">>> Setting Krummenacher voltage reference to " << VREF_KRUM_SYNC << std::endl;
            fe.writeRegister(&Rd53a::SyncVrefKrum, VREF_KRUM_SYNC);           
        }
        if (set_AUTO_ZERO){
            std::cout << ">>> Setting Autozero to " << AUTO_ZERO << std::endl;
            fe.writeRegister(&Rd53a::SyncAutoZero, AUTO_ZERO);         
        }
        if (set_SELC2F){
            std::cout << ">>> Setting SelC2F to " << SELC2F << std::endl;
            fe.writeRegister(&Rd53a::SyncSelC2F, SELC2F);                
        }
        if (set_SELC4F){
            std::cout << ">>> Setting SelC4F to " << SELC4F << std::endl;
            fe.writeRegister(&Rd53a::SyncSelC4F, SELC4F);           
        }
        if (set_FASTTOT){
            std::cout << ">>> Setting Fast ToT to " << FASTTOT << std::endl;
            fe.writeRegister(&Rd53a::SyncFastTot, FASTTOT);         
        }       
    }  

    if (fe_flavor == 1){
        std::cout << "============LINEAR FRONT END FLAVOR============"<<std::endl;
        if (set_PA_IN_BIAS_LIN){
            std::cout << ">>> Setting preamplifier input branch bias current to " << PA_IN_BIAS_LIN << std::endl;
            fe.writeRegister(&Rd53a::LinPaInBias, PA_IN_BIAS_LIN); 
        }
        if (set_FC_BIAS_LIN){
            std::cout << ">>> Setting folded cascode branch current to " << FC_BIAS_LIN << std::endl;
            fe.writeRegister(&Rd53a::LinPaInBias, FC_BIAS_LIN); 
        }
        if (set_KRUM_CURR_LIN){
            std::cout << ">>> Setting Krummenacher feedback bias current to " << KRUM_CURR_LIN << std::endl;
            fe.writeRegister(&Rd53a::LinKrumCurr, KRUM_CURR_LIN); 
        }
        if (set_LDAC_LIN){
            std::cout << ">>> Setting fine threshold voltage to " << LDAC_LIN << std::endl;
            fe.writeRegister(&Rd53a::LinLdac, LDAC_LIN); 
        }
        if (set_COMP_LIN){
            std::cout << ">>> Setting comparator bias current to " << COMP_LIN << std::endl;
            fe.writeRegister(&Rd53a::LinComp, COMP_LIN); 
        }
        if (set_REF_KRUM_LIN){
            std::cout << ">>> Setting Krummenacher reference voltage to " << REF_KRUM_LIN << std::endl;
            fe.writeRegister(&Rd53a::LinRefKrum, REF_KRUM_LIN); 
        }
        if (set_VTH_LIN){
            std::cout << ">>> Setting global threshold voltage to " << VTH_LIN << std::endl;
            fe.writeRegister(&Rd53a::LinVth, VTH_LIN); 
        } 
    }
    
    if (fe_flavor == 2){
        std::cout << "============DIFFERENTIAL FRONT END FLAVOR============"<<std::endl;
        if (set_PRMP_DIFF){
            std::cout << ">>> Setting preamp input stage bias current to " << PRMP_DIFF << std::endl;
            fe.writeRegister(&Rd53a::DiffPrmp, PRMP_DIFF); 
        }
        if (set_FOL_DIFF){
            std::cout << ">>> Setting preamp follower bais current to " << FOL_DIFF << std::endl;
            fe.writeRegister(&Rd53a::DiffFol, FOL_DIFF); 
        }
        if (set_PRECOMP_DIFF){
            std::cout << ">>> Setting precomparator tail current to " << PRECOMP_DIFF << std::endl;
            fe.writeRegister(&Rd53a::DiffPrecomp, PRECOMP_DIFF); 
        }
        if (set_COMP_DIFF){
            std::cout << ">>> Setting comparator bias current to " << COMP_DIFF << std::endl;
            fe.writeRegister(&Rd53a::DiffComp, COMP_DIFF); 
        }
        if (set_VFF_DIFF){
            std::cout << ">>> Setting preamp feedback discharge current to " << VFF_DIFF << std::endl;
            fe.writeRegister(&Rd53a::DiffVff, VFF_DIFF); 
        }
        if (set_VTH1_DIFF){
            std::cout << ">>> Setting negative branch threshold offset to " << VTH1_DIFF << std::endl;
            fe.writeRegister(&Rd53a::DiffVth1, VTH1_DIFF); 
        }
        if (set_VTH2_DIFF){
            std::cout << ">>> Setting positive branch threshold offset to " << VTH2_DIFF << std::endl;
            fe.writeRegister(&Rd53a::DiffVth2, VTH2_DIFF); 
        }
        if (set_LCC_DIFF){
            std::cout << ">>> Setting leakage current compensation bias to " << LCC_DIFF << std::endl;
            fe.writeRegister(&Rd53a::DiffLcc, LCC_DIFF); 
        }
        if (set_LCC_ENABLE){
            std::cout << ">>> Setting LCC to " << LCC_ENABLE << std::endl;
            fe.writeRegister(&Rd53a::DiffLccEn, LCC_ENABLE); 
        }
        if (set_FEEDBACK_CAP){
            std::cout << ">>> Setting feedback cap to " << FEEDBACK_CAP << std::endl;
            fe.writeRegister(&Rd53a::DiffFbCapEn, FEEDBACK_CAP); 
        }
    }
    
    std::cout << ">>> Setting Vcal HIGH to " << initVcalHigh << std::endl;
    fe.writeRegister(&Rd53a::InjVcalHigh, initVcalHigh);
    std::cout << ">>> Setting Vcal MED to " << initVcalMed << std::endl;
    fe.writeRegister(&Rd53a::InjVcalMed, initVcalMed);

    //Digital front ends disable
    if (fe_flavor == 0){    
        std::cout << ">>> Disabling Linear and Differential digital front ends" << std::endl;
        fe.writeRegister(&Rd53a::EnCoreColLin1, 0);
        fe.writeRegister(&Rd53a::EnCoreColLin2, 0);
        fe.writeRegister(&Rd53a::EnCoreColDiff1, 0);
        fe.writeRegister(&Rd53a::EnCoreColDiff2, 0);
   }
    if (fe_flavor == 1){    
        std::cout << ">>> Disabling Synchronous and Differential digital front ends" << std::endl;
        fe.writeRegister(&Rd53a::EnCoreColDiff1, 0);
        fe.writeRegister(&Rd53a::EnCoreColDiff2, 0);
        fe.writeRegister(&Rd53a::EnCoreColSync, 0);
   }
    if (fe_flavor == 2){    
        std::cout << ">>> Disabling Synchronous and Linear digital front ends" << std::endl;
        fe.writeRegister(&Rd53a::EnCoreColSync, 0);
        fe.writeRegister(&Rd53a::EnCoreColLin1, 0);
        fe.writeRegister(&Rd53a::EnCoreColLin2, 0);
   }
            
    std::cout << ">>> Enabling analog injection" << std::endl;
    fe.writeRegister(&Rd53a::InjAnaMode, 0);
    fe.writeRegister(&Rd53a::LatencyConfig, 40);
    
    if (fe_flavor == 0){    
        std::cout << ">>> Enabling CAL for all synchronous front ends" << std::endl;
        fe.writeRegister(&Rd53a::CalColprLin1, 0);
        fe.writeRegister(&Rd53a::CalColprLin2, 0);
        fe.writeRegister(&Rd53a::CalColprLin3, 0);
        fe.writeRegister(&Rd53a::CalColprLin4, 0);
        fe.writeRegister(&Rd53a::CalColprLin5, 0);
        fe.writeRegister(&Rd53a::CalColprDiff1, 0);
        fe.writeRegister(&Rd53a::CalColprDiff2, 0);
        fe.writeRegister(&Rd53a::CalColprDiff3, 0);
        fe.writeRegister(&Rd53a::CalColprDiff4, 0);
        fe.writeRegister(&Rd53a::CalColprDiff5, 0);
   }
    if (fe_flavor == 1){    
        std::cout << ">>> Enabling CAL for all linear front ends" << std::endl;
        fe.writeRegister(&Rd53a::CalColprSync1, 0);
        fe.writeRegister(&Rd53a::CalColprSync2, 0);
        fe.writeRegister(&Rd53a::CalColprSync3, 0);
        fe.writeRegister(&Rd53a::CalColprSync4, 0);
        fe.writeRegister(&Rd53a::CalColprDiff1, 0);
        fe.writeRegister(&Rd53a::CalColprDiff2, 0);
        fe.writeRegister(&Rd53a::CalColprDiff3, 0);
        fe.writeRegister(&Rd53a::CalColprDiff4, 0);
        fe.writeRegister(&Rd53a::CalColprDiff5, 0);
   }
    if (fe_flavor == 2){    
        std::cout << ">>> Enabling CAL for all differential front ends" << std::endl;
        fe.writeRegister(&Rd53a::CalColprLin1, 0);
        fe.writeRegister(&Rd53a::CalColprLin2, 0);
        fe.writeRegister(&Rd53a::CalColprLin3, 0);
        fe.writeRegister(&Rd53a::CalColprLin4, 0);
        fe.writeRegister(&Rd53a::CalColprLin5, 0);
        fe.writeRegister(&Rd53a::CalColprSync1, 0);
        fe.writeRegister(&Rd53a::CalColprSync2, 0);
        fe.writeRegister(&Rd53a::CalColprSync3, 0);
        fe.writeRegister(&Rd53a::CalColprSync4, 0);
    }
    
    bool one_clmn = false;
    if (one_clmn){   
        std::cout << "Turning off CAL for all different front ends except double coloumn 22" << std::endl;
        fe.writeRegister(&Rd53a::CalColprDiff1, 0x0002); //double coloumn 3 : 0002
        fe.writeRegister(&Rd53a::CalColprDiff2, 0x0020); //double coloumn 22 : 0020
        fe.writeRegister(&Rd53a::CalColprDiff3, 0x0400); //double coloumn 42 : 0400
        fe.writeRegister(&Rd53a::CalColprDiff4, 0);
        fe.writeRegister(&Rd53a::CalColprDiff5, 0);
    }
    bool top_row = true;
    if (top_row){
        int clmn = 270;
        fe.enableCalCol(clmn/2);
        fe.setEn(clmn, 0, 1);
        fe.setInjEn(clmn, 0, 1);   
        clmn = 307;
        fe.enableCalCol(clmn/2);
        fe.setEn(clmn, 0, 1);
        fe.setInjEn(clmn, 0, 1);
        clmn = 347;
        fe.enableCalCol(clmn/2);
        fe.setEn(clmn, 0, 1);
        fe.setInjEn(clmn, 0, 1);    
    }
    fe.configurePixels();
    
    std::cout << ">>> Routing Hitors to LVDS outputs" << std::endl;
    fe.writeRegister(&Rd53a::GpLvdsRoute, 0x02);
    
    std::cout << ">>> Enabling HitOr for all cores of the selected front end flavor" << std::endl;
    if (fe_flavor == 0){
        fe.writeRegister(&Rd53a::HitOr0MaskSync ,0);
        fe.writeRegister(&Rd53a::HitOr1MaskSync ,0);
        fe.writeRegister(&Rd53a::HitOr2MaskSync ,0);
        fe.writeRegister(&Rd53a::HitOr3MaskSync ,0);
    }
    if (fe_flavor == 1){
        fe.writeRegister(&Rd53a::HitOr0MaskLin0 ,0);
        fe.writeRegister(&Rd53a::HitOr1MaskLin0 ,0);
        fe.writeRegister(&Rd53a::HitOr2MaskLin0 ,0);
        fe.writeRegister(&Rd53a::HitOr3MaskLin0 ,0);
        fe.writeRegister(&Rd53a::HitOr0MaskLin1 ,0);
        fe.writeRegister(&Rd53a::HitOr1MaskLin1 ,0);
        fe.writeRegister(&Rd53a::HitOr2MaskLin1 ,0);
        fe.writeRegister(&Rd53a::HitOr3MaskLin1 ,0);
    }
    if (fe_flavor == 2){
        fe.writeRegister(&Rd53a::HitOr0MaskDiff0 ,0);
        fe.writeRegister(&Rd53a::HitOr1MaskDiff0 ,0);
        fe.writeRegister(&Rd53a::HitOr2MaskDiff0 ,0);
        fe.writeRegister(&Rd53a::HitOr3MaskDiff0 ,0);
        fe.writeRegister(&Rd53a::HitOr0MaskDiff1 ,0);
        fe.writeRegister(&Rd53a::HitOr1MaskDiff1 ,0);
        fe.writeRegister(&Rd53a::HitOr2MaskDiff1 ,0);
        fe.writeRegister(&Rd53a::HitOr3MaskDiff1 ,0);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    int test_scheme;
    
    bool option_valid = false;
    while (not option_valid){
        option_valid = true;
        std::cout << "Please choose a test scheme.\n1: Choose a injection voltage and step I_comp from 0 to 511.\n2: Choose a maximum injection voltage and step VCalDiff from 0 to this value.\n3: Choose a specific injection voltage and inject 1000 times with this voltage.\n4: Choose a V_cal_med and V_cal_high and perform 1000 double injections.\n5: Constant Vcal=1000 injection into subset of coloumns and rows.\n6: Step VFF with constant injection voltage\n7: Set Vcal_diff and VFF and inject 1000 times."<<std::endl;
        std::cin >> test_scheme;
        int VcalHigh=0;
        int VcalMed=0;
        switch(test_scheme){
            case 1 : step_I_comp(VcalHigh,VcalMed, &fe); break;
            case 2 : step_V_cal(VcalHigh,VcalMed, &fe); break;
            case 3 : const_V_cal(VcalHigh,VcalMed, &fe); break;
            case 4 : const_V_cal_double(VcalHigh,VcalMed, &fe); break;
            case 5 : step_clmns(fe_flavor, gain, &fe); break;
            case 6 : const_V_cal_and_VFF(fe_flavor, gain, &fe); break;
            case 7 : step_VFF(fe_flavor, gain, &fe); break;
            default : std::cout << "Not a valid option."; option_valid = false; break;
        }
    }


}
