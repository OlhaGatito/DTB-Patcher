#include "dtb_model.hpp"
#include <cstdio>
#include <string>
#include <fstream>

static bool readTree(const char* path,DtbNode& n){
    return parse_dts_file(path,n);
}

int main(int argc,char** argv){
    if(argc!=3){
        std::fprintf(stderr,"usage: test_model <donor.dts> <receiver.dts>\n");
        return 2;
    }

    DtbNode donor,receiver;
    if(!readTree(argv[1],donor)||!readTree(argv[2],receiver)){
        std::fprintf(stderr,"semantic model parse failed\n");
        return 3;
    }

    auto plan=build_transfer_plan(donor,receiver);

    int up=-1;
    int audio=-1;
    for(int i=0;i<(int)plan.size();++i){
        if(plan[i].category=="controls" && plan[i].label=="up")up=i;
        if(plan[i].category=="audio")audio=i;
    }

    if(up<0||audio<0){
        std::fprintf(stderr,"semantic plan did not expose expected UP/audio blocks\n");
        return 4;
    }
    if(!plan[up].compatible){
        std::fprintf(stderr,"UP donor/receptor block was not recognized as transferable\\n");
        return 5;
    }

    if(!apply_transfer_item(receiver,donor,plan[up])){
        std::fprintf(stderr,"UP transfer was not applied\n");
        return 6;
    }

    const auto& gpio=receiver.children.at("gpio-keys").children.at("button-up")
                          .properties.at("gpios").value;
    if(gpio.find("20")==std::string::npos){
        std::fprintf(stderr,"receiver GPIO was not replaced: %s\n",gpio.c_str());
        return 7;
    }

    std::printf("Semantic model smoke test OK: %zu transfer block(s)\n",plan.size());
    return 0;
}
