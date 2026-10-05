/*
 * Semantic DTB comparison/transfer model.
 *
 * The UI must not expose thousands of low-level property differences when the
 * user is trying to move a functional block such as a physical button.
 * This layer therefore builds a compact transfer plan:
 *   - Controls: one physical input/GPIO entry per function.
 *   - Audio/Display/Power: one functional node block with relevant properties.
 *   - Other: grouped same-node differences as a safe fallback.
 *
 * A transfer never creates a missing receiver node. A selected item can only
 * replace properties that already exist in the receiver. This keeps the
 * receiver DTB as the structural base and prevents accidental donor-only
 * subtrees/phandle graphs from being inserted blindly.
 */
#include "dtb_model.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <set>
#include <cstdlib>
#include <functional>
#include <cstdint>

namespace {

std::vector<std::string> lex(const std::string& in){
    std::vector<std::string> o;
    for(size_t i=0;i<in.size();){
        if(std::isspace((unsigned char)in[i])){++i;continue;}
        if(in[i]=='/'&&i+1<in.size()&&in[i+1]=='*'){
            i+=2;
            while(i+1<in.size()&&!(in[i]=='*'&&in[i+1]=='/'))++i;
            if(i+1<in.size())i+=2;
            continue;
        }
        if(in[i]=='/'&&i+1<in.size()&&in[i+1]=='/'){
            i+=2;
            while(i<in.size()&&in[i]!='\n')++i;
            continue;
        }
        if(in[i]=='"'){
            size_t j=i+1;
            while(j<in.size()){
                if(in[j]=='\\')j+=2;
                else if(in[j]=='"'){++j;break;}
                else ++j;
            }
            o.push_back(in.substr(i,j-i));
            i=j;
            continue;
        }
        if(std::string("{};=<>[]:").find(in[i])!=std::string::npos){
            o.emplace_back(1,in[i]);++i;continue;
        }
        size_t j=i;
        while(j<in.size() &&
              !std::isspace((unsigned char)in[j]) &&
              std::string("{};=<>[]:").find(in[j])==std::string::npos)++j;
        o.push_back(in.substr(i,j-i));
        i=j;
    }
    return o;
}

struct Parser{
    std::vector<std::string> t;
    size_t p=0;

    std::string until(const std::string& end){
        std::string r;
        while(p<t.size()&&t[p]!=end){
            if(!r.empty())r+=' ';
            r+=t[p++];
        }
        return r;
    }

