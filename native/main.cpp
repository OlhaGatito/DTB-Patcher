/*
 * DTB-Patcher native Windows GUI.
 *
 * Design goals:
 *   - keep donor and receptor untouched;
 *   - show differences before applying anything;
 *   - call the integrated DTC directly, never dtc.exe via subprocess;
 *   - keep the interface simple enough for a ready-to-use Windows build.
 *
 * Visual identity uses the Sarue mascot from Gatito-Ports. The mascot is
 * rendered with Windows GDI+ so the final executable does not need an image
 * runtime or a separate GUI toolkit.
 */
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include "dtc_bridge.h"
#include "dtb_model.hpp"

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"gdiplus.lib")

using namespace Gdiplus;
namespace fs=std::filesystem;

enum : int { ID_DONOR=101, ID_RECEIVER=102, ID_ANALYZE=103, ID_BUILD=104, ID_SWAP=105, ID_ABOUT=106 };
static HWND g_main,g_donor,g_receiver,g_list,g_log,g_status,g_logo,g_title,g_sub;
static std::vector<DtbChange> g_changes;
static DtbNode g_donor_tree,g_receiver_tree;
static ULONG_PTR g_gdiplus=0;

static COLORREF bg(){return RGB(246,244,239);}
static COLORREF ink(){return RGB(55,48,42);}
static COLORREF accent(){return RGB(196,104,110);}
static COLORREF cream(){return RGB(255,252,246);}
static COLORREF panel(){return RGB(255,255,255);}

static std::string getText(HWND h){int n=GetWindowTextLengthA(h);std::string s(n,'\\0');if(n)GetWindowTextA(h,s.data(),n+1);return s;}
static void setText(HWND h,const std::string&s){SetWindowTextA(h,s.c_str());}
static void logLine(const std::string&s){int n=GetWindowTextLengthA(g_log);SendMessageA(g_log,EM_SETSEL,n,n);std::string x=s+"\\r\\n";SendMessageA(g_log,EM_REPLACESEL,FALSE,(LPARAM)x.c_str());}
static void setStatus(const std::string&s){SetWindowTextA(g_status,s.c_str());}

static void chooseFile(HWND edit){
 OPENFILENAMEA o{};char buf[MAX_PATH]="";
 o.lStructSize=sizeof(o);o.hwndOwner=g_main;o.lpstrFile=buf;o.nMaxFile=MAX_PATH;
 o.lpstrFilter="Device Tree Binary (*.dtb)\\0*.dtb\\0All files\\0*.*\\0";
 o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
 if(GetOpenFileNameA(&o))setText(edit,buf);
}

static std::string tempPath(const char* n){return (fs::temp_directory_path()/("dtbp_"+std::string(n))).string();}

static void swapFiles(){
 std::string d=getText(g_donor),r=getText(g_receiver);
 setText(g_donor,r);setText(g_receiver,d);
 setStatus("Doador e receptor invertidos.");
}

static void analyze(){
 std::string d=getText(g_donor),r=getText(g_receiver);
 if(!fs::is_regular_file(d)||!fs::is_regular_file(r)){
  MessageBoxA(g_main,"Selecione um DTB Doador e um DTB Receptor.","DTB-Patcher",MB_ICONERROR);return;
 }
 std::string dd=tempPath("donor.dts"),rr=tempPath("receiver.dts");
 if(dtbp_dtc_decompile(d.c_str(),dd.c_str())||dtbp_dtc_decompile(r.c_str(),rr.c_str())){
  MessageBoxA(g_main,dtbp_dtc_error(),"DTC nativo",MB_ICONERROR);return;
 }
 if(!parse_dts_file(dd,g_donor_tree)||!parse_dts_file(rr,g_receiver_tree)){
  MessageBoxA(g_main,"Falha ao analisar a estrutura DTS.","DTB-Patcher",MB_ICONERROR);return;
 }
 g_changes=compare_trees(g_donor_tree,g_receiver_tree);
 ListView_DeleteAllItems(g_list);
 for(int i=0;i<(int)g_changes.size();++i){
  LVITEMA it{};it.mask=LVIF_TEXT;it.iItem=i;it.pszText=(LPSTR)g_changes[i].path.c_str();
  ListView_InsertItemA(g_list,&it);
  ListView_SetItemText(g_list,i,1,(LPSTR)g_changes[i].kind.c_str());
  ListView_SetItemText(g_list,i,2,(LPSTR)g_changes[i].category.c_str());
  ListView_SetItemText(g_list,i,3,(LPSTR)g_changes[i].detail.c_str());
  ListView_SetCheckState(g_list,i,TRUE);
 }
 logLine("DTC nativo integrado: "+std::string(dtbp_dtc_version()));
 logLine(std::to_string(g_changes.size())+" diferencas encontradas.");
 setStatus(std::to_string(g_changes.size())+" diferencas prontas para selecao.");
}

