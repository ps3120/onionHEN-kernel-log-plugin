#pragma once

#include <onion/services.h>
#include <onion/status.h>
#include <onion/ui.h>

#include <stdint.h>

#include "plugin_settings.h"

typedef enum plugin_ui_action_kind {
    PLUGIN_UI_ACTION_NONE = 0,
    PLUGIN_UI_ACTION_SET_ENABLED,
    PLUGIN_UI_ACTION_SET_PORT,
    PLUGIN_UI_ACTION_RESTART
} plugin_ui_action_kind;

typedef struct plugin_ui_action {
    plugin_ui_action_kind kind;
    int enabled;
    uint16_t port;
} plugin_ui_action;

onion_status plugin_ui_create(const plugin_settings *settings,
                              onion_ui_document **out_document);
onion_status plugin_ui_decode_action(onion_ui_handle handle,
                                     const onion_ui_event_v1 *event,
                                     plugin_ui_action *out_action);
onion_status plugin_ui_set_enabled(const onion_host_services_v1 *services,
                                   onion_ui_handle handle, int enabled);
onion_status plugin_ui_set_port(const onion_host_services_v1 *services,
                                onion_ui_handle handle, uint16_t port);
