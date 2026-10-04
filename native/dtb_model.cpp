#include "dtb_model.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace {
std::vector<std::string> lex(const std::string& in){
 std::vector<std::string> o;
 for(size_t i=0;i<in.size();){
  if(std::isspace((unsigned char)in[i])){++i;continue;}
  if(in[i]=='/'&&i+1<in.size()&&in[i+1]=='*'){i+=2;while(i+1<in.size()&&!(in[i]=='*'&&in[i+1]=='/'))++i;i+=2;continue;}
  if(in[i]=='/'&&i+1<in.size()&&in[i+1]=='/'){i+=2;while(i<in.size()&&in[i]!='\n')++i;continue;}
  if(in[i]=='"'){size_t j=i+1;while(j<in.size()){if(in[j]=='\\')j+=2;else if(in[j]=='"'){++j;break;}else ++j;}o.push_back(in.substr(i,j-i));i=j;continue;}
  if(std::string("{};=<>[],:&").find(in[i])!=std::string::npos){o.emplace_back(1,in[i]);++i;continue;}
  size_t j=i;while(j<in.size()&&!std::isspace((unsigned char)in[j])&&std::string("{};=<>[],:&").find(in[j])==std::string::npos)++j;
  o.push_back(in.substr(i,j-i));i=j;
 }
 return o;
}
struct Parser{
 std::vector<std::string> t;size_t p=0;
 std::string until(const std::string& end){std::string r;while(p<t.size()&&t[p]!=end){if(!r.empty())r+=' ';r+=t[p++];}return r;}
 bool node(DtbNode& n,const std::string& path){
  if(p>=t.size()||t[p++]!="{")return false;n.path=path;
  while(p<t.size()&&t[p]!="}"){
   if(t[p]==";"){++p;continue;}
   std::string a=t[p++];
   if(a=="/dts-v1/"||a=="/plugin/"){while(p<t.size()&&t[p]!=";")++p;if(p<t.size())++p;continue;}
   if(p<t.size()&&t[p]==":"){++p;if(p<t.size())a=t[p++];}
   if(p<t.size()&&t[p]=="{"){
    DtbNode c;c.name=a;std::string cp=path=="/"?"/"+a:path+"/"+a;
    if(!node(c,cp))return false;n.children[c.name]=std::move(c);if(p<t.size()&&t[p]==";")++p;
   }else{
    std::string v;
    if(p<t.size()&&t[p]=="="){++p;v=until(";");if(p<t.size())++p;}
    else{while(p<t.size()&&t[p]!=";")++p;if(p<t.size())++p;}
    n.properties[a]={a,v};
   }
  }
  if(p>=t.size())return false;++p;return true;
 }
};
DtbNode* find_node(DtbNode& root,const std::string& path){
 if(path=="/")return &root;if(path.size()<2)return nullptr;DtbNode* n=&root;size_t s=1;
 while(s<path.size()){size_t e=path.find('/',s);if(e==std::string::npos)e=path.size();auto it=n->children.find(path.substr(s,e-s));if(it==n->children.end())return nullptr;n=&it->second;s=e+1;}return n;
}
const DtbNode* find_node(const DtbNode& root,const std::string& p){return find_node(const_cast<DtbNode&>(root),p);}
std::string cat(std::string x){
 std::transform(x.begin(),x.end(),x.begin(),[](char c){return(char)std::tolower((unsigned char)c);});
 if(x.find("gpio")!=std::string::npos||x.find("pinctrl")!=std::string::npos||x.find("button")!=std::string::npos||x.find("joy")!=std::string::npos)return"controls";
 if(x.find("audio")!=std::string::npos||x.find("sound")!=std::string::npos||x.find("codec")!=std::string::npos)return"audio";
 if(x.find("display")!=std::string::npos||x.find("panel")!=std::string::npos||x.find("backlight")!=std::string::npos||x.find("lcd")!=std::string::npos)return"display";
 if(x.find("battery")!=std::string::npos||x.find("charger")!=std::string::npos||x.find("adc")!=std::string::npos)return"power";
 return"other";
}
void compare_node(const DtbNode& d,const DtbNode& r,std::vector<DtbChange>& o){
 for(const auto& kv:d.properties){auto it=r.properties.find(kv.first);if(it==r.properties.end())o.push_back({d.path+"#"+kv.first,"add",cat(d.path+" "+kv.first),"Missing in receptor"});else if(it->second.value!=kv.second.value)o.push_back({d.path+"#"+kv.first,"replace",cat(d.path+" "+kv.first),"Different value"});}
 for(const auto& kv:d.children){auto it=r.children.find(kv.first);if(it==r.children.end())o.push_back({kv.second.path,"node-add",cat(kv.second.path),"Node missing in receptor"});else compare_node(kv.second,it->second,o);}
}
void render_node(std::ostream& o,const DtbNode& n,int lv){
 std::string i(lv,'\t');o<<i<<(n.name.empty()?"/":n.name)<<" {\n";
 for(const auto& kv:n.properties){o<<i<<"\t"<<kv.second.name;if(!kv.second.value.empty())o<<" = "<<kv.second.value;o<<";\n";}
 for(const auto& kv:n.children){o<<"\n";render_node(o,kv.second,lv+1);}o<<i<<"};\n";
}
}
bool parse_dts_file(const std::string& f,DtbNode& root){std::ifstream in(f,std::ios::binary);if(!in)return false;std::stringstream s;s<<in.rdbuf();Parser p;p.t=lex(s.str());root={};root.path="/";root.name="";while(p.p<p.t.size()&&p.t[p.p]!="{")++p.p;return p.node(root,"/");}
bool render_dts(const DtbNode& root,const std::string& f){std::ofstream o(f,std::ios::binary);if(!o)return false;o<<"/dts-v1/;\n\n";render_node(o,root,0);return true;}
std::vector<DtbChange> compare_trees(const DtbNode& d,const DtbNode& r){std::vector<DtbChange>o;compare_node(d,r,o);return o;}
bool apply_change(DtbNode& r,const DtbNode& d,const DtbChange& c){auto x=c.path.rfind('#');if(x==std::string::npos)return false;auto* rn=find_node(r,c.path.substr(0,x));auto* dn=find_node(d,c.path.substr(0,x));if(!rn||!dn)return false;auto it=dn->properties.find(c.path.substr(x+1));if(it==dn->properties.end())return false;rn->properties[it->first]=it->second;return true;}