static void build(){
 if(g_changes.empty()){analyze();if(g_changes.empty())return;}
 DtbNode patched=g_receiver_tree;int applied=0;
 for(int i=0;i<(int)g_changes.size();++i)
  if(ListView_GetCheckState(g_list,i)&&apply_change(patched,g_donor_tree,g_changes[i]))++applied;
 if(!applied){MessageBoxA(g_main,"Marque pelo menos uma transferencia.","DTB-Patcher",MB_ICONWARNING);return;}
 const char* home=std::getenv("USERPROFILE");if(!home)home="";
 fs::path dir=fs::path(home)/"Documents"/"DTB-Patcher"/"New dtb";fs::create_directories(dir);
 int n=1;fs::path out;do{out=dir/("patch-"+std::to_string(n++)+".dtb");}while(fs::exists(out));
 std::string dts=tempPath("patched.dts");
 if(!render_dts(patched,dts)){MessageBoxA(g_main,"Falha ao gerar DTS.","DTB-Patcher",MB_ICONERROR);return;}
 if(dtbp_dtc_compile(dts.c_str(),out.string().c_str())){MessageBoxA(g_main,dtbp_dtc_error(),"DTC nativo",MB_ICONERROR);return;}
 logLine("Novo DTB gerado: "+out.string());
 setStatus(std::to_string(applied)+" transferencia(s) aplicada(s).");
 MessageBoxA(g_main,out.string().c_str(),"DTB criado com sucesso",MB_ICONINFORMATION);
}

static void rounded(Graphics& g,RectF r,REAL radius,Color fill){
 SolidBrush b(fill);g.FillRoundedRectangle(&b,radius,radius,r);
}

static void drawSarue(Graphics& g,float x,float y,float scale){
 Pen outline(Color(255,70,54,45),3.2f*scale);
 SolidBrush body(Color(255,217,208,189)), face(Color(255,238,231,216)), pink(Color(255,215,143,145)), dark(Color(255,70,54,45)), white(Color(255,255,250,240));
 g.SetSmoothingMode(SmoothingModeAntiAlias);
 g.DrawArc(&outline, x+82*scale,y+82*scale,74*scale,28*scale,20,150);
 PointF ears1[]={PointF(x+28*scale,y+36*scale),PointF(x+17*scale,y+15*scale),PointF(x+42*scale,y+17*scale),PointF(x+54*scale,y+30*scale)};
 PointF ears2[]={PointF(x+74*scale,y+30*scale),PointF(x+86*scale,y+17*scale),PointF(x+111*scale,y+15*scale),PointF(x+100*scale,y+36*scale)};
 g.FillPolygon(&body,ears1,4);g.DrawPolygon(&outline,ears1,4);g.FillPolygon(&body,ears2,4);g.DrawPolygon(&outline,ears2,4);
 g.FillEllipse(&face,x+25*scale,y+17*scale,78*scale,76*scale);g.DrawEllipse(&outline,x+25*scale,y+17*scale,78*scale,76*scale);
 g.FillEllipse(&dark,x+27*scale,y+38*scale,31*scale,25*scale);g.FillEllipse(&dark,x+70*scale,y+38*scale,31*scale,25*scale);
 g.FillEllipse(&white,x+40*scale,y+47*scale,6*scale,8*scale);g.FillEllipse(&white,x+82*scale,y+47*scale,6*scale,8*scale);
 g.FillEllipse(&white,x+43*scale,y+57*scale,42*scale,29*scale);
 g.FillEllipse(&pink,x+57*scale,y+58*scale,14*scale,12*scale);g.FillEllipse(&pink,x+31*scale,y+24*scale,18*scale,10*scale);g.FillEllipse(&pink,x+79*scale,y+24*scale,18*scale,10*scale);
 g.DrawLine(&outline,x+64*scale,y+69*scale,x+64*scale,y+76*scale);
}

