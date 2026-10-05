#pragma once
#include <string>
#include <vector>
#include <map>

struct DtbProperty {
    std::string name;
    std::string value;
};

struct DtbNode {
    std::string name;
    std::string path;
    std::map<std::string,DtbProperty> properties;
    std::map<std::string,DtbNode> children;
};

struct DtbTransferItem {
    std::string category;
    std::string key;
    std::string label;

    std::string donorPath;
    std::string receiverPath;
    std::string donorProperty;
    std::string receiverProperty;

    std::vector<std::string> propertyNames;

    std::string donorValue;
    std::string receiverValue;
    std::string detail;

    bool compatible=false;
};

bool parse_dts_file(const std::string&,DtbNode&);
bool render_dts(const DtbNode&,const std::string&);

std::vector<DtbTransferItem> build_transfer_plan(const DtbNode&,const DtbNode&);
bool apply_transfer_item(DtbNode&,const DtbNode&,const DtbTransferItem&);
