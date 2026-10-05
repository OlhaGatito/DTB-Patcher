/*
 * Gatito Dtb Pacher native DTC bridge.
 *
 * The implementation deliberately uses the upstream DTC front-end APIs
 * instead of reimplementing DTS parsing or DTB serialization here.
 * This file is compiled together with the official DTC sources.
 */
#include "dtc_bridge.h"
#include "dtc.h"
#include "version_gen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>

static char last_error[2048];
static jmp_buf dtbp_exit_env;
static volatile int dtbp_exit_active=0;
static FILE* dtbp_active_out=NULL;
static char dtbp_active_tmp[4096];

/* Exported from the embedded/patched dtc.c during the native build. */
extern void dtbp_fill_fullpaths(struct node* tree, const char* prefix);

/* DTC fatal helpers call exit(). In the standalone CLI that is fine; inside
 * Gatito Dtb Pacher it would terminate the GUI. The native build compiles
 * DTC with -Dexit=dtbp_dtc_exit so fatal DTC paths return here. */
void dtbp_dtc_exit(int status){
    if(dtbp_exit_active){
        if(dtbp_active_out){fclose(dtbp_active_out);dtbp_active_out=NULL;}
        if(dtbp_active_tmp[0]){remove(dtbp_active_tmp);dtbp_active_tmp[0]=0;}
        snprintf(last_error,sizeof(last_error),
                 "DTC abortou a operacao (exit status %d). Consulte o log para a etapa exata.",status);
        longjmp(dtbp_exit_env,1);
    }
    _Exit(status);
}

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
    dtbp_exit_active=1;
    if(setjmp(dtbp_exit_env)!=0){
        dtbp_exit_active=0;
        return 99;
    }

    struct dt_info* dti=dt_from_blob(dtb_path);
    if(!dti){set_error("DTC could not parse the DTB.");dtbp_exit_active=0;return 1;}

    char tmp[4096];
    snprintf(tmp,sizeof(tmp),"%s.gatito-tmp",dts_path);
    remove(tmp);
    snprintf(dtbp_active_tmp,sizeof(dtbp_active_tmp),"%s",tmp);
    FILE* out=fopen(tmp,"wb");
    if(!out){dtbp_active_tmp[0]=0;set_error("Could not create DTS output.");dtbp_exit_active=0;return 2;}
    dtbp_active_out=out;
    dt_to_source(out,dti);
    fclose(out);dtbp_active_out=NULL;dtbp_active_tmp[0]=0;

    if(rename(tmp,dts_path)!=0){
        remove(tmp);
        set_error("Could not finalize DTS output.");
        dtbp_exit_active=0;
        return 2;
    }

    dtbp_exit_active=0;
    return 0;
}

int dtbp_dtc_compile(const char* dts_path,const char* dtb_path) {
    reset_state();
    dtbp_exit_active=1;
    if(setjmp(dtbp_exit_env)!=0){
        dtbp_exit_active=0;
        return 99;
    }

    struct dt_info* dti=dt_from_source(dts_path);
    if(!dti){set_error("DTC could not parse the DTS.");dtbp_exit_active=0;return 1;}

    /* Match the upstream DTC CLI sequence: checks rely on basenamelen/fullpath. */
    dtbp_fill_fullpaths(dti->dt, "");
    process_checks(false,dti);

    char tmp[4096];
    snprintf(tmp,sizeof(tmp),"%s.gatito-tmp",dtb_path);
    remove(tmp);
    snprintf(dtbp_active_tmp,sizeof(dtbp_active_tmp),"%s",tmp);
    FILE* out=fopen(tmp,"wb");
    if(!out){dtbp_active_tmp[0]=0;set_error("Could not create DTB output.");dtbp_exit_active=0;return 2;}
    dtbp_active_out=out;

    dt_to_blob(out,dti,DEFAULT_FDT_VERSION);
    fclose(out);dtbp_active_out=NULL;dtbp_active_tmp[0]=0;

    if(rename(tmp,dtb_path)!=0){
        remove(tmp);
        set_error("Could not finalize DTB output.");
        dtbp_exit_active=0;
        return 2;
    }

    dtbp_exit_active=0;
    return 0;
}