static LRESULT CALLBACK logoProc(HWND h,UINT m,WPARAM,LPARAM){
 if(m==WM_PAINT){
  PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);Graphics g(dc);RECT r;GetClientRect(h,&r);
  SolidBrush b(Color(255,255,252,246));g.FillRectangle(&b,0,0,r.right,r.bottom);
  drawSarue(g,6,2,1.05f);EndPaint(h,&ps);return 0;
 }
 return DefWindowProcA(h,m,0,0);
}

static void about(){
 std::string msg=
 "DTB-Patcher\\n\\n"
 "Ferramenta nativa para comparar e transferir propriedades selecionadas entre DTBs.\\n\\n"
 "Fluxo: Doador -> analise -> selecao -> novo DTB.\\n"
 "Os arquivos originais nunca sao sobrescritos.\\n\\n"
 "DTC integrado no mesmo processo.\\n"
 "DTC: Device Tree Compiler, projeto de David Gibson e colaboradores.\\n"
 "DTC e distribuido sob GPL-2.0-or-later.\\n\\n"
 "Mascote: Sarue / Gatito-Ports.\\n"
 "Projeto: OlhaGatito / DTB-Patcher";
 MessageBoxA(g_main,msg.c_str(),"Sobre o DTB-Patcher",MB_OK|MB_ICONINFORMATION);
}