    bool node(DtbNode& n,const std::string& path){
        if(p>=t.size()||t[p++]!="{")return false;
        n.path=path;
        while(p<t.size()&&t[p]!="}"){
            if(t[p]==";"){++p;continue;}

            std::string a=t[p++];

            std::string label;
            if(p<t.size()&&t[p]==":"){
                label=a;
                ++p;
                if(p>=t.size())return false;
                a=t[p++];
            }

            if(a=="/dts-v1/"||a=="/plugin/"){
                while(p<t.size()&&t[p]!=";")++p;
                if(p<t.size())++p;
                continue;
            }

            if(a=="/memreserve/"){
                std::string v;
                while(p<t.size()&&t[p]!=";"){
                    if(!v.empty())v+=' ';
                    v+=t[p++];
                }
                if(p<t.size())++p;
                if(!label.empty())v=label+": /memreserve/ "+v;
                else v="/memreserve/ "+v;
                n.memreserve.push_back(v);
                continue;
            }

            if(p<t.size()&&t[p]=="{"){
                DtbNode c;
                c.name=a;
                c.label=label;
                std::string cp=path=="/"?"/"+a:path+"/"+a;
                if(!node(c,cp))return false;
                n.children[c.name]=std::move(c);
                if(p<t.size()&&t[p]==";")++p;
            }else{
                std::string v;
                if(p<t.size()&&t[p]=="="){
                    ++p;
                    v=until(";");
                    if(p<t.size())++p;
                }else{
                    while(p<t.size()&&t[p]!=";")++p;
                    if(p<t.size())++p;
                }
                n.properties[a]={a,v,label};
            }
        }
        if(p>=t.size())return false;
        ++p;
        return true;
    }
};

std::string lower(std::string s){
    std::transform(s.begin(),s.end(),s.begin(),[](char c){
        return (char)std::tolower((unsigned char)c);
    });
    return s;
}

bool has(const std::string& s,const std::string& q){
    return lower(s).find(lower(q))!=std::string::npos;
}

bool starts(const std::string& s,const std::string& q){
    std::string a=lower(s),b=lower(q);
    return a.rfind(b,0)==0;
}

bool ends(const std::string& s,const std::string& q){
    std::string a=lower(s),b=lower(q);
    return a.size()>=b.size()&&a.compare(a.size()-b.size(),b.size(),b)==0;
}

std::string stripQuotes(std::string s){
    if(s.size()>=2&&s.front()=='"'&&s.back()=='"')return s.substr(1,s.size()-2);
    return s;
}

std::string propValue(const DtbNode& n,const std::string& name){
    auto it=n.properties.find(name);
    return it==n.properties.end()?std::string():it->second.value;
}

std::string firstProp(const DtbNode& n,const std::vector<std::string>& names){
    for(const auto& x:names){
        auto it=n.properties.find(x);
        if(it!=n.properties.end())return stripQuotes(it->second.value);
    }
    return {};
}

const DtbNode* find_node(const DtbNode& root,const std::string& path){
    if(path=="/")return &root;
    if(path.size()<2)return nullptr;
    const DtbNode* n=&root;
    size_t s=1;
    while(s<path.size()){
        size_t e=path.find('/',s);
        if(e==std::string::npos)e=path.size();
        auto it=n->children.find(path.substr(s,e-s));
        if(it==n->children.end())return nullptr;
        n=&it->second;
        s=e+1;
    }
    return n;
}

DtbNode* find_node(DtbNode& root,const std::string& path){
    return const_cast<DtbNode*>(find_node((const DtbNode&)root,path));
}

void collectNodes(const DtbNode& n,std::vector<const DtbNode*>& out){
    out.push_back(&n);
    for(const auto& kv:n.children)collectNodes(kv.second,out);
}

std::string codeName(long v){
    switch(v){
        case 304:return "A";
        case 305:return "B";
        case 307:return "X";
        case 308:return "Y";
        case 310:return "L1";
        case 311:return "R1";
        case 312:return "L2";
        case 313:return "R2";
        case 314:return "SELECT";
        case 315:return "START";
        case 317:return "L3";
        case 318:return "R3";
        case 544:return "UP";
        case 545:return "DOWN";
        case 546:return "LEFT";
        case 547:return "RIGHT";
        case 316:return "MODE";
        default:return {};
    }
}

std::string numericCodeName(const std::string& value){
    size_t p=value.find('<');
    if(p==std::string::npos)return {};
    ++p;
    int index=0;
    while(p<value.size()){
        while(p<value.size()&&std::isspace((unsigned char)value[p]))++p;
        if(p>=value.size()||value[p]=='>')break;
        size_t end=p;
        while(end<value.size()&&!std::isspace((unsigned char)value[end])&&value[end]!='>')++end;
        std::string tok=value.substr(p,end-p);
        char* e=nullptr;
        long v=std::strtol(tok.c_str(),&e,0);
        if(e&&*e=='\0'&&index==0){
            std::string n=codeName(v);
            if(!n.empty())return n;
        }
        ++index;
        p=end;
    }
    return {};
}

std::string normalizeName(std::string s){
    s=lower(s);
    for(char& c:s){
        if(c=='_'||c=='-')c=' ';
    }
    while(s.find("  ")!=std::string::npos)s.replace(s.find("  "),2," ");
    return s;
}

std::string controlKey(const DtbNode& n,const std::string& prop){
    std::string label=firstProp(n,{"label","key-label","button-label","name"});
    if(!label.empty())return normalizeName(label);

    std::string code=n.properties.count("linux,code")?
        numericCodeName(n.properties.at("linux,code").value):"";
    if(!code.empty())return normalizeName(code);

    std::string name=normalizeName(n.name);
    const std::vector<std::pair<std::string,std::string>> known={
        {"volume up","VOLUME UP"},{"volume down","VOLUME DOWN"},
        {"dpad up","UP"},{"dpad down","DOWN"},
        {"dpad left","LEFT"},{"dpad right","RIGHT"},
        {"button a","A"},{"button b","B"},{"button x","X"},{"button y","Y"},
        {"key up","UP"},{"key down","DOWN"},{"key left","LEFT"},{"key right","RIGHT"},
        {"up","UP"},{"down","DOWN"},{"left","LEFT"},{"right","RIGHT"},
        {"select","SELECT"},{"start","START"},{"mode","MODE"},
        {"l1","L1"},{"l2","L2"},{"l3","L3"},{"r1","R1"},{"r2","R2"},{"r3","R3"}
    };
    for(const auto& k:known)if(name==k.first||name.find(k.first)!=std::string::npos)
        return normalizeName(k.second);

    if(has(prop,"rocker1"))return "rocker 1";
    if(has(prop,"rocker"))return "rocker";
    if(!name.empty())return name;
    return normalizeName(prop);
}

bool isPhysicalControlNode(const DtbNode& n){
    std::string x=lower(n.path+" "+n.name);
    if(has(x,"pinctrl")||has(x,"gpio@"))return false;
    bool named=has(x,"button")||has(x,"key")||has(x,"joy")||has(x,"gamepad")||
               has(x,"rocker")||has(x,"input")||has(x,"retrogame");
    bool hasGpio=false;
    for(const auto& kv:n.properties){
        if(kv.first=="gpio"||kv.first=="gpios"||ends(kv.first,"-gpios")){hasGpio=true;break;}
    }
    return named&&hasGpio;
}

std::string compact(std::string s,size_t maxLen=150){
    for(char& c:s)if(c=='\r'||c=='\n')c=' ';
    if(s.size()<=maxLen)return s;
    return s.substr(0,maxLen-3)+"...";
}

std::string gpioSummary(const std::string& value){
    size_t p=value.find('<');
    if(p==std::string::npos)return compact(value);
    ++p;
    int index=0;
    long chosen=-1;
    std::string extra;
    while(p<value.size()){
        while(p<value.size()&&std::isspace((unsigned char)value[p]))++p;
        if(p>=value.size()||value[p]=='>')break;
        size_t e=p;
        while(e<value.size()&&!std::isspace((unsigned char)value[e])&&value[e]!='>')++e;
        std::string tok=value.substr(p,e-p);
        char* ep=nullptr;
        long v=std::strtol(tok.c_str(),&ep,0);
        if(ep&&*ep=='\0'){
            if(index==1)chosen=v;
            if(index>=2&&!extra.empty())extra+=' ';
            if(index>=2)extra+=tok;
            ++index;
        }
        p=e;
    }
    if(chosen>=0){
        std::string out="GPIO "+std::to_string(chosen);
        if(!extra.empty())out+="  <"+extra+">";
        return out;
    }
    return compact(value);
}

std::vector<std::string> controlProperties(const DtbNode& n){
    std::vector<std::string> out;
    for(const auto& kv:n.properties){
        if(kv.first=="gpio"||kv.first=="gpios"||ends(kv.first,"-gpios"))out.push_back(kv.first);
    }
    return out;
}

bool relevantNode(const DtbNode& n,const std::string& category){
    std::string x=lower(n.path+" "+n.name);
    if(category=="audio")
        return has(x,"audio")||has(x,"sound")||has(x,"codec")||has(x,"i2s")||has(x,"dai");
    if(category=="display")
        return has(x,"display")||has(x,"panel")||has(x,"backlight")||has(x,"lcd")||has(x,"mipi")||has(x,"dsi");
    if(category=="power")
        return has(x,"battery")||has(x,"charger")||has(x,"power")||has(x,"fuel")||has(x,"adc");
    return false;
}

bool relevantProperty(const std::string& category,const std::string& p){
    std::string x=lower(p);
    if(category=="audio")
        return has(x,"audio")||has(x,"sound")||has(x,"codec")||has(x,"dai")||
               has(x,"i2s")||has(x,"routing")||has(x,"format")||has(x,"mclk")||
               has(x,"clock")||x=="compatible"||x=="status"||x=="reg";
    if(category=="display")
        return has(x,"display")||has(x,"panel")||has(x,"backlight")||has(x,"lcd")||
               has(x,"timing")||has(x,"width")||has(x,"height")||has(x,"format")||
               has(x,"reset")||has(x,"enable")||has(x,"power")||has(x,"remote")||
               x=="compatible"||x=="status"||x=="reg";
    if(category=="power")
        return has(x,"battery")||has(x,"charger")||has(x,"charge")||has(x,"voltage")||
               has(x,"current")||has(x,"capacity")||has(x,"adc")||has(x,"channel")||
               has(x,"gpio")||x=="compatible"||x=="status"||x=="reg";
    return true;
}

std::string nodeKey(const DtbNode& n,const std::string& category){
    std::string label=firstProp(n,{"label","device_type"});
    if(!label.empty())return category+":label:"+normalizeName(label);
    std::string reg=propValue(n,"reg");
    if(!reg.empty())
        return category+":node:"+normalizeName(n.name)+"|reg:"+normalizeName(reg);
    return category+":path:"+normalizeName(n.path);
}

std::string blockSummary(const DtbNode& n,const std::vector<std::string>& props,bool gpioMode){
    std::string out;
    for(const auto& p:props){
        auto it=n.properties.find(p);
        if(it==n.properties.end())continue;
        std::string v=gpioMode?gpioSummary(it->second.value):compact(it->second.value,80);
        if(!out.empty())out+="  |  ";
        out+=p+": "+v;
    }
    return compact(out,220);
}

void addControlItems(const DtbNode& donor,const DtbNode& receiver,std::vector<DtbTransferItem>& out){
    std::vector<const DtbNode*> dn,rn;
    collectNodes(donor,dn);collectNodes(receiver,rn);

    std::map<std::string,const DtbNode*> rmap;
    for(const DtbNode* n:rn){
        if(!isPhysicalControlNode(*n))continue;
        for(const auto& p:controlProperties(*n)){
            std::string key=controlKey(*n,p);
            if(!rmap.count(key))rmap[key]=n;
        }
    }

    std::set<std::string> seen;
    for(const DtbNode* n:dn){
        if(!isPhysicalControlNode(*n))continue;
        for(const auto& p:controlProperties(*n)){
            std::string key=controlKey(*n,p);
            if(!seen.insert(key).second)continue;

            DtbTransferItem item;
            item.category="controls";
            item.key=key;
            item.label=key;
            item.donorPath=n->path;
            item.donorProperty=p;
            item.donorValue=gpioSummary(propValue(*n,p));
            item.detail="GPIO fisico da funcao; somente este valor sera transferido.";

            auto rit=rmap.find(key);
            if(rit==rmap.end()){
                item.compatible=false;
                item.receiverPath="";
                item.receiverProperty=p;
                item.receiverValue="Funcao nao encontrada no receptor";
            }else{
                const DtbNode* r=rit->second;
                item.receiverPath=r->path;
                item.receiverProperty=r->properties.count(p)?p:"";
                if(item.receiverProperty.empty()){
                    item.compatible=false;
                    item.receiverValue="Propriedade GPIO nao encontrada";
                }else{
                    item.receiverValue=gpioSummary(propValue(*r,p));
                    item.compatible=(propValue(*n,p)!=propValue(*r,p));
                    if(!item.compatible)continue; // no visible row for an unchanged control
                }
            }
            out.push_back(std::move(item));
        }
    }
}

void addFunctionalItems(const DtbNode& donor,const DtbNode& receiver,const std::string& category,std::vector<DtbTransferItem>& out){
    std::vector<const DtbNode*> dn,rn;
    collectNodes(donor,dn);collectNodes(receiver,rn);

    std::map<std::string,const DtbNode*> rmap;
    for(const DtbNode* n:rn){
        if(relevantNode(*n,category))rmap.emplace(nodeKey(*n,category),n);
    }

    std::set<std::string> seen;
    for(const DtbNode* n:dn){
        if(!relevantNode(*n,category))continue;
        std::vector<std::string> props;
        for(const auto& kv:n->properties)if(relevantProperty(category,kv.first))props.push_back(kv.first);
        if(props.empty())continue;

        std::string key=nodeKey(*n,category);
        if(!seen.insert(key).second)continue;

        DtbTransferItem item;
        item.category=category;
        item.key=key.substr(category.size()+1);
        item.label=item.key;
        item.donorPath=n->path;
        item.donorProperty=props.empty()?std::string():props.front();
        item.donorValue=blockSummary(*n,props,false);
        item.detail="Bloco funcional; substitui somente propriedades existentes no receptor.";

        auto rit=rmap.find(key);
        if(rit==rmap.end()){
            item.compatible=false;
            item.receiverValue="Bloco correspondente nao encontrado";
        }else{
            const DtbNode* r=rit->second;
            item.receiverPath=r->path;
            std::vector<std::string> common;
            for(const auto& p:props){
                auto ri=r->properties.find(p);
                if(ri!=r->properties.end()&&ri->second.value!=n->properties.at(p).value)
                    common.push_back(p);
            }
            item.compatible=!common.empty();
            item.receiverValue=common.empty()?
                "Sem diferenca transferivel":
                blockSummary(*r,common,false);
            item.propertyNames=std::move(common);
        }
        out.push_back(std::move(item));
    }
}

void addOtherItems(const DtbNode& donor,const DtbNode& receiver,std::vector<DtbTransferItem>& out){
    std::vector<const DtbNode*> dn;
    collectNodes(donor,dn);
    for(const DtbNode* n:dn){
        const DtbNode* r=find_node(receiver,n->path);
        if(!r)continue;
        std::vector<std::string> common;
        for(const auto& kv:n->properties){
            if(r->properties.count(kv.first)&&r->properties.at(kv.first).value!=kv.second.value)
                common.push_back(kv.first);
        }
        if(common.empty())continue;

        DtbTransferItem item;
        item.category="other";
        item.key=n->name.empty()?"/":n->name;
        item.label=item.key;
        item.donorPath=n->path;
        item.receiverPath=r->path;
        item.propertyNames=common;
        item.donorProperty=common.front();
        item.donorValue=blockSummary(*n,common,false);
        item.receiverValue=blockSummary(*r,common,false);
        item.compatible=true;
        item.detail="Diferencas no mesmo no; somente propriedades ja existentes serao substituidas.";
        out.push_back(std::move(item));
    }
}

} // namespace

