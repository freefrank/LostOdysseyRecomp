#pragma once
#define AV_LOG_WARNING 24
inline void av_log_default_callback(void*,int,const char*,va_list) {}
inline void av_log_set_callback(void (*)(void*,int,const char*,va_list)) {}
