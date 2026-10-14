#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum klog_lang {
    KLOG_LANG_EN = 0,
    KLOG_LANG_ZH_HANS = 1,
} klog_lang_t;

/**
 * Initialize / refresh the current system language.
 * Returns true if language changed since last check.
 */
bool klog_i18n_refresh(void);

/**
 * Get current active language.
 */
klog_lang_t klog_i18n_get_lang(void);

/**
 * Translate a key to current language string.
 */
const char *klog_tr(const char *key);

#ifdef __cplusplus
}
#endif
