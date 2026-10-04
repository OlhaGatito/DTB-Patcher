#include "dtc_bridge.h"
#include "dtc.h"
#include <stdio.h>
#include <string.h>

static char last_error[2048];

static void reset_state(void) {
    quiet=0; reservenum=0; minsize=0; padsize=0; alignsize=0;
    phandle_format=PHANDLE_EPAPR; generate_symbols=0; generate_fixups=0;
    auto_label_aliases=0; annotate=0; last_error[0]=0;
}
static void set_error(const char* s) {
    snprintf(last_error,sizeof(last_error),"%s",s?s:"DTC operation failed");
}
const char* dtbp_dtc_error(void) { return last_error; }
const char* dtbp_dtc_version(void) { return DTC_VERSION; }

int dtbp_dtc_decompile(const char* dtb_path,const char* dts_path) {
    reset_state();
    struct dt_info* dti=dt_from_blob(dtb_path);
    if(!dti){set_error("DTC could not parse the DTB.");return 1;}
    FILE* out=fopen(dts_path,"wb");
    if(!out){set_error("Could not create DTS output.");return 2;}
    dt_to_source(out,dti); fclose(out); return 0;
}
int dtbp_dtc_compile(const char* dts_path,const char* dtb_path) {
    reset_state();
    struct dt_info* dti=dt_from_source(dts_path);
    if(!dti){set_error("DTC could not parse the DTS.");return 1;}
    process_checks(false,dti);
    FILE* out=fopen(dtb_path,"wb");
    if(!out){set_error("Could not create DTB output.");return 2;}
    dt_to_blob(out,dti,DEFAULT_FDT_VERSION); fclose(out); return 0;
}
