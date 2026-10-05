/*
 * Native C ABI used by the C++ application to call the upstream DTC
 * parser/compiler without starting an external dtc.exe process.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void dtbp_dtc_exit(int status);
int dtbp_dtc_decompile(const char* dtb_path, const char* dts_path);
int dtbp_dtc_compile(const char* dts_path, const char* dtb_path);
const char* dtbp_dtc_version(void);
const char* dtbp_dtc_error(void);
#ifdef __cplusplus
}
#endif
