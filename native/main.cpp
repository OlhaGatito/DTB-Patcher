#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdlib>
#include "dtc_bridge.h"
#include "dtb_model.hpp"

#pragma comment(lib,"comctl32.lib")
namespace fs=std::filesystem;
static HWND donorEdit,receiverEdit,listView,logEdit;
static std::vector<DtbChange> changes;
static DtbNode donorTree,receiverTree;

static std::string getText(HWND h){int n=GetWindowTextLengthA(h);std::string s(n,'\\0');GetWindowTextA(h,s.data(),n+1);return s;}
static void setText(HWND h,const std::string&s){SetWindowTextA(h,s.c_str());}
static void logLine(const std::string&s){int n=GetWindowTextLengthA(logEdit);SendMessageA(logEdit,EM_SETSEL,n,n);std::string x=s+"\\r\\n";SendMessageA(logEdit,EM_REPLACESEL,FALSE,(LPARAM)x.c_str());}
static void chooseFile(HWND edit){
 OPENFILENAMEA o{};char buf[MAX_PATH]="";o.lStructSize=sizeof(o);o.hwndOwner=GetParent(edit);o.lpstrFile=buf;o.nMaxFile=MAX_PATH;o.lpstrFilter="Device Tree Binary (*.dtb)\\0*.dtb\\0All files\\0*.*\\0";o.Flags=OFN_FILEMUSTEXIST;
 if(GetOpenFileNameA(&o))setText(edit,buf);
}
static std::string tempPath(const char* n){return (fs::temp_directory_path()/("dtbp_"+std::string(n))).string();}

static void analyze(){
 std::string d=getText(donorEdit),r=getText(receiverEdit);
 if(!fs::is_regular_file(d)||!fs::is_regular_file(r)){MessageBoxA(0,"Selecione o DTB Doador e o DTB Receptor.","DTB-Patcher",MB_ICONERROR);return;}
 std::string dd=tempPath("donor.dts"),rr=tempPath("receiver.dts");
 if(dtbp_dtc_decompile(d.c_str(),dd.c_str())||dtbp_dtc_decompile(r.c_str(),rr.c_str())){MessageBoxA(0,dtbp_dtc_error(),"DTC nativo",MB_ICONERROR);return;}
 if(!parse_dts_file(dd,donorTree)||!parse_dts_file(rr,receiverTree)){MessageBoxA(0,"Falha ao analisar a estrutura DTS.","DTB-Patcher",MB_ICONERROR);return;}
 changes=compare_trees(donorTree,receiverTree);ListView_DeleteAllItems(listView);
 for(int i=0;i<(int)changes.size();++i){
  LVITEMA it{};it.mask=LVIF_TEXT;it.iItem=i;it.pszText=(LPSTR)changes[i].path.c_str();ListView_InsertItemA(listView,&it);
  ListView_SetItemText(listView,i,1,(LPSTR)changes[i].kind.c_str());
  ListView_SetItemText(listView,i,2,(LPSTR)changes[i].category.c_str());
  ListView_SetItemText(listView,i,3,(LPSTR)changes[i].detail.c_str());
  ListView_SetCheckState(listView,i,TRUE);
 }
 logLine("DTC nativo integrado: "+std::string(dtbp_dtc_version()));
 logLine(std::to_string(changes.size())+" diferenças encontradas.");
}

static void build(){
 if(changes.empty()){analyze();if(changes.empty())return;}
 DtbNode patched=receiverTree;int applied=0;
 for(int i=0;i<(int)changes.size();++i)if(ListView_GetCheckState(listView,i))if(apply_change(patched,donorTree,changes[i]))++applied;
 if(!applied){MessageBoxA(0,"Marque pelo menos uma transferência.","DTB-Patcher",MB_ICONWARNING);return;}
 const char* home=std::getenv("USERPROFILE");if(!home)home="";
 fs::path dir=fs::path(home)/"Documents"/"DTB-Patcher"/"New dtb";fs::create_directories(dir);
 int n=1;fs::path out;do{out=dir/("patch-"+std::to_string(n++)+".dtb");}while(fs::exists(out));
 std::string dts=tempPath("patched.dts");
 if(!render_dts(patched,dts)){MessageBoxA(0,"Falha ao gerar DTS.","DTB-Patcher",MB_ICONERROR);return;}
 if(dtbp_dtc_compile(dts.c_str(),out.string().c_str())){MessageBoxA(0,dtbp_dtc_error(),"DTC nativo",MB_ICONERROR);return;}
 logLine("Gerado: "+out.string());MessageBoxA(0,out.string().c_str(),"Novo DTB",MB_ICONINFORMATION);
}

static LRESULT CALLBACK wndProc(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_CREATE:{
  CreateWindowA("STATIC","DOADOR",WS_CHILD|WS_VISIBLE,10,10,70,22,h,0,0,0);
  donorEdit=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER,80,8,650,25,h,0,0,0);
  CreateWindowA("BUTTON","Selecionar",WS_CHILD|WS_VISIBLE,740,8,100,25,h,(HMENU)101,0,0);
  CreateWindowA("STATIC","RECEPTOR",WS_CHILD|WS_VISIBLE,10,45,70,22,h,0,0,0);
  receiverEdit=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER,80,43,650,25,h,0,0,0);
  CreateWindowA("BUTTON","Selecionar",WS_CHILD|WS_VISIBLE,740,43,100,25,h,(HMENU)102,0,0);
  CreateWindowA("BUTTON","ANALISAR DTBs",WS_CHILD|WS_VISIBLE,10,78,130,28,h,(HMENU)103,0,0,0);
  CreateWindowA("BUTTON","GERAR NOVO DTB",WS_CHILD|WS_VISIBLE,145,78,140,28,h,(HMENU)104,0,0,0);
  listView=CreateWindowA(WC_LISTVIEWA,"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SHOWSELALWAYS,10,115,830,420,h,0,0,0);
  ListView_SetExtendedListViewStyle(listView,LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER);
  const char* heads[]={"Transferência","Tipo","Categoria","Detalhe"};int widths[]={390,110,110,220};
  for(int i=0;i<4;i++){LVCOLUMNA c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=(LPSTR)heads[i];c.cx=widths[i];ListView_InsertColumnA(listView,i,&c);}
  logEdit=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,10,545,830,100,h,0,0,0);
  CreateWindowA("STATIC","DTC nativo integrado — sem dtc.exe externo",WS_CHILD|WS_VISIBLE,10,650,450,22,h,0,0,0);
  break;}
 case WM_COMMAND:
  if(LOWORD(w)==101)chooseFile(donorEdit);
  else if(LOWORD(w)==102)chooseFile(receiverEdit);
  else if(LOWORD(w)==103)analyze();
  else if(LOWORD(w)==104)build();
  break;
 case WM_DESTROY:PostQuitMessage(0);break;
 }
 return DefWindowProcA(h,m,w,l);
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE,LPSTR,int){
 INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&ic);
 WNDCLASSA wc{};wc.hInstance=hi;wc.lpfnWndProc=wndProc;wc.lpszClassName="DTBPatcherNative";wc.hCursor=LoadCursorA(0,IDC_ARROW);RegisterClassA(&wc);
 CreateWindowA("DTBPatcherNative","DTB-Patcher — Native C++",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,80,870,720,0,0,hi,0);
 MSG msg;while(GetMessageA(&msg,0,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}return 0;
}
