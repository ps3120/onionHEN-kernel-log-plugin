#include "klog_i18n.h"

#include <stdatomic.h>
#include <string.h>

extern int sceSystemServiceParamGetInt(int param_id, int *value);

typedef struct klog_tr_entry {
    const char *key;
    const char *en;
    const char *zh_hans;
} klog_tr_entry;

static const klog_tr_entry kTranslations[] = {
    {"title", "Kernel Log", "内核日志"},
    {"about_title", "About", "关于此插件"},
    {"about_desc",
     "Managed PS5 kernel log server. Streams /dev/klog over TCP for debugging.",
     "由 OnionHEN 管理的 PS5 内核日志服务。通过 TCP 输出 /dev/klog 以便调试。"},
    {"enabled_title", "Enabled", "启用内核日志服务"},
    {"enabled_desc",
     "Start the /dev/klog server on the configured TCP port.",
     "开启后在配置的 TCP 端口上启动 /dev/klog 服务器。"},
    {"port_title", "TCP Port", "TCP 端口"},
    {"port_desc",
     "Server port (1-65535, default 3232). Changing the port restarts the server.",
     "服务器端口 (1-65535，默认 3232)。修改端口会重启服务。"},
    {"restart_title", "Restart Server", "重启内核日志服务"},
    {"restart_desc",
     "Stop and start the /dev/klog server on the current port.",
     "在当前端口停止并重新启动 /dev/klog 服务器。"},
};

#define TRANSLATION_COUNT (sizeof(kTranslations) / sizeof(kTranslations[0]))

static atomic_int g_current_lang = ATOMIC_VAR_INIT(KLOG_LANG_EN);
static atomic_int g_initialized = ATOMIC_VAR_INIT(0);

bool klog_i18n_refresh(void) {
    int sys_lang = 1;
    if (sceSystemServiceParamGetInt(1, &sys_lang) < 0) {
        if (!atomic_load_explicit(&g_initialized, memory_order_relaxed)) {
            atomic_store_explicit(&g_current_lang, KLOG_LANG_EN, memory_order_relaxed);
            atomic_store_explicit(&g_initialized, 1, memory_order_relaxed);
            return true;
        }
        return false;
    }

    klog_lang_t resolved = KLOG_LANG_EN;
    /* 11 = Simplified Chinese, 10 = Traditional Chinese */
    if (sys_lang == 11 || sys_lang == 10) {
        resolved = KLOG_LANG_ZH_HANS;
    } else {
        resolved = KLOG_LANG_EN;
    }

    klog_lang_t old_lang =
        (klog_lang_t)atomic_load_explicit(&g_current_lang, memory_order_relaxed);
    int was_inited = atomic_load_explicit(&g_initialized, memory_order_relaxed);

    atomic_store_explicit(&g_current_lang, resolved, memory_order_relaxed);
    atomic_store_explicit(&g_initialized, 1, memory_order_relaxed);

    return (!was_inited) || (old_lang != resolved);
}

klog_lang_t klog_i18n_get_lang(void) {
    if (!atomic_load_explicit(&g_initialized, memory_order_relaxed)) {
        klog_i18n_refresh();
    }
    return (klog_lang_t)atomic_load_explicit(&g_current_lang, memory_order_relaxed);
}

const char *klog_tr(const char *key) {
    if (!key) return "";
    klog_lang_t lang = klog_i18n_get_lang();
    for (size_t i = 0; i < TRANSLATION_COUNT; ++i) {
        if (strcmp(kTranslations[i].key, key) == 0) {
            return (lang == KLOG_LANG_ZH_HANS) ? kTranslations[i].zh_hans
                                               : kTranslations[i].en;
        }
    }
    return key;
}
