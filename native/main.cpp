/*
 * DTB-Patcher native Windows GUI.
 * Responsive Win32 interface with explicit DTB analysis and transfer groups.
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
#include <map>
#include <exception>
#include "dtc_bridge.h"
#include "dtb_model.hpp"

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"gdiplus.lib")

using namespace Gdiplus;
namespace fs=std::filesystem;

enum : int {
 ID_DONOR=101, ID_RECEIVER=102, ID_ANALYZE=103, ID_BUILD=104,
 ID_SWAP=105, ID_ABOUT=106,
 ID_DONOR_INFO=201, ID_RECEIVER_INFO=202,
 ID_LIST_CONTROLS=301, ID_LIST_AUDIO=302, ID_LIST_DISPLAY=303,
 ID_LIST_POWER=304, ID_LIST_OTHER=305
};

static HWND g_main=nullptr,g_donor=nullptr,g_receiver=nullptr,g_log=nullptr,g_status=nullptr,g_donorLabel=nullptr,g_receiverLabel=nullptr;
static HWND g_donorInfo=nullptr,g_receiverInfo=nullptr;
static HWND g_lists[5]{};
static std::vector<DtbChange> g_changes;
static std::vector<int> g_groupIndex[5];
static DtbNode g_donor_tree,g_receiver_tree;
static ULONG_PTR g_gdiplus=0;
static HFONT g_font=nullptr,g_fontBold=nullptr,g_fontSmall=nullptr;
static HBRUSH g_whiteBrush=nullptr,g_cardBrush=nullptr;
static bool g_analyzed=false;

static COLORREF bg(){return RGB(244,241,235);}
static COLORREF card(){return RGB(255,253,249);}
static COLORREF ink(){return RGB(55,48,42);}
static COLORREF muted(){return RGB(111,100,91);}
static COLORREF header(){return RGB(68,51,43);}
static COLORREF accent(){return RGB(196,104,110);}
static COLORREF line(){return RGB(224,216,205);}
static COLORREF white(){return RGB(255,255,255);}

static std::string getText(HWND h){
 int n=GetWindowTextLengthA(h);
 std::string s((size_t)n,'\0');
 if(n)GetWindowTextA(h,s.data(),n+1);
 return s;
}
static void setText(HWND h,const std::string&s){SetWindowTextA(h,s.c_str());}

static void logLine(const std::string&s){
 if(!g_log)return;
 int n=GetWindowTextLengthA(g_log);
 SendMessageA(g_log,EM_SETSEL,n,n);
 std::string x=s+"\r
";
 SendMessageA(g_log,EM_REPLACESEL,FALSE,(LPARAM)x.c_str());
}
static void setStatus(const std::string&s){if(g_status)setText(g_status,s);}

static std::string baseName(const std::string& p){
 try{return fs::path(p).filename().string();}catch(...){return p;}
}
static size_t countProperties(const DtbNode& n){
 size_t total=n.properties.size();
 for(const auto& c:n.children)total+=countProperties(c.second);
 return total;
}
static size_t countNodes(const DtbNode& n){
 size_t total=1;
 for(const auto& c:n.children)total+=countNodes(c.second);
 return total;
}

static std::string tempPath(const char* n){
 try{
  fs::path p=fs::temp_directory_path();
  return (p/("dtbp_"+std::string(n))).string();
 }catch(...){
  return std::string("dtbp_")+n;
 }
}

static void chooseFile(HWND edit){
 OPENFILENAMEA o{};char buf[MAX_PATH]="";
 o.lStructSize=sizeof(o);o.hwndOwner=g_main;o.lpstrFile=buf;o.nMaxFile=MAX_PATH;
 o.lpstrFilter="Device Tree Binary (*.dtb)\0*.dtb\0All files\0*.*\0";
 o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(GetOpenFileNameA(&o)){setText(edit,buf);g_analyzed=false;}
}

static void swapFiles(){
 std::string d=getText(g_donor),r=getText(g_receiver);
 setText(g_donor,r);setText(g_receiver,d);
 setText(g_donorInfo,"Aguardando analise");
 setText(g_receiverInfo,"Aguardando analise");
 g_analyzed=false;
 setStatus("Doador e receptor invertidos. Analise novamente.");
}

static int categoryIndex(const std::string& c){
 if(c=="controls")return 0;
 if(c=="audio")return 1;
 if(c=="display")return 2;
 if(c=="power")return 3;
 return 4;
}
static const char* categoryTitle(int i){
 static const char* a[]={"Controles","Audio","Display","Energia","Outros"};
 return a[i];
}
static const char* categoryDescription(int i){
 static const char* a[]={
  "GPIO, botoes, joystick e pinctrl",
  "Codec, I2S, routing e som",
  "LCD, painel, backlight e video",
  "Bateria, carregamento e ADC",
  "Propriedades fora das categorias acima"
 };
 return a[i];
}

static void clearLists(){
 for(int i=0;i<5;i++){
  g_groupIndex[i].clear();
  if(g_lists[i])ListView_DeleteAllItems(g_lists[i]);
 }
}
static void fillLists(){
 clearLists();
 for(int i=0;i<(int)g_changes.size();++i){
  int c=categoryIndex(g_changes[i].category);
  HWND lv=g_lists[c];
  if(!lv)continue;
  int row=ListView_GetItemCount(lv);
  LVITEMA it{};
  it.mask=LVIF_TEXT;
  it.iItem=row;
  it.pszText=(LPSTR)g_changes[i].path.c_str();
  int pos=ListView_InsertItem(lv,&it);
  if(pos>=0){
   ListView_SetItemText(lv,pos,1,(LPSTR)g_changes[i].kind.c_str());
   ListView_SetItemText(lv,pos,2,(LPSTR)g_changes[i].detail.c_str());
   ListView_SetCheckState(lv,pos,TRUE);
   g_groupIndex[c].push_back(i);
  }
 }
}

static void analyze(){
 try{
  std::string d=getText(g_donor),r=getText(g_receiver);
  if(d.empty()||r.empty()||!fs::is_regular_file(d)||!fs::is_regular_file(r)){
   MessageBoxA(g_main,"Selecione um DTB Doador e um DTB Receptor validos.","DTB-Patcher",MB_ICONERROR);
   return;
  }
  setStatus("Lendo Doador e Receptor...");
  logLine("Iniciando leitura dos dois DTBs.");

  std::string dd=tempPath("donor.dts"),rr=tempPath("receiver.dts");
  if(dtbp_dtc_decompile(d.c_str(),dd.c_str())){
   std::string e=dtbp_dtc_error();
   MessageBoxA(g_main,e.c_str(),"Falha ao ler o Doador",MB_ICONERROR);return;
  }
  if(dtbp_dtc_decompile(r.c_str(),rr.c_str())){
   std::string e=dtbp_dtc_error();
   MessageBoxA(g_main,e.c_str(),"Falha ao ler o Receptor",MB_ICONERROR);return;
  }
  if(!parse_dts_file(dd,g_donor_tree)){
   MessageBoxA(g_main,"O Doador foi convertido pelo DTC, mas a estrutura DTS nao pode ser analisada.","Analise do Doador",MB_ICONERROR);return;
  }
  if(!parse_dts_file(rr,g_receiver_tree)){
   MessageBoxA(g_main,"O Receptor foi convertido pelo DTC, mas a estrutura DTS nao pode ser analisada.","Analise do Receptor",MB_ICONERROR);return;
  }

  g_changes=compare_trees(g_donor_tree,g_receiver_tree);
  fillLists();
  g_analyzed=true;

  setText(g_donorInfo,baseName(d)+"  |  "+std::to_string(countNodes(g_donor_tree))+" nos  |  "+std::to_string(countProperties(g_donor_tree))+" propriedades");
  setText(g_receiverInfo,baseName(r)+"  |  "+std::to_string(countNodes(g_receiver_tree))+" nos  |  "+std::to_string(countProperties(g_receiver_tree))+" propriedades");
  logLine("DTC nativo integrado: "+std::string(dtbp_dtc_version()));
  logLine(std::to_string(g_changes.size())+" diferencas encontradas.");
  setStatus(std::to_string(g_changes.size())+" diferencas encontradas. Selecione o que deseja transferir.");
 }catch(const std::exception& e){
  logLine(std::string("ERRO: ")+e.what());
  MessageBoxA(g_main,e.what(),"Erro durante a analise",MB_ICONERROR);
  setStatus("A analise falhou. Nenhum DTB foi alterado.");
 }catch(...){
  logLine("ERRO desconhecido durante a analise.");
  MessageBoxA(g_main,"Ocorreu um erro inesperado durante a analise.","DTB-Patcher",MB_ICONERROR);
  setStatus("A analise falhou.");
 }
}

static bool anyChecked(int& applied){
 applied=0;
 for(int c=0;c<5;c++){
  for(int row=0;row<ListView_GetItemCount(g_lists[c]);row++){
   if(ListView_GetCheckState(g_lists[c],row))++applied;
  }
 }
 return applied>0;
}

static void build(){
 try{
  if(!g_analyzed){
   analyze();
   if(!g_analyzed)return;
  }
  if(g_changes.empty()){
   MessageBoxA(g_main,"A analise nao encontrou diferencas transferiveis.","Gerar novo DTB",MB_ICONINFORMATION);
   return;
  }

  int selected=0;
  if(!anyChecked(selected)){
   MessageBoxA(g_main,"Marque pelo menos uma transferencia antes de gerar o DTB.","Gerar novo DTB",MB_ICONWARNING);
   return;
  }

  setStatus("Aplicando transferencias selecionadas...");
  DtbNode patched=g_receiver_tree;
  int applied=0;
  for(int c=0;c<5;c++){
   for(int row=0;row<ListView_GetItemCount(g_lists[c]);row++){
    if(!ListView_GetCheckState(g_lists[c],row))continue;
    int gi=g_groupIndex[c][row];
    if(apply_change(patched,g_donor_tree,g_changes[gi]))++applied;
    else logLine("Ignorada: "+g_changes[gi].path+" ("+g_changes[gi].kind+")");
   }
  }

  if(!applied){
   MessageBoxA(g_main,"Nenhuma das transferencias selecionadas pode ser aplicada com seguranca pelo modelo atual.","Gerar novo DTB",MB_ICONWARNING);
   setStatus("Nenhuma transferencia aplicavel.");
   return;
  }

  const char* home=std::getenv("USERPROFILE");
  if(!home||!*home){
   MessageBoxA(g_main,"Nao foi possivel localizar a pasta do usuario (USERPROFILE).","Gerar novo DTB",MB_ICONERROR);
   return;
  }

  fs::path dir=fs::path(home)/"Documents"/"DTB-Patcher"/"New dtb";
  std::error_code ec;
  fs::create_directories(dir,ec);
  if(ec){
   MessageBoxA(g_main,("Nao foi possivel criar a pasta de saida:\r
"+dir.string()+"\r
\r
"+ec.message()).c_str(),"Gerar novo DTB",MB_ICONERROR);
   return;
  }

  int n=1;fs::path out;
  do{out=dir/("patch-"+std::to_string(n++)+".dtb");}while(fs::exists(out,ec)&&!ec);
  if(ec){
   MessageBoxA(g_main,("Nao foi possivel verificar o proximo nome de saida:\r
"+ec.message()).c_str(),"Gerar novo DTB",MB_ICONERROR);
   return;
  }

  std::string dts=tempPath("patched.dts");
  if(!render_dts(patched,dts)){
   MessageBoxA(g_main,"Falha ao gerar o DTS intermediario. O receptor original permanece intacto.","Gerar novo DTB",MB_ICONERROR);
   return;
  }
  setStatus("Compilando novo DTB com DTC nativo...");
  if(dtbp_dtc_compile(dts.c_str(),out.string().c_str())){
   std::string e=dtbp_dtc_error();
   MessageBoxA(g_main,e.c_str(),"Falha ao compilar novo DTB",MB_ICONERROR);
   setStatus("Falha na compilacao. Nenhum arquivo original foi alterado.");
   return;
  }
  if(!fs::is_regular_file(out)||fs::file_size(out,ec)==0||ec){
   MessageBoxA(g_main,"O DTC terminou, mas o arquivo DTB de saida nao foi validado.","Gerar novo DTB",MB_ICONERROR);
   return;
  }

  logLine("Novo DTB gerado: "+out.string());
  setStatus(std::to_string(applied)+" transferencia(s) aplicada(s).");
  MessageBoxA(g_main,out.string().c_str(),"DTB criado com sucesso",MB_ICONINFORMATION);
 }catch(const std::exception& e){
  logLine(std::string("ERRO AO GERAR: ")+e.what());
  MessageBoxA(g_main,e.what(),"Erro ao gerar novo DTB",MB_ICONERROR);
  setStatus("Falha controlada. Os DTBs originais nao foram alterados.");
 }catch(...){
  logLine("ERRO AO GERAR: excecao desconhecida.");
  MessageBoxA(g_main,"Ocorreu um erro inesperado ao gerar o novo DTB.\r
\r
Os arquivos originais nao foram alterados.","DTB-Patcher",MB_ICONERROR);
  setStatus("Falha controlada. Os DTBs originais nao foram alterados.");
 }
}

static void drawSarue(Graphics& g,float x,float y,float s){
 Pen outline(Color(255,70,54,45),3.0f*s);
 SolidBrush body(Color(255,217,208,189)),face(Color(255,238,231,216)),pink(Color(255,215,143,145)),dark(Color(255,70,54,45)),whiteB(Color(255,255,250,240));
 g.SetSmoothingMode(SmoothingModeAntiAlias);
 PointF e1[]={PointF(x+28*s,y+36*s),PointF(x+17*s,y+15*s),PointF(x+42*s,y+17*s),PointF(x+54*s,y+30*s)};
 PointF e2[]={PointF(x+74*s,y+30*s),PointF(x+86*s,y+17*s),PointF(x+111*s,y+15*s),PointF(x+100*s,y+36*s)};
 g.FillPolygon(&body,e1,4);g.DrawPolygon(&outline,e1,4);g.FillPolygon(&body,e2,4);g.DrawPolygon(&outline,e2,4);
 g.FillEllipse(&face,x+25*s,y+17*s,78*s,76*s);g.DrawEllipse(&outline,x+25*s,y+17*s,78*s,76*s);
 g.FillEllipse(&dark,x+27*s,y+38*s,31*s,25*s);g.FillEllipse(&dark,x+70*s,y+38*s,31*s,25*s);
 g.FillEllipse(&whiteB,x+40*s,y+47*s,6*s,8*s);g.FillEllipse(&whiteB,x+82*s,y+47*s,6*s,8*s);
 g.FillEllipse(&whiteB,x+43*s,y+57*s,42*s,29*s);g.FillEllipse(&pink,x+57*s,y+58*s,14*s,12*s);
 g.FillEllipse(&pink,x+31*s,y+24*s,18*s,10*s);g.FillEllipse(&pink,x+79*s,y+24*s,18*s,10*s);
 g.DrawLine(&outline,x+64*s,y+69*s,x+64*s,y+76*s);
 g.DrawLine(&outline,x+64*s,y+76*s,x+54*s,y+79*s);g.DrawLine(&outline,x+64*s,y+76*s,x+74*s,y+79*s);
}

static void drawIcon(Graphics& g,int kind,float x,float y,float s){
 Pen p(Color(255,196,104,110),2.5f*s);
 SolidBrush b(Color(255,196,104,110));
 g.SetSmoothingMode(SmoothingModeAntiAlias);
 if(kind==0){
  g.DrawArc(&p,x+5*s,y+12*s,34*s,22*s,200,140);g.DrawArc(&p,x+33*s,y+12*s,34*s,22*s,200,140);
  g.DrawLine(&p,x+18*s,y+23*s,x+18*s,y+31*s);g.DrawLine(&p,x+14*s,y+27*s,x+22*s,y+27*s);
  g.FillEllipse(&b,x+49*s,y+24*s,5*s,5*s);g.FillEllipse(&b,x+57*s,y+19*s,5*s,5*s);
 }else if(kind==1){
  g.FillRectangle(&b,x+8*s,y+17*s,8*s,12*s);PointF q[]={PointF(x+18*s,y+17*s),PointF(x+30*s,y+10*s),PointF(x+30*s,y+36*s),PointF(x+18*s,y+29*s)};g.FillPolygon(&b,q,4);
  g.DrawArc(&p,x+25*s,y+13*s,28*s,20*s,-65,130);g.DrawArc(&p,x+25*s,y+8*s,39*s,30*s,-65,130);
 }else if(kind==2){
  g.DrawRectangle(&p,x+5*s,y+8*s,58*s,32*s);g.DrawLine(&p,x+25*s,y+48*s,x+43*s,y+48*s);g.DrawLine(&p,x+34*s,y+40*s,x+34*s,y+48*s);
 }else if(kind==3){
  g.DrawRoundedRectangle(&p, x+10*s,y+8*s,46*s,32*s,4*s,4*s);g.FillRectangle(&b,x+56*s,y+18*s,7*s,12*s);g.FillRectangle(&b,x+15*s,y+13*s,30*s,22*s);
 }else{
  g.DrawEllipse(&p,x+10*s,y+10*s,45*s,45*s);g.DrawLine(&p,x+18*s,y+47*s,x+49*s,y+16*s);g.DrawLine(&p,x+18*s,y+16*s,x+49*s,y+47*s);
 }
}

static void roundFill(Graphics& g,SolidBrush& br,Pen& pn,int x,int y,int w,int h,float radius){ GraphicsPath path; path.AddArc(x,y,radius,radius,180,90); path.AddArc(x+w-radius,y,radius,radius,270,90); path.AddArc(x+w-radius,y+h-radius,radius,radius,0,90); path.AddArc(x,y+h-radius,radius,radius,90,90); path.CloseFigure(); g.FillPath(&br,&path); g.DrawPath(&pn,&path); }

static void drawCard(Graphics& g,int x,int y,int w,int h,int kind,int count){
 SolidBrush br(Color(255,255,253,249));Pen pn(Color(255,224,216,205),1.2f);
 roundFill(g,br,pn,x,y,w,h,10.0f);
 drawIcon(g,kind,(float)x+10,(float)y+7,0.65f);
 FontFamily ff(L"Segoe UI");Font title(&ff,13,FontStyleBold,UnitPixel);Font small(&ff,10,FontStyleRegular,UnitPixel);
 SolidBrush txt(Color(255,55,48,42)),mut(Color(255,111,100,91));
 std::wstring wt;for(char c:std::string(categoryTitle(kind)))wt.push_back((wchar_t)(unsigned char)c);
 std::wstring wd;for(char c:std::string(categoryDescription(kind)))wd.push_back((wchar_t)(unsigned char)c);
 g.DrawString(wt.c_str(),-1,&title,PointF((REAL)x+56,(REAL)y+9),&txt);
 g.DrawString(wd.c_str(),-1,&small,PointF((REAL)x+56,(REAL)y+28),&mut);
 std::wstring wc=std::to_wstring(count)+L" item(ns)"; g.DrawString(wc.c_str(),-1,&small,PointF((REAL)x+56,(REAL)y+41),&mut);
}

static HFONT makeFont(int size,bool bold=false){
 return CreateFontA(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
}
static void setFont(HWND h,int size,bool bold=false){SendMessage(h,WM_SETFONT,(WPARAM)makeFont(size,bold),TRUE);}

static void layout(){
 RECT r{};GetClientRect(g_main,&r);
 int W=r.right,H=r.bottom;
 int margin=24;
 int headerH=112;
 int y=headerH+16;
 int gap=10;
 int colW=(W-2*margin-gap)/2;
 int rowH=62;

 MoveWindow(g_donorLabel,margin,y+28,80,20,TRUE);
 MoveWindow(g_receiverLabel,margin,y+28+rowH,80,20,TRUE);
 MoveWindow(g_donor,margin+90,y+26,colW-170,30,TRUE);
 MoveWindow(g_receiver,margin+90,y+26+rowH,colW-170,30,TRUE);
 MoveWindow(GetDlgItem(g_main,ID_DONOR),W-margin-72,y+26,72,30,TRUE);
 MoveWindow(GetDlgItem(g_main,ID_RECEIVER),W-margin-72,y+26+rowH,72,30,TRUE);
 MoveWindow(g_donorInfo,margin+90,y+2,colW-162,20,TRUE);
 MoveWindow(g_receiverInfo,margin+90,y+2+rowH,colW-162,20,TRUE);

 int rightX=margin+colW+gap;
 MoveWindow(GetDlgItem(g_main,ID_SWAP),rightX,y+8,180,32,TRUE);
 MoveWindow(GetDlgItem(g_main,ID_ANALYZE),rightX+190,y+8,145,32,TRUE);
 MoveWindow(GetDlgItem(g_main,ID_BUILD),rightX,y+46,335,34,TRUE);
 MoveWindow(GetDlgItem(g_main,ID_ABOUT),W-margin-80,18,80,30,TRUE);

 int cardsTop=y+2*rowH+18;
 int available=H-cardsTop-112;
 if(available<230)available=230;
 int cardGap=12;
 int cardW=(W-2*margin-2*cardGap)/3;
 int cardH=(available-cardGap)/2;

 int xs[5]={margin,margin+cardW+cardGap,margin+2*(cardW+cardGap),margin,margin+cardW+cardGap};
 int ys[5]={cardsTop,cardsTop,cardsTop,cardsTop+cardH+cardGap,cardsTop+cardH+cardGap};
 int ws[5]={cardW,cardW,cardW,cardW,cardW};
 int hs[5]={cardH,cardH,cardH,cardH,cardH};

 for(int i=0;i<5;i++){
  int header=52;
  MoveWindow(g_lists[i],xs[i]+8,ys[i]+header,ws[i]-16,hs[i]-header-8,TRUE);
  InvalidateRect(g_main,nullptr,FALSE);
 }

 int logY=H-88;
 MoveWindow(g_log,margin,logY,W-2*margin-170,60,TRUE);
 MoveWindow(g_status,W-2*margin-150,logY,150,60,TRUE);
}

static LRESULT CALLBACK wndProc(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_ERASEBKGND:return 1;
 case WM_SIZE:if(g_main)layout();return 0;
 case WM_CTLCOLORSTATIC:{
  HDC dc=(HDC)w;HWND child=(HWND)l;
  SetBkMode(dc,TRANSPARENT);
  if(child==GetDlgItem(g_main,ID_DONOR_INFO)||child==GetDlgItem(g_main,ID_RECEIVER_INFO)){
   SetTextColor(dc,muted());return(LRESULT)GetStockObject(NULL_BRUSH);
  }
  if(child==g_status){SetTextColor(dc,muted());return(LRESULT)GetStockObject(NULL_BRUSH);}
  SetTextColor(dc,ink());return(LRESULT)GetStockObject(NULL_BRUSH);
 }
 case WM_CTLCOLOREDIT:{
  HDC dc=(HDC)w;SetBkColor(dc,white());SetTextColor(dc,ink());return(LRESULT)g_whiteBrush;
 }
 case WM_PAINT:{
  PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
  HBRUSH br=CreateSolidBrush(bg());FillRect(dc,&r,br);DeleteObject(br);
  HBRUSH hd=CreateSolidBrush(header());RECT hr={0,0,r.right,112};FillRect(dc,&hr,hd);DeleteObject(hd);
  HBRUSH ac=CreateSolidBrush(accent());RECT ar={0,108,r.right,112};FillRect(dc,&ar,ac);DeleteObject(ac);

  Graphics g(dc);
  drawSarue(g,24,5,0.72f);

  FontFamily ff(L"Segoe UI");Font t(&ff,26,FontStyleBold,UnitPixel);Font s(&ff,11,FontStyleRegular,UnitPixel);
  SolidBrush wt(Color(255,255,250,245)),ws(Color(255,226,214,207));
  g.DrawString(L"DTB-Patcher",-1,&t,PointF(118,22),&wt);
  g.DrawString(L"Analise, compare e transfira propriedades com controle explicito",-1,&s,PointF(120,61),&ws);

  for(int i=0;i<5;i++){
   RECT q{};GetWindowRect(g_lists[i],&q);POINT p1{q.left,q.top},p2{q.right,q.bottom};
   ScreenToClient(h,&p1);ScreenToClient(h,&p2);
   drawCard(g,p1.x-8,p1.y-52,p2.x-p1.x+16,p2.y-p1.y+60,i,ListView_GetItemCount(g_lists[i]));
  }
  EndPaint(h,&ps);return 0;
 }
 case WM_NOTIFY:{
  NMHDR* n=(NMHDR*)l;
  for(int i=0;i<5;i++)if(n->hwndFrom==g_lists[i]&&n->code==NM_CUSTOMDRAW){
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
   case ID_ABOUT:{
    const char* msg="DTB-Patcher\
\
Doador -> analise -> selecao -> novo DTB.\
\
Os arquivos originais nunca sao sobrescritos.\
DTC integrado no mesmo processo.\
\
Mascote: Sarue / Gatito-Ports.";
    MessageBoxA(g_main,msg,"Sobre o DTB-Patcher",MB_OK|MB_ICONINFORMATION);break;
   }
  }break;
 case WM_DESTROY:PostQuitMessage(0);break;
 }
 return DefWindowProcA(h,m,w,l);
}

static HWND makeList(HWND parent,HINSTANCE hi,int id){
 HWND lv=CreateWindowExA(WS_EX_CLIENTEDGE,WC_LISTVIEWA,"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,
 0,0,100,100,parent,(HMENU)id,hi,nullptr);
 ListView_SetExtendedListViewStyle(lv,LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER);
 const char* heads[]={"Propriedade / caminho","Tipo","Detalhe"};
 int widths[]={320,95,260};
 for(int i=0;i<3;i++){LVCOLUMNA c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=(LPSTR)heads[i];c.cx=widths[i];ListView_InsertColumn(lv,i,&c);}
 setFont(lv,9);
 return lv;
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE,HINSTANCE,LPSTR,int){
 try{
  GdiplusStartupInput gi;
  if(GdiplusStartup(&g_gdiplus,&gi,nullptr)!=Ok)return 1;
  INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&ic);

  g_whiteBrush=CreateSolidBrush(white());
  g_cardBrush=CreateSolidBrush(card());

  WNDCLASSA wc{};
  wc.hInstance=hi;wc.lpfnWndProc=wndProc;wc.lpszClassName="DTBPatcherNative";
  wc.hCursor=LoadCursorA(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH);
  RegisterClassA(&wc);

  g_main=CreateWindowA("DTBPatcherNative","DTB-Patcher",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
   0,0,1280,900,nullptr,nullptr,hi,nullptr);
  if(!g_main)return 1;

  HWND donorLabel=CreateWindowA("STATIC","DOADOR",WS_CHILD|WS_VISIBLE,0,0,80,20,g_main,nullptr,hi,nullptr);
  HWND recvLabel=CreateWindowA("STATIC","RECEPTOR",WS_CHILD|WS_VISIBLE,0,0,80,20,g_main,nullptr,hi,nullptr);
  setFont(donorLabel,10,true);setFont(recvLabel,10,true);

  g_donor=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,0,0,100,30,g_main,nullptr,hi,nullptr);
  g_receiver=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,0,0,100,30,g_main,nullptr,hi,nullptr);
  setFont(g_donor,10);setFont(g_receiver,10);

  HWND db=CreateWindowA("BUTTON","Abrir",WS_CHILD|WS_VISIBLE|BS_FLAT,0,0,72,30,g_main,(HMENU)ID_DONOR,hi,nullptr);
  HWND rb=CreateWindowA("BUTTON","Abrir",WS_CHILD|WS_VISIBLE|BS_FLAT,0,0,72,30,g_main,(HMENU)ID_RECEIVER,hi,nullptr);
  setFont(db,9,true);setFont(rb,9,true);

  g_donorInfo=CreateWindowA("STATIC","Aguardando analise",WS_CHILD|WS_VISIBLE,0,0,100,20,g_main,(HMENU)ID_DONOR_INFO,hi,nullptr);
  g_receiverInfo=CreateWindowA("STATIC","Aguardando analise",WS_CHILD|WS_VISIBLE,0,0,100,20,g_main,(HMENU)ID_RECEIVER_INFO,hi,nullptr);
  setFont(g_donorInfo,9);setFont(g_receiverInfo,9);

  HWND sw=CreateWindowA("BUTTON","Trocar Doador / Receptor",WS_CHILD|WS_VISIBLE|BS_FLAT,0,0,180,34,g_main,(HMENU)ID_SWAP,hi,nullptr);
  HWND an=CreateWindowA("BUTTON","Analisar DTBs",WS_CHILD|WS_VISIBLE|BS_FLAT,0,0,145,34,g_main,(HMENU)ID_ANALYZE,hi,nullptr);
  HWND bu=CreateWindowA("BUTTON","Gerar novo DTB",WS_CHILD|WS_VISIBLE|BS_FLAT,0,0,180,34,g_main,(HMENU)ID_BUILD,hi,nullptr);
  HWND ab=CreateWindowA("BUTTON","Sobre",WS_CHILD|WS_VISIBLE|BS_FLAT,0,0,80,30,g_main,(HMENU)ID_ABOUT,hi,nullptr);
  setFont(sw,9,true);setFont(an,9,true);setFont(bu,9,true);setFont(ab,9,true);

  for(int i=0;i<5;i++)g_lists[i]=makeList(g_main,hi,ID_LIST_CONTROLS+i);

  g_log=CreateWindowA("EDIT","Log de operacao:\r
",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,0,0,100,60,g_main,nullptr,hi,nullptr);
  setFont(g_log,9);
  g_status=CreateWindowA("STATIC","Pronto. Selecione os dois DTBs para comecar.",WS_CHILD|WS_VISIBLE|SS_CENTER,0,0,100,60,g_main,nullptr,hi,nullptr);
  setFont(g_status,9,true);

  ShowWindow(g_main,SW_SHOWMAXIMIZED);
  UpdateWindow(g_main);
  layout();

  MSG msg{};
  while(GetMessageA(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}

  DeleteObject(g_whiteBrush);DeleteObject(g_cardBrush);
  GdiplusShutdown(g_gdiplus);
  return 0;
 }catch(...){
  return 1;
 }
}
