#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "picam360_capture_plugin.h"

void create_omx_encoder_plugin(PLUGIN_HOST_T *plugin_host, PLUGIN_T **plugin);

#ifdef __cplusplus
}
#endif
