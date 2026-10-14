#pragma once

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(__PROSPERO__)
#include <ps5/klog.h>
#else
#define klog_puts(s)
#define klog_printf(s, ...)
#endif

/**
 * Log to stdout and the kernel log
 **/
#define KLOG_LOG_PUTS(s) {      \
    puts(s);                    \
    klog_puts(s);               \
  }

#define KLOG_LOG_PRINTF(s, ...) {   \
    printf(s, __VA_ARGS__);         \
    klog_printf(s, __VA_ARGS__);    \
  }

#define KLOG_LOG_PERROR(s) {                            \
    printf("%s:%d:%s: %s\n", __FILE__, __LINE__, s,     \
           strerror(errno));                            \
    klog_printf("%s:%d:%s: %s\n", __FILE__, __LINE__, s, \
           strerror(errno));                            \
  }