bool parse_dts_file(const std::string& f,DtbNode& root){
    std::ifstream in(f,std::ios::binary);
    if(!in)return false;
    std::stringstream s;s<<in.rdbuf();
    Parser p;
    p.t=lex(s.str());
    root={};
    root.path="/";
    root.name="";
    while(p.p<p.t.size()&&p.t[p.p]!="{"){
        if(p.t[p.p]=="/dts-v1/"||p.t[p.p]=="/plugin/"){
            while(p.p<p.t.size()&&p.t[p.p]!=";")++p.p;
            if(p.p<p.t.size())++p.p;
            continue;
        }
        std::string label;
        if(p.p+1<p.t.size()&&p.t[p.p+1]==":"){
            label=p.t[p.p];
            p.p+=2;
        }
        if(p.p<p.t.size()&&p.t[p.p]=="/memreserve/"){
            std::string v;
            ++p.p;
            while(p.p<p.t.size()&&p.t[p.p]!=";"){
                if(!v.empty())v+=' ';
                v+=p.t[p.p++];
            }
            if(p.p<p.t.size())++p.p;
            root.memreserve.push_back(label.empty()?"/memreserve/ "+v:label+": /memreserve/ "+v);
            continue;
        }
        while(p.p<p.t.size()&&p.t[p.p]!=";")++p.p;
        if(p.p<p.t.size())++p.p;
    }
    return p.p<p.t.size()&&p.node(root,"/");
}

