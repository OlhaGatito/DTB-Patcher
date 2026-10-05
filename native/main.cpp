/*
 * DTB-Patcher native Windows GUI.
 *
 * UI model:
 *   Doador -> semantic comparison -> explicit donor selection -> Receiver base
 *   -> replace compatible receiver properties -> DTC -> validated new DTB.
 *
 * The comparison view deliberately avoids exposing raw DTB noise. Each
 * category is split into side-by-side Doador/Receptor values and only the
 * Doador side is selectable for transfer.
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
#include <exception>
#include <cstdio>
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

static HWND g_main=nullptr;
static HWND g_donor=nullptr,g_receiver=nullptr;
static HWND g_donorInfo=nullptr,g_receiverInfo=nullptr;
static HWND g_log=nullptr,g_status=nullptr;
static HWND g_lists[5]{};
static std::vector<DtbTransferItem> g_items;
static DtbNode g_donor_tree,g_receiver_tree;
static ULONG_PTR g_gdiplus=0;
static HFONT g_fonts[4]{};
static HBRUSH g_whiteBrush=nullptr;
static bool g_analyzed=false;

static COLORREF bg(){return RGB(244,241,235);}
static COLORREF card(){return RGB(255,253,249);}
static COLORREF ink(){return RGB(55,48,42);}
static COLORREF muted(){return RGB(111,100,91);}
static COLORREF header(){return RGB(68,51,43);}
static COLORREF accent(){return RGB(196,104,110);}
static COLORREF line(){return RGB(224,216,205);}
static COLORREF white(){return RGB(255,255,255);}
static COLORREF good(){return RGB(50,115,74);}
static COLORREF bad(){return RGB(155,65,65);}

static std::string getText(HWND h){
    int n=GetWindowTextLengthA(h);
    if(n<=0)return {};
    std::string s((size_t)n+1,'\0');
    GetWindowTextA(h,s.data(),n+1);
    s.resize((size_t)n);
    return s;
}

static void setText(HWND h,const std::string& s){
    if(h)SetWindowTextA(h,s.c_str());
}

static void logLine(const std::string& s){
    if(!g_log)return;
    int n=GetWindowTextLengthA(g_log);
    SendMessageA(g_log,EM_SETSEL,n,n);
    std::string x=s+"\r\n";
    SendMessageA(g_log,EM_REPLACESEL,FALSE,(LPARAM)x.c_str());
}

static void setStatus(const std::string& s){
    if(g_status)setText(g_status,s);
}

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
        return (p/("dtbp_"+std::to_string(GetCurrentProcessId())+"_"+std::string(n))).string();
    }catch(...){
        return std::string("dtbp_")+n;
    }
}

static void chooseFile(HWND edit){
    OPENFILENAMEA o{};
    char buf[MAX_PATH]="";
    o.lStructSize=sizeof(o);
    o.hwndOwner=g_main;
    o.lpstrFile=buf;
    o.nMaxFile=MAX_PATH;
    o.lpstrFilter="Device Tree Binary (*.dtb)\0*.dtb\0All files\0*.*\0";
    o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetOpenFileNameA(&o)){
        setText(edit,buf);
        g_analyzed=false;
        setStatus("DTB alterado. Analise novamente para atualizar a comparacao.");
    }
}

static void swapFiles(){
    std::string d=getText(g_donor),r=getText(g_receiver);
    setText(g_donor,r);setText(g_receiver,d);
    setText(g_donorInfo,"Aguardando analise");
    setText(g_receiverInfo,"Aguardando analise");
    g_analyzed=false;
    g_items.clear();
    for(auto lv:g_lists)if(lv)ListView_DeleteAllItems(lv);
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
        "Botoes fisicos e GPIOs necessarios",
        "Codec, I2S, DAI e roteamento",
        "Painel, backlight e video",
        "Bateria, carregador e ADC",
        "Outras diferencas no mesmo bloco"
    };
    return a[i];
}

static std::string prettyLabel(std::string s){
    if(s.empty())return "Sem nome";
    for(char& c:s){
        if(c=='_'||c=='-')c=' ';
    }
    std::string out;
    bool up=true;
    for(char c:s){
        if(up&&c>='a'&&c<='z')c=(char)(c-'a'+'A');
        out+=c;
        up=(c==' ');
    }
    return out;
}

static std::string compactText(std::string s,size_t n=180){
    for(char& c:s)if(c=='\r'||c=='\n')c=' ';
    if(s.size()<=n)return s;
    return s.substr(0,n-3)+"...";
}

static int categoryItemCount(int c){
    int n=0;
    for(const auto& x:g_items)if(categoryIndex(x.category)==c)++n;
    return n;
}

static int categoryCompatibleCount(int c){
    int n=0;
    for(const auto& x:g_items)if(categoryIndex(x.category)==c&&x.compatible)++n;
    return n;
}

static void clearLists(){
    for(auto lv:g_lists)if(lv)ListView_DeleteAllItems(lv);
}

static void fillLists(){
    clearLists();

    for(int i=0;i<(int)g_items.size();++i){
        const auto& x=g_items[i];
        int c=categoryIndex(x.category);
        HWND lv=g_lists[c];
        if(!lv)continue;

        LVITEMA it{};
        it.mask=LVIF_TEXT|LVIF_PARAM;
        it.iItem=ListView_GetItemCount(lv);
        it.lParam=(LPARAM)i;
        it.pszText=(LPSTR)prettyLabel(x.label).c_str();

        int row=ListView_InsertItem(lv,&it);
        if(row<0)continue;

        std::string donor=x.donorValue.empty()?"—":compactText(x.donorValue);
        std::string receiver=x.receiverValue.empty()?"—":compactText(x.receiverValue);
        std::string result=x.compatible?"COMPATIVEL":"NAO COMPATIVEL";

        ListView_SetItemText(lv,row,2,(LPSTR)donor.c_str());
        ListView_SetItemText(lv,row,3,(LPSTR)receiver.c_str());
        ListView_SetItemText(lv,row,4,(LPSTR)result.c_str());

        // Explicit selection is always opt-in. Incompatible rows cannot be
        // selected by the LVN_ITEMCHANGING guard below.
        ListView_SetCheckState(lv,row,FALSE);
    }
}

static bool checkedCompatibleCount(int& selected){
    selected=0;
    for(auto lv:g_lists){
        if(!lv)continue;
        int rows=ListView_GetItemCount(lv);
        for(int row=0;row<rows;++row){
            if(!ListView_GetCheckState(lv,row))continue;
            LVITEMA it{};
            it.mask=LVIF_PARAM;
            it.iItem=row;
            if(!ListView_GetItem(lv,&it))continue;
            int idx=(int)it.lParam;
            if(idx>=0&&idx<(int)g_items.size()&&g_items[idx].compatible)++selected;
        }
    }
    return selected>0;
}

static void analyze(){
    try{
        std::string d=getText(g_donor),r=getText(g_receiver);

        if(d.empty()||r.empty()){
            MessageBoxA(g_main,
                "Selecione um DTB Doador e um DTB Receptor.\r\n\r\n"
                "O Doador fornece os blocos selecionados; o Receptor permanece como base.",
                "DTB-Patcher",MB_OK|MB_ICONWARNING);
            return;
        }
        if(!fs::is_regular_file(d)||!fs::is_regular_file(r)){
            MessageBoxA(g_main,
                "Um dos caminhos selecionados nao aponta para um arquivo DTB valido.",
                "DTB-Patcher",MB_OK|MB_ICONERROR);
            return;
        }

        setStatus("Convertendo Doador e Receptor com o DTC nativo...");
        logLine("=== NOVA ANALISE ===");
        logLine("Doador: "+d);
        logLine("Receptor: "+r);

        std::string dd=tempPath("donor.dts");
        std::string rr=tempPath("receiver.dts");

        if(dtbp_dtc_decompile(d.c_str(),dd.c_str())){
            std::string e=dtbp_dtc_error();
            logLine("Falha no Doador: "+e);
            MessageBoxA(g_main,e.c_str(),"Falha ao ler o Doador",MB_OK|MB_ICONERROR);
            setStatus("Falha ao ler o Doador. Nenhum arquivo foi alterado.");
            return;
        }

        if(dtbp_dtc_decompile(r.c_str(),rr.c_str())){
            std::string e=dtbp_dtc_error();
            logLine("Falha no Receptor: "+e);
            MessageBoxA(g_main,e.c_str(),"Falha ao ler o Receptor",MB_OK|MB_ICONERROR);
            setStatus("Falha ao ler o Receptor. Nenhum arquivo foi alterado.");
            return;
        }

        DtbNode donor,receiver;
        if(!parse_dts_file(dd,donor)){
            MessageBoxA(g_main,
                "O DTC leu o Doador, mas a estrutura DTS nao pode ser analisada.",
                "Falha de analise",MB_OK|MB_ICONERROR);
            return;
        }
        if(!parse_dts_file(rr,receiver)){
            MessageBoxA(g_main,
                "O DTC leu o Receptor, mas a estrutura DTS nao pode ser analisada.",
                "Falha de analise",MB_OK|MB_ICONERROR);
            return;
        }

        g_donor_tree=std::move(donor);
        g_receiver_tree=std::move(receiver);
        g_items=build_transfer_plan(g_donor_tree,g_receiver_tree);
        fillLists();
        g_analyzed=true;

        setText(g_donorInfo,
            baseName(d)+"  |  "+std::to_string(countNodes(g_donor_tree))+
            " nos  |  "+std::to_string(countProperties(g_donor_tree))+" props");
        setText(g_receiverInfo,
            baseName(r)+"  |  "+std::to_string(countNodes(g_receiver_tree))+
            " nos  |  "+std::to_string(countProperties(g_receiver_tree))+" props");

        int compatible=0;
        for(const auto& x:g_items)if(x.compatible)++compatible;

        logLine("DTC nativo: "+std::string(dtbp_dtc_version()));
        logLine(std::to_string(g_items.size())+" blocos funcionais encontrados.");
        logLine(std::to_string(compatible)+" blocos com par compativel no Receptor.");
        logLine("Somente propriedades que ja existem no Receptor podem ser substituidas.");

        setStatus(std::to_string(compatible)+" bloco(s) compativel(is). "
                  "Marque no Doador o que deseja transferir.");
    }catch(const std::exception& e){
        logLine(std::string("ERRO NA ANALISE: ")+e.what());
        MessageBoxA(g_main,e.what(),"Erro durante a analise",MB_OK|MB_ICONERROR);
        setStatus("Falha controlada. Nenhum DTB original foi alterado.");
    }catch(...){
        logLine("ERRO NA ANALISE: excecao desconhecida.");
        MessageBoxA(g_main,
            "Ocorreu um erro inesperado durante a analise.",
            "DTB-Patcher",MB_OK|MB_ICONERROR);
        setStatus("Falha controlada. Nenhum DTB original foi alterado.");
    }
}

static bool applySelected(DtbNode& patched,int& applied,int& skipped){
    applied=0;skipped=0;

    for(auto lv:g_lists){
        if(!lv)continue;
        int rows=ListView_GetItemCount(lv);
        for(int row=0;row<rows;++row){
            if(!ListView_GetCheckState(lv,row))continue;

            LVITEMA it{};
            it.mask=LVIF_PARAM;
            it.iItem=row;
            if(!ListView_GetItem(lv,&it)){
                ++skipped;
                continue;
            }

            int idx=(int)it.lParam;
            if(idx<0||idx>=(int)g_items.size()){
                ++skipped;
                continue;
            }

            const auto& item=g_items[idx];
            if(!item.compatible){
                ++skipped;
                continue;
            }

            if(apply_transfer_item(patched,g_donor_tree,item)){
                ++applied;
                logLine("Transferido: "+prettyLabel(item.label)+
                        " ["+item.category+"]");
            }else{
                ++skipped;
                logLine("Nao aplicado: "+prettyLabel(item.label)+
                        " ["+item.category+"]");
            }
        }
    }
    return applied>0;
}

static bool nextOutputPath(fs::path& out){
    const char* home=std::getenv("USERPROFILE");
    if(!home||!*home)return false;

    fs::path dir=fs::path(home)/"Documents"/"DTB-Patcher"/"New dtb";
    std::error_code ec;
    fs::create_directories(dir,ec);
    if(ec)return false;

    for(int n=1;n<100000;n++){
        fs::path candidate=dir/("patch-"+std::to_string(n)+".dtb");
        if(!fs::exists(candidate,ec)&&!ec){
            out=candidate;
            return true;
        }
        if(ec)return false;
    }
    return false;
}

static void build(){
    try{
        if(!g_analyzed){
            analyze();
            if(!g_analyzed)return;
        }

        int selected=0;
        if(!checkedCompatibleCount(selected)){
            MessageBoxA(g_main,
                "Marque pelo menos um bloco COMPATIVEL na coluna de selecao do Doador.",
                "Gerar novo DTB",MB_OK|MB_ICONWARNING);
            setStatus("Nenhuma transferencia foi selecionada.");
            return;
        }

        setStatus("Criando DTS temporario a partir do Receptor...");
        logLine("=== GERACAO ===");
        logLine("Base: Receptor. As selecoes do Doador serao aplicadas por bloco.");

        DtbNode patched=g_receiver_tree;
        int applied=0,skipped=0;

        if(!applySelected(patched,applied,skipped)||applied<=0){
            MessageBoxA(g_main,
                "Nenhum bloco selecionado pode ser aplicado ao Receptor.\r\n\r\n"
                "O Receptor original permanece intacto.",
                "Gerar novo DTB",MB_OK|MB_ICONWARNING);
            setStatus("Nenhuma transferencia aplicavel.");
            return;
        }

        fs::path out;
        if(!nextOutputPath(out)){
            MessageBoxA(g_main,
                "Nao foi possivel criar/verificar a pasta de saida:\r\n"
                "Documents\\DTB-Patcher\\New dtb",
                "Gerar novo DTB",MB_OK|MB_ICONERROR);
            setStatus("Falha ao preparar a pasta de saida.");
            return;
        }

        std::string dts=tempPath("patched.dts");
        if(!render_dts(patched,dts)){
            MessageBoxA(g_main,
                "Falha ao gerar o DTS intermediario.\r\n\r\n"
                "Nenhum DTB original foi alterado.",
                "Gerar novo DTB",MB_OK|MB_ICONERROR);
            setStatus("Falha ao gerar o DTS. Receptor preservado.");
            return;
        }

        logLine("DTS temporario criado a partir do Receptor.");
        setStatus("Compilando o novo DTS com o DTC integrado...");

        if(dtbp_dtc_compile(dts.c_str(),out.string().c_str())){
            std::string e=dtbp_dtc_error();
            logLine("Falha de compilacao: "+e);
            MessageBoxA(g_main,e.c_str(),
                "Falha ao compilar novo DTB",MB_OK|MB_ICONERROR);
            setStatus("Falha na compilacao. Originais preservados.");
            return;
        }

        std::error_code ec;
        if(!fs::is_regular_file(out)||fs::file_size(out,ec)==0||ec){
            MessageBoxA(g_main,
                "O DTC terminou, mas o DTB de saida nao passou na validacao basica.",
                "Gerar novo DTB",MB_OK|MB_ICONERROR);
            setStatus("Saida invalida. Originais preservados.");
            return;
        }

        // Round-trip validation is mandatory before presenting success.
        setStatus("Validando o DTB gerado...");
        std::string verifyDts=tempPath("verify.dts");
        DtbNode verifyTree;

        if(dtbp_dtc_decompile(out.string().c_str(),verifyDts.c_str())||
           !parse_dts_file(verifyDts,verifyTree)){
            std::string e=dtbp_dtc_error();
            if(e.empty())e="O DTB foi criado, mas nao pode ser lido novamente pelo DTC.";
            logLine("VALIDACAO FALHOU: "+e);
            MessageBoxA(g_main,
                (e+"\r\n\r\nO arquivo foi reprovado e nao deve ser usado no hardware.").c_str(),
                "Falha na validacao",MB_OK|MB_ICONERROR);
            setStatus("DTB reprovado na validacao. Nao use no hardware.");
            return;
        }

        logLine("Round-trip DTS -> DTB -> DTS: OK.");
        logLine("Transferencias aplicadas: "+std::to_string(applied)+
                " | ignoradas: "+std::to_string(skipped));
        logLine("Novo DTB: "+out.string());

        setStatus(std::to_string(applied)+
                  " bloco(s) transferido(s). DTB compilado e validado.");

        MessageBoxA(g_main,
            ("Novo DTB criado e validado:\r\n\r\n"+out.string()+
             "\r\n\r\nBase: Receptor\r\nBlocos aplicados do Doador: "+
             std::to_string(applied)).c_str(),
            "DTB criado com sucesso",MB_OK|MB_ICONINFORMATION);

    }catch(const std::exception& e){
        logLine(std::string("ERRO AO GERAR: ")+e.what());
        MessageBoxA(g_main,
            (std::string("Falha controlada ao gerar o DTB:\r\n\r\n")+e.what()+
             "\r\n\r\nO Doador e o Receptor originais nao foram alterados.").c_str(),
            "Erro ao gerar novo DTB",MB_OK|MB_ICONERROR);
        setStatus("Falha controlada. O programa continua aberto.");
    }catch(...){
        logLine("ERRO AO GERAR: excecao desconhecida.");
        MessageBoxA(g_main,
            "Ocorreu um erro inesperado ao gerar o novo DTB.\r\n\r\n"
            "Os arquivos originais nao foram alterados.",
            "DTB-Patcher",MB_OK|MB_ICONERROR);
        setStatus("Falha controlada. O programa continua aberto.");
    }
}

static void roundFill(Graphics& g,SolidBrush& br,Pen& pn,
                      int x,int y,int w,int h,float radius){
    GraphicsPath path;
    REAL X=(REAL)x,Y=(REAL)y,W=(REAL)w,H=(REAL)h,R=(REAL)radius;
    path.AddArc(X,Y,R,R,180.0f,90.0f);
    path.AddArc(X+W-R,Y,R,R,270.0f,90.0f);
    path.AddArc(X+W-R,Y+H-R,R,R,0.0f,90.0f);
    path.AddArc(X,Y+H-R,R,R,90.0f,90.0f);
    path.CloseFigure();
    g.FillPath(&br,&path);
    g.DrawPath(&pn,&path);
}

static void drawSarue(Graphics& g,float x,float y,float s){
    Pen outline(Color(255,70,54,45),3.0f*s);
    SolidBrush body(Color(255,217,208,189)),face(Color(255,238,231,216));
    SolidBrush pink(Color(255,215,143,145)),dark(Color(255,70,54,45));
    SolidBrush whiteB(Color(255,255,250,240));
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    PointF e1[]={
        PointF(x+28*s,y+36*s),PointF(x+17*s,y+15*s),
        PointF(x+42*s,y+17*s),PointF(x+54*s,y+30*s)
    };
    PointF e2[]={
        PointF(x+74*s,y+30*s),PointF(x+86*s,y+17*s),
        PointF(x+111*s,y+15*s),PointF(x+100*s,y+36*s)
    };
    g.FillPolygon(&body,e1,4);g.DrawPolygon(&outline,e1,4);
    g.FillPolygon(&body,e2,4);g.DrawPolygon(&outline,e2,4);
    g.FillEllipse(&face,x+25*s,y+17*s,78*s,76*s);
    g.DrawEllipse(&outline,x+25*s,y+17*s,78*s,76*s);
    g.FillEllipse(&dark,x+27*s,y+38*s,31*s,25*s);
    g.FillEllipse(&dark,x+70*s,y+38*s,31*s,25*s);
    g.FillEllipse(&whiteB,x+40*s,y+47*s,6*s,8*s);
    g.FillEllipse(&whiteB,x+82*s,y+47*s,6*s,8*s);
    g.FillEllipse(&whiteB,x+43*s,y+57*s,42*s,29*s);
    g.FillEllipse(&pink,x+57*s,y+58*s,14*s,12*s);
    g.FillEllipse(&pink,x+31*s,y+24*s,18*s,10*s);
    g.FillEllipse(&pink,x+79*s,y+24*s,18*s,10*s);
    g.DrawLine(&outline,x+64*s,y+69*s,x+64*s,y+76*s);
    g.DrawLine(&outline,x+64*s,y+76*s,x+54*s,y+79*s);
    g.DrawLine(&outline,x+64*s,y+76*s,x+74*s,y+79*s);
}

static void drawIcon(Graphics& g,int kind,float x,float y,float s){
    Pen p(Color(255,196,104,110),2.5f*s);
    SolidBrush b(Color(255,196,104,110));
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    if(kind==0){
        g.DrawArc(&p,x+5*s,y+12*s,34*s,22*s,200,140);
        g.DrawArc(&p,x+33*s,y+12*s,34*s,22*s,200,140);
        g.DrawLine(&p,x+18*s,y+23*s,x+18*s,y+31*s);
        g.DrawLine(&p,x+14*s,y+27*s,x+22*s,y+27*s);
        g.FillEllipse(&b,x+49*s,y+24*s,5*s,5*s);
        g.FillEllipse(&b,x+57*s,y+19*s,5*s,5*s);
    }else if(kind==1){
        g.FillRectangle(&b,x+8*s,y+17*s,8*s,12*s);
        PointF q[]={
            PointF(x+18*s,y+17*s),PointF(x+30*s,y+10*s),
            PointF(x+30*s,y+36*s),PointF(x+18*s,y+29*s)
        };
        g.FillPolygon(&b,q,4);
        g.DrawArc(&p,x+25*s,y+13*s,28*s,20*s,-65,130);
        g.DrawArc(&p,x+25*s,y+8*s,39*s,30*s,-65,130);
    }else if(kind==2){
        g.DrawRectangle(&p,x+5*s,y+8*s,58*s,32*s);
        g.DrawLine(&p,x+25*s,y+48*s,x+43*s,y+48*s);
        g.DrawLine(&p,x+34*s,y+40*s,x+34*s,y+48*s);
    }else if(kind==3){
        roundFill(g,b,p,(int)(x+10*s),(int)(y+8*s),(int)(46*s),(int)(32*s),4.0f);
        g.FillRectangle(&b,x+56*s,y+18*s,7*s,12*s);
        g.FillRectangle(&b,x+15*s,y+13*s,30*s,22*s);
    }else{
        g.DrawEllipse(&p,x+10*s,y+10*s,45*s,45*s);
        g.DrawLine(&p,x+18*s,y+47*s,x+49*s,y+16*s);
        g.DrawLine(&p,x+18*s,y+16*s,x+49*s,y+47*s);
    }
}

static HFONT makeFont(int size,bool bold=false){
    return CreateFontA(
        -size,0,0,0,bold?FW_BOLD:FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
}

static void setFont(HWND h,int size,bool bold=false){
    if(!h)return;
    SendMessage(h,WM_SETFONT,(WPARAM)makeFont(size,bold),TRUE);
}

static void drawCard(Graphics& g,int x,int y,int w,int h,int kind){
    SolidBrush br(Color(255,255,253,249));
    Pen pn(Color(255,224,216,205),1.2f);
    roundFill(g,br,pn,x,y,w,h,10.0f);

    drawIcon(g,kind,(float)x+10,(float)y+7,0.65f);

    FontFamily ff(L"Segoe UI");
    Font title(&ff,14,FontStyleBold,UnitPixel);
    Font small(&ff,10,FontStyleRegular,UnitPixel);
    SolidBrush txt(Color(255,55,48,42));
    SolidBrush mut(Color(255,111,100,91));

    std::wstring wt;
    for(char c:std::string(categoryTitle(kind)))
        wt.push_back((wchar_t)(unsigned char)c);

    std::wstring wd;
    for(char c:std::string(categoryDescription(kind)))
        wd.push_back((wchar_t)(unsigned char)c);

    int total=categoryItemCount(kind);
    int compat=categoryCompatibleCount(kind);

    g.DrawString(wt.c_str(),-1,&title,PointF((REAL)x+56,(REAL)y+8),&txt);
    g.DrawString(wd.c_str(),-1,&small,PointF((REAL)x+56,(REAL)y+29),&mut);

    std::wstring count=std::to_wstring(total)+L" bloco(s)  |  "+
                       std::to_wstring(compat)+L" compativel(is)";
    g.DrawString(count.c_str(),-1,&small,PointF((REAL)x+56,(REAL)y+43),&mut);

    // Divider and side labels make the Doador/Receptor split unmistakable.
    int split=x+w/2;
    Pen divider(Color(255,232,224,214),1.0f);
    g.DrawLine(&divider,(REAL)split,(REAL)y+58,(REAL)split,(REAL)(y+h-10));

    Font side(&ff,9,FontStyleBold,UnitPixel);
    SolidBrush donorText(Color(255,125,72,78));
    SolidBrush recvText(Color(255,72,92,106));
    g.DrawString(L"DOADOR — selecionar",-1,&side,
                 PointF((REAL)x+14,(REAL)y+57),&donorText);
    g.DrawString(L"RECEPTOR — base",-1,&side,
                 PointF((REAL)split+8,(REAL)y+57),&recvText);
}

static void resizeListColumns(HWND lv,int w){
    if(!lv)return;
    int usable=w-8;
    int c0=82;
    int c1=120;
    int c4=104;
    int remain=usable-c0-c1-c4;
    if(remain<160)remain=160;
    int half=remain/2;
    ListView_SetColumnWidth(lv,0,c0);
    ListView_SetColumnWidth(lv,1,c1);
    ListView_SetColumnWidth(lv,2,half);
    ListView_SetColumnWidth(lv,3,half);
    ListView_SetColumnWidth(lv,4,c4);
}

static void layout(){
    RECT r{};
    GetClientRect(g_main,&r);
    int W=r.right,H=r.bottom;
    const int margin=24;
    const int top=166;

    // Top source cards.
    const int actionW=260;
    const int sourceGap=14;
    int sourceW=(W-margin*2-actionW-2*sourceGap)/2;
    if(sourceW<300)sourceW=300;
    int sourceY=124;
    int sourceH=78;
    int receiverX=margin+sourceW+sourceGap;
    int actionX=receiverX+sourceW+sourceGap;

    MoveWindow(g_donor,margin+18,sourceY+26,sourceW-116,32,TRUE);
    MoveWindow(GetDlgItem(g_main,ID_DONOR),margin+sourceW-88,sourceY+26,70,32,TRUE);
    MoveWindow(g_donorInfo,margin+18,sourceY+4,sourceW-36,18,TRUE);

    MoveWindow(g_receiver,receiverX+18,sourceY+26,sourceW-116,32,TRUE);
    MoveWindow(GetDlgItem(g_main,ID_RECEIVER),receiverX+sourceW-88,sourceY+26,70,32,TRUE);
    MoveWindow(g_receiverInfo,receiverX+18,sourceY+4,sourceW-36,18,TRUE);

    MoveWindow(GetDlgItem(g_main,ID_SWAP),actionX,sourceY+2,actionW,30,TRUE);
    MoveWindow(GetDlgItem(g_main,ID_ANALYZE),actionX,sourceY+37,actionW,30,TRUE);
    MoveWindow(GetDlgItem(g_main,ID_BUILD),actionX,sourceY+72,actionW,38,TRUE);

    int cardTop=top;
    int logH=78;
    int cardBottom=H-logH-18;
    int cardArea=cardBottom-cardTop;
    if(cardArea<300)cardArea=300;

    int gap=12;
    int cardW=(W-2*margin-2*gap)/3;
    int rowH=(cardArea-gap)/2;

    int xs[5]={
        margin,margin+cardW+gap,margin+2*(cardW+gap),
        margin,margin+cardW+gap
    };
    int ys[5]={
        cardTop,cardTop,cardTop,
        cardTop+rowH+gap,cardTop+rowH+gap
    };

    for(int i=0;i<5;i++){
        int listY=ys[i]+72;
        int listH=rowH-82;
        MoveWindow(g_lists[i],xs[i]+8,listY,cardW-16,listH,TRUE);
        resizeListColumns(g_lists[i],cardW-16);
    }

    int logY=H-logH+4;
    MoveWindow(g_log,margin,logY,W-2*margin-220,logH-8,TRUE);
    MoveWindow(g_status,W-margin-205,logY,205,logH-8,TRUE);
    InvalidateRect(g_main,nullptr,FALSE);
}

static LRESULT CALLBACK wndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
        case WM_GETMINMAXINFO:{
            MINMAXINFO* mm=(MINMAXINFO*)l;
            mm->ptMinTrackSize.x=1180;
            mm->ptMinTrackSize.y=760;
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            if(g_main)layout();
            return 0;

        case WM_CTLCOLORSTATIC:{
            HDC dc=(HDC)w;
            HWND child=(HWND)l;
            SetBkMode(dc,TRANSPARENT);
            if(child==g_donorInfo||child==g_receiverInfo||child==g_status){
                SetTextColor(dc,muted());
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
            SetTextColor(dc,ink());
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }

        case WM_CTLCOLOREDIT:{
            HDC dc=(HDC)w;
            SetBkColor(dc,white());
            SetTextColor(dc,ink());
            return (LRESULT)g_whiteBrush;
        }

        case WM_PAINT:{
            PAINTSTRUCT ps;
            HDC dc=BeginPaint(h,&ps);
            RECT r{};
            GetClientRect(h,&r);

            HBRUSH br=CreateSolidBrush(bg());
            FillRect(dc,&r,br);
            DeleteObject(br);

            HBRUSH hd=CreateSolidBrush(header());
            RECT hr={0,0,r.right,110};
            FillRect(dc,&hr,hd);
            DeleteObject(hd);

            HBRUSH ac=CreateSolidBrush(accent());
            RECT ar={0,106,r.right,110};
            FillRect(dc,&ar,ac);
            DeleteObject(ac);

            Graphics g(dc);
            drawSarue(g,24,5,0.72f);

            FontFamily ff(L"Segoe UI");
            Font t(&ff,27,FontStyleBold,UnitPixel);
            Font s(&ff,12,FontStyleRegular,UnitPixel);
            SolidBrush wt(Color(255,255,250,245));
            SolidBrush ws(Color(255,226,214,207));

            g.DrawString(L"DTB-Patcher",-1,&t,PointF(118,20),&wt);
            g.DrawString(
                L"Compare blocos funcionais e transfira somente o que o Doador fornece",
                -1,&s,PointF(120,60),&ws);

            Font hint(&ff,10,FontStyleRegular,UnitPixel);
            SolidBrush hintB(Color(255,224,216,205));
            g.DrawString(
                L"Doador = origem da transferencia     Receptor = base do novo DTB",
                -1,&hint,PointF(120,82),&hintB);

            // Source card backgrounds.
            int W=r.right;
            const int sourceGap=14;
            const int actionW=260;
            int sourceW=(W-24*2-actionW-2*sourceGap)/2;
            if(sourceW<300)sourceW=300;
            int sy=124;
            SolidBrush sourceBr(Color(255,255,253,249));
            Pen sourcePn(Color(255,224,216,205),1.0f);
            roundFill(g,sourceBr,sourcePn,24,sy,sourceW,78,8.0f);
            roundFill(g,sourceBr,sourcePn,24+sourceW+sourceGap,sy,sourceW,78,8.0f);

            Font sourceTitle(&ff,11,FontStyleBold,UnitPixel);
            SolidBrush donorC(Color(255,125,72,78));
            SolidBrush recvC(Color(255,72,92,106));
            g.DrawString(L"DOADOR — origem",-1,&sourceTitle,
                         PointF(42,(REAL)sy+7),&donorC);
            g.DrawString(L"RECEPTOR — base",-1,&sourceTitle,
                         PointF((REAL)(42+sourceW+sourceGap),sy+7),&recvC);

            for(int i=0;i<5;i++){
                int cardGap=12;
                int cardW=(W-48-2*cardGap)/3;
                int cardArea=r.bottom-166-78-18;
                if(cardArea<300)cardArea=300;
                int rowH=(cardArea-cardGap)/2;
                int x=(i<3)?24+i*(cardW+cardGap):24+(i-3)*(cardW+cardGap);
                int y=(i<3)?166:166+rowH+cardGap;
                drawCard(g,x,y,cardW,rowH,i);
            }

            EndPaint(h,&ps);
            return 0;
        }

        case WM_NOTIFY:{
            NMHDR* n=(NMHDR*)l;

            for(int i=0;i<5;i++){
                if(n->hwndFrom!=g_lists[i])continue;

                if(n->code==LVN_ITEMCHANGING){
                    NMLISTVIEW* lv=(NMLISTVIEW*)l;
                    if(!(lv->uChanged&LVIF_STATE))break;

                    UINT oldState=(lv->uOldState&LVIS_STATEIMAGEMASK)>>12;
                    UINT newState=(lv->uNewState&LVIS_STATEIMAGEMASK)>>12;
                    if(newState==2&&oldState!=2){
                        LVITEMA item{};
                        item.mask=LVIF_PARAM;
                        item.iItem=lv->iItem;
                        if(ListView_GetItem(g_lists[i],&item)){
                            int idx=(int)item.lParam;
                            if(idx>=0&&idx<(int)g_items.size()&&!g_items[idx].compatible)
                                return TRUE;
                        }
                    }
                }

                if(n->code==NM_CUSTOMDRAW){
                    NMLVCUSTOMDRAW* cd=(NMLVCUSTOMDRAW*)l;
                    if(cd->nmcd.dwDrawStage==CDDS_PREPAINT)
                        return CDRF_NOTIFYITEMDRAW;

                    if(cd->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){
                        int row=(int)cd->nmcd.dwItemSpec;
                        cd->clrText=(row%2)?RGB(60,55,50):ink();
                        cd->clrTextBk=(row%2)?RGB(249,247,242):RGB(255,255,255);

                        LVITEMA item{};
                        item.mask=LVIF_PARAM;
                        item.iItem=row;
                        if(ListView_GetItem(g_lists[i],&item)){
                            int idx=(int)item.lParam;
                            if(idx>=0&&idx<(int)g_items.size()&&!g_items[idx].compatible){
                                cd->clrText=bad();
                                cd->clrTextBk=RGB(252,239,239);
                            }
                        }
                        return CDRF_DODEFAULT;
                    }
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
                    const char* msg=
                        "DTB-Patcher\r\n\r\n"
                        "Doador -> comparacao funcional -> selecao -> Receptor como base -> novo DTB.\r\n\r\n"
                        "Os arquivos originais nunca sao sobrescritos.\r\n"
                        "O DTC oficial e integrado no mesmo processo.\r\n"
                        "Somente propriedades existentes no Receptor sao substituidas.\r\n\r\n"
                        "Mascote: Sarue / Gatito-Ports.";
                    MessageBoxA(g_main,msg,"Sobre o DTB-Patcher",
                                 MB_OK|MB_ICONINFORMATION);
                    break;
                }
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(h,m,w,l);
}

static HWND makeList(HWND parent,HINSTANCE hi,int id){
    HWND lv=CreateWindowExA(
        WS_EX_CLIENTEDGE,WC_LISTVIEWA,"",
        WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,
        0,0,100,100,parent,(HMENU)(INT_PTR)id,hi,nullptr);

    ListView_SetExtendedListViewStyle(
        lv,LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER|
        LVS_EX_LABELTIP);

    const char* heads[]={
        "Selecionar","Funcao / bloco","Doador","Receptor","Resultado"
    };
    const int widths[]={82,120,160,160,104};

    for(int i=0;i<5;i++){
        LVCOLUMNA c{};
        c.mask=LVCF_TEXT|LVCF_WIDTH;
        c.pszText=(LPSTR)heads[i];
        c.cx=widths[i];
        ListView_InsertColumn(lv,i,&c);
    }

    setFont(lv,10);
    return lv;
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE,LPSTR,int){
    try{
        GdiplusStartupInput gi;
        if(GdiplusStartup(&g_gdiplus,&gi,nullptr)!=Ok)return 1;

        INITCOMMONCONTROLSEX ic{
            sizeof(ic),
            ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES
        };
        InitCommonControlsEx(&ic);

        g_whiteBrush=CreateSolidBrush(white());

        WNDCLASSA wc{};
        wc.hInstance=hi;
        wc.lpfnWndProc=wndProc;
        wc.lpszClassName="DTBPatcherNative";
        wc.hCursor=LoadCursorA(nullptr,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH);
        RegisterClassA(&wc);

        g_main=CreateWindowA(
            "DTBPatcherNative","DTB-Patcher",
            WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            0,0,1440,900,nullptr,nullptr,hi,nullptr);

        if(!g_main)return 1;

        g_donor=CreateWindowA(
            "EDIT","",
            WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            0,0,100,32,g_main,nullptr,hi,nullptr);
        g_receiver=CreateWindowA(
            "EDIT","",
            WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            0,0,100,32,g_main,nullptr,hi,nullptr);

        setFont(g_donor,10);
        setFont(g_receiver,10);

        HWND db=CreateWindowA(
            "BUTTON","Selecionar",
            WS_CHILD|WS_VISIBLE|BS_FLAT,
            0,0,70,32,g_main,(HMENU)ID_DONOR,hi,nullptr);
        HWND rb=CreateWindowA(
            "BUTTON","Selecionar",
            WS_CHILD|WS_VISIBLE|BS_FLAT,
            0,0,70,32,g_main,(HMENU)ID_RECEIVER,hi,nullptr);

        setFont(db,9,true);
        setFont(rb,9,true);

        g_donorInfo=CreateWindowA(
            "STATIC","Aguardando analise",
            WS_CHILD|WS_VISIBLE,0,0,100,18,g_main,
            (HMENU)ID_DONOR_INFO,hi,nullptr);
        g_receiverInfo=CreateWindowA(
            "STATIC","Aguardando analise",
            WS_CHILD|WS_VISIBLE,0,0,100,18,g_main,
            (HMENU)ID_RECEIVER_INFO,hi,nullptr);

        setFont(g_donorInfo,9);
        setFont(g_receiverInfo,9);

        HWND sw=CreateWindowA(
            "BUTTON","Trocar Doador / Receptor",
            WS_CHILD|WS_VISIBLE|BS_FLAT,
            0,0,145,30,g_main,(HMENU)ID_SWAP,hi,nullptr);
        HWND an=CreateWindowA(
            "BUTTON","Analisar DTBs",
            WS_CHILD|WS_VISIBLE|BS_FLAT,
            0,0,145,30,g_main,(HMENU)ID_ANALYZE,hi,nullptr);
        HWND bu=CreateWindowA(
            "BUTTON","Gerar novo DTB",
            WS_CHILD|WS_VISIBLE|BS_FLAT,
            0,0,180,36,g_main,(HMENU)ID_BUILD,hi,nullptr);
        HWND ab=CreateWindowA(
            "BUTTON","Sobre",
            WS_CHILD|WS_VISIBLE|BS_FLAT,
            0,0,80,30,g_main,(HMENU)ID_ABOUT,hi,nullptr);

        setFont(sw,9,true);
        setFont(an,9,true);
        setFont(bu,10,true);
        setFont(ab,9,true);

        for(int i=0;i<5;i++)
            g_lists[i]=makeList(g_main,hi,ID_LIST_CONTROLS+i);

        g_log=CreateWindowA(
            "EDIT","Log de operacao:\r\n",
            WS_CHILD|WS_VISIBLE|WS_BORDER|
            ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,
            0,0,100,60,g_main,nullptr,hi,nullptr);
        setFont(g_log,9);

        g_status=CreateWindowA(
            "STATIC",
            "Pronto. Selecione Doador e Receptor para iniciar.",
            WS_CHILD|WS_VISIBLE|SS_CENTER,
            0,0,100,60,g_main,nullptr,hi,nullptr);
        setFont(g_status,9,true);

        ShowWindow(g_main,SW_SHOWMAXIMIZED);
        UpdateWindow(g_main);
        layout();

        MSG msg{};
        while(GetMessageA(&msg,nullptr,0,0)>0){
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if(g_whiteBrush)DeleteObject(g_whiteBrush);
        GdiplusShutdown(g_gdiplus);
        return 0;
    }catch(...){
        // Initialization failures should not leave the process in a half-built
        // state. Runtime generation errors are handled inside build().
        return 1;
    }
}
