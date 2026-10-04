#include "dtc_bridge.h"
#include <cstdio>
#include <filesystem>
int main(int argc,char**argv){
    if(argc!=3){std::fprintf(stderr,"usage: test_dtc <dts> <outdir>\n");return 2;}
    std::filesystem::path dts=argv[1], out=argv[2];
    std::filesystem::create_directories(out);
    auto dtb=(out/"smoke.dtb").string();
    auto round=(out/"smoke-roundtrip.dts").string();
    if(dtbp_dtc_compile(dts.string().c_str(),dtb.c_str())){std::fprintf(stderr,"%s\n",dtbp_dtc_error());return 3;}
    if(dtbp_dtc_decompile(dtb.c_str(),round.c_str())){std::fprintf(stderr,"%s\n",dtbp_dtc_error());return 4;}
    if(!std::filesystem::exists(dtb)||!std::filesystem::exists(round))return 5;
    std::printf("DTC native smoke test OK: %s\n",dtbp_dtc_version());
    return 0;
}