bool render_dts(const DtbNode& root,const std::string& f){
    std::ofstream o(f,std::ios::binary);
    if(!o)return false;
    for(const auto& reserve:root.memreserve)
        o<<reserve<<";\n";
    if(!root.memreserve.empty())o<<"\n";

    std::function<void(const DtbNode&,int)> render=[&](const DtbNode& n,int lv){
        std::string i(lv,'\t');
        if(!n.label.empty())o<<i<<n.label<<": ";
        o<<(n.name.empty()?"/":n.name)<<" {\n";
        for(const auto& kv:n.properties){
            o<<i<<"\t";
            if(!kv.second.label.empty())o<<kv.second.label<<": ";
            o<<kv.second.name;
            if(!kv.second.value.empty())o<<" = "<<kv.second.value;
            o<<";\n";
        }
        for(const auto& kv:n.children){
            o<<"\n";
            render(kv.second,lv+1);
        }
        o<<i<<"};\n";
    };
    o<<"/dts-v1/;\n\n";
    render(root,0);
    return (bool)o;
}

std::vector<DtbTransferItem> build_transfer_plan(const DtbNode& donor,const DtbNode& receiver){
    std::vector<DtbTransferItem> out;
    addControlItems(donor,receiver,out);
    addFunctionalItems(donor,receiver,"audio",out);
    addFunctionalItems(donor,receiver,"display",out);
    addFunctionalItems(donor,receiver,"power",out);
    addOtherItems(donor,receiver,out);
    return out;
}

bool apply_transfer_item(DtbNode& receiver,const DtbNode& donor,const DtbTransferItem& item){
    if(!item.compatible)return false;
    const DtbNode* dn=find_node(donor,item.donorPath);
    DtbNode* rn=find_node(receiver,item.receiverPath);
    if(!dn||!rn)return false;

    std::vector<std::string> props=item.propertyNames;
    if(props.empty()&& !item.donorProperty.empty())props.push_back(item.donorProperty);

    int changed=0;
    for(const auto& p:props){
        auto di=dn->properties.find(p);
        auto ri=rn->properties.find(p);
        if(di==dn->properties.end()||ri==rn->properties.end())continue;
        if(ri->second.value!=di->second.value){
            ri->second.value=di->second.value;
            ++changed;
        }
    }
    return changed>0;
}
