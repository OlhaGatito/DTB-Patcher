#pragma once
#include <string>
#include <vector>
#include <map>
struct DtbProperty { std::string name; std::string value; };
struct DtbNode {
    std::string name;
    std::string path;
    std::map<std::string,DtbProperty> properties;
    std::map<std::string,DtbNode> children;
};
struct DtbChange {
    std::string path;
    std::string kind;
    std::string category;
    std::string detail;
};
bool parse_dts_file(const std::string&,DtbNode&);
bool render_dts(const DtbNode&,const std::string&);
std::vector<DtbChange> compare_trees(const DtbNode&,const DtbNode&);
bool apply_change(DtbNode&,const DtbNode&,const DtbChange&);