static HFONT makeFont(int size,bool bold=false){
 return CreateFontA(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
}

static void setFont(HWND h,int size,bool bold=false){SendMessage(h,WM_SETFONT,(WPARAM)makeFont(size,bold),TRUE);}

static LRESULT CALLBACK wndProc(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_ERASEBKGND:return 1;
 case WM_CTLCOLORSTATIC:{
  HDC dc=(HDC)w;SetBkMode(dc,TRANSPARENT);SetTextColor(dc,ink());return (LRESULT)GetStockObject(NULL_BRUSH);
 }
 case WM_CTLCOLOREDIT:{
  HDC dc=(HDC)w;SetBkColor(dc,RGB(255,255,255));SetTextColor(dc,ink());static HBRUSH br=CreateSolidBrush(RGB(255,255,255));return(LRESULT)br;
 }
 case WM_PAINT:{
  PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HBRUSH br=CreateSolidBrush(bg());FillRect(dc,&r,br);DeleteObject(br);
  HBRUSH head=CreateSolidBrush(RGB(70,54,45));RECT hr={0,0,r.right,118};FillRect(dc,&hr,head);DeleteObject(head);
  HBRUSH line=CreateSolidBrush(accent());RECT ar={0,114,r.right,118};FillRect(dc,&ar,line);DeleteObject(line);
  EndPaint(h,&ps);return 0;
 }
 case WM_NOTIFY:{
  NMHDR* n=(NMHDR*)l;
  if(n->hwndFrom==g_list&&n->code==NM_CUSTOMDRAW){
   NMLVCUSTOMDRAW* cd=(NMLVCUSTOMDRAW*)l;
   if(cd->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
   if(cd->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){
    int row=(int)cd->nmcd.dwItemSpec;
    cd->clrText=ink();cd->clrTextBk=(row%2)?RGB(249,247,242):RGB(255,255,255);
    return CDRF_DODEFAULT;
   }
  }
  break;
 }
 case WM_COMMAND:
  switch(LOWORD(w)){
   case ID_DONOR:chooseFile(g_donor);break;
   case ID_RECEIVER:chooseFile(g_receiver);break;
   case ID_SWAP:swapFiles();break;
   case ID_ANALYZE:analyze();break;
   case ID_BUILD:build();break;
   case ID_ABOUT:about();break;
  }break;
 case WM_DESTROY:PostQuitMessage(0);break;
 }
 return DefWindowProcA(h,m,w,l);
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE,LPSTR,int){
 GdiplusStartupInput gi;GdiplusStartup(&g_gdiplus,&gi,nullptr);
 INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&ic);

 WNDCLASSA logoClass{};logoClass.hInstance=hi;logoClass.lpfnWndProc=logoProc;logoClass.lpszClassName="DTBPatcherLogo";logoClass.hCursor=LoadCursorA(0,IDC_ARROW);RegisterClassA(&logoClass);
 WNDCLASSA wc{};wc.hInstance=hi;wc.lpfnWndProc=wndProc;wc.lpszClassName="DTBPatcherNative";wc.hCursor=LoadCursorA(0,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH);RegisterClassA(&wc);

 g_main=CreateWindowA("DTBPatcherNative","DTB-Patcher",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,60,1050,790,0,0,hi,0);
 g_logo=CreateWindowA("DTBPatcherLogo","",WS_CHILD|WS_VISIBLE,18,10,112,104,g_main,0,hi,0);

 HWND title=CreateWindowA("STATIC","DTB-Patcher",WS_CHILD|WS_VISIBLE,145,20,420,36,g_main,0,hi,0);setFont(title,25,true);SetWindowLongPtrA(title,GWLP_USERDATA,(LONG_PTR)1);
 HWND sub=CreateWindowA("STATIC","Analise e transferencia segura de propriedades Device Tree",WS_CHILD|WS_VISIBLE,147,60,600,26,g_main,0,hi,0);setFont(sub,11,false);

 HWND aboutBtn=CreateWindowA("BUTTON","Sobre",WS_CHILD|WS_VISIBLE|BS_FLAT,900,24,95,30,g_main,(HMENU)ID_ABOUT,hi,0);setFont(aboutBtn,10,true);

 CreateWindowA("STATIC","DOADOR",WS_CHILD|WS_VISIBLE,24,140,90,22,g_main,0,hi,0);
 g_donor=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,120,136,730,30,g_main,0,hi,0);setFont(g_donor,10);
 HWND db=CreateWindowA("BUTTON","Selecionar",WS_CHILD|WS_VISIBLE|BS_FLAT,860,136,90,30,g_main,(HMENU)ID_DONOR,hi,0);setFont(db,9,true);

 CreateWindowA("STATIC","RECEPTOR",WS_CHILD|WS_VISIBLE,24,180,90,22,g_main,0,hi,0);
 g_receiver=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,120,176,730,30,g_main,0,hi,0);setFont(g_receiver,10);
 HWND rb=CreateWindowA("BUTTON","Selecionar",WS_CHILD|WS_VISIBLE|BS_FLAT,860,176,90,30,g_main,(HMENU)ID_RECEIVER,hi,0);setFont(rb,9,true);

 HWND sw=CreateWindowA("BUTTON","Trocar Doador / Receptor",WS_CHILD|WS_VISIBLE|BS_FLAT,24,220,205,32,g_main,(HMENU)ID_SWAP,hi,0);setFont(sw,9,true);
 HWND an=CreateWindowA("BUTTON","Analisar DTBs",WS_CHILD|WS_VISIBLE|BS_FLAT,240,220,160,32,g_main,(HMENU)ID_ANALYZE,hi,0);setFont(an,9,true);
 HWND bu=CreateWindowA("BUTTON","Gerar novo DTB",WS_CHILD|WS_VISIBLE|BS_FLAT,411,220,180,32,g_main,(HMENU)ID_BUILD,hi,0);setFont(bu,9,true);

 g_list=CreateWindowA(WC_LISTVIEWA,"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SHOWSELALWAYS,24,268,926,355,g_main,0,hi,0);
 ListView_SetExtendedListViewStyle(g_list,LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER);
 const char* heads[]={"Transferencia","Tipo","Categoria","Detalhe"};int widths[]={430,120,130,240};
 for(int i=0;i<4;i++){LVCOLUMNA c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=(LPSTR)heads[i];c.cx=widths[i];ListView_InsertColumnA(g_list,i,&c);}

 g_log=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,24,634,926,75,g_main,0,hi,0);setFont(g_log,9);
 g_status=CreateWindowA("STATIC","Pronto. Selecione dois DTBs para comecar.",WS_CHILD|WS_VISIBLE,24,720,926,25,g_main,0,hi,0);setFont(g_status,9,true);

 MSG msg;while(GetMessageA(&msg,0,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
 GdiplusShutdown(g_gdiplus);return 0;
}
