#include "dtb_model.hpp"
#include <cstdio>
#include <filesystem>

int main(int argc,char** argv){
    if(argc!=2){
        std::fprintf(stderr,"usage: test_parser <fixture.dts>\n");
        return 2;
    }

    DtbNode root;
    if(!parse_dts_file(argv[1],root)){
        std::fprintf(stderr,"parser regression fixture failed to parse\n");
        return 3;
    }

    if(root.memreserve.size()!=1){
        std::fprintf(stderr,"/memreserve/ was lost: %zu entry(s)\n",root.memreserve.size());
        return 4;
    }

    const auto it=root.children.find("test");
    if(it==root.children.end()){
        std::fprintf(stderr,"test node missing\n");
        return 5;
    }

    const DtbNode& n=it->second;
    if(!n.properties.count("linux,code")||n.properties.at("linux,code").value!="< 0x220 >"){
        std::fprintf(stderr,"linux,code property was not preserved: '%s'\n",
                     n.properties.count("linux,code")?n.properties.at("linux,code").value.c_str():"<missing>");
        return 6;
    }
    if(!n.properties.count("rockchip,pins")){
        std::fprintf(stderr,"rockchip,pins property was lost\n");
        return 7;
    }
    if(!n.properties.count("gpios")||n.properties.at("gpios").value.find("&gpio0")==std::string::npos){
        std::fprintf(stderr,"DTS phandle reference was not preserved: '%s'\n",
                     n.properties.count("gpios")?n.properties.at("gpios").value.c_str():"<missing>");
        return 8;
    }

    auto round=std::filesystem::temp_directory_path()/"gatito-parser-regression-roundtrip.dts";
    if(!render_dts(root,round.string())){
        std::fprintf(stderr,"parser regression render failed\n");
        return 9;
    }

    DtbNode reread;
    if(!parse_dts_file(round.string(),reread)){
        std::fprintf(stderr,"parser regression round-trip parse failed\n");
        return 10;
    }
    std::error_code ec;
    std::filesystem::remove(round,ec);

    if(reread.memreserve.size()!=1||!reread.children.at("test").properties.count("linux,code")||
       !reread.children.at("test").properties.count("rockchip,pins")){
        std::fprintf(stderr,"parser regression round-trip lost data\n");
        return 11;
    }

    std::printf("DTS parser regression test OK\n");
    return 0;
}
