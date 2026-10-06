#include "dtb_model.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

std::vector<std::string> lex(const std::string& in);

int main(int argc,char** argv){
    if(argc!=2){std::cerr<<"usage: debug_lex <file.dts>\n";return 1;}
    std::ifstream f(argv[1],std::ios::binary);
    if(!f){std::cerr<<"cannot open: "<<argv[1]<<"\n";return 1;}
    std::stringstream s;s<<f.rdbuf();
    std::string content=s.str();
    std::cout<<"file size: "<<content.size()<<"\n";
    auto tokens=lex(content);
    std::cout<<"tokens: "<<tokens.size()<<"\n";
    for(size_t i=0;i<tokens.size()&&i<60;++i)
        std::cout<<"  ["<<i<<"] '"<<tokens[i]<<"'\n";
    if(tokens.size()>60)std::cout<<"  ... (+"<<(tokens.size()-60)<<")\n";
    DtbNode root;
    if(parse_dts_file(argv[1],root)){
        std::cout<<"PARSE OK. root children="<<root.children.size()<<"\n";
        return 0;
    }
    std::cout<<"PARSE FAILED\n";
    return 2;
}
