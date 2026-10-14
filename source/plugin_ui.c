#include "plugin_ui.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "klog_i18n.h"
#include "plugin_config.h"

#define CONTRIBUTION_ID "settings"

static onion_ui_node_desc_v1 make_node(uint32_t kind, const char *id,
                                       const char *parent_id,
                                       const char *title) {
    onion_ui_node_desc_v1 node;
    memset(&node, 0, sizeof(node));
    node.struct_size = sizeof(node);
    node.abi_version = ONION_UI_ABI_VERSION;
    node.kind = kind;
    snprintf(node.id, sizeof(node.id), "%s", id);
    snprintf(node.parent_id, sizeof(node.parent_id), "%s",
             parent_id ? parent_id : "");
    snprintf(node.title, sizeof(node.title), "%s", title);
    return node;
}

static onion_status add_nodes(onion_ui_document *document,
                              const plugin_settings *settings) {
    /* Root page: directly hosts all kernel log settings */
    onion_ui_node_desc_v1 node =
        make_node(ONION_UI_NODE_PAGE, "main", NULL, klog_tr("title"));
    onion_status status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_LABEL, "about", "main", klog_tr("about_title"));
    snprintf(node.description, sizeof(node.description), "%s",
             klog_tr("about_desc"));
    status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_TOGGLE, "enabled", "main",
                     klog_tr("enabled_title"));
    node.value_type = ONION_UI_VALUE_BOOL;
    node.binding = ONION_UI_BINDING_EVENT;
    snprintf(node.binding_key, sizeof(node.binding_key), "enabled_changed");
    snprintf(node.value, sizeof(node.value), "%s",
             settings->enabled ? "true" : "false");
    snprintf(node.description, sizeof(node.description), "%s",
             klog_tr("enabled_desc"));
    status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_INPUT, "port", "main", klog_tr("port_title"));
    node.value_type = ONION_UI_VALUE_INT;
    node.binding = ONION_UI_BINDING_EVENT;
    node.min_value = 1;
    node.max_value = 65535;
    node.min_length = 1;
    node.max_length = 5;
    snprintf(node.binding_key, sizeof(node.binding_key), "port_changed");
    snprintf(node.value, sizeof(node.value), "%u", (unsigned)settings->port);
    snprintf(node.description, sizeof(node.description), "%s",
             klog_tr("port_desc"));
    status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_ACTION, "restart", "main",
                     klog_tr("restart_title"));
    node.flags = ONION_UI_NODE_FLAG_CONFIRM;
    node.binding = ONION_UI_BINDING_EVENT;
    snprintf(node.binding_key, sizeof(node.binding_key), "restart_requested");
    snprintf(node.description, sizeof(node.description), "%s",
             klog_tr("restart_desc"));
    return onion_ui_document_add_node(document, &node);
}

onion_status plugin_ui_create(const plugin_settings *settings,
                              onion_ui_document **out_document) {
    if (!settings || !out_document) return ONION_E_INVALID_ARGUMENT;

    onion_ui_document_desc_v1 description;
    memset(&description, 0, sizeof(description));
    description.struct_size = sizeof(description);
    description.abi_version = ONION_UI_ABI_VERSION;
    description.priority = 100;
    snprintf(description.plugin_id, sizeof(description.plugin_id), "%s",
             PLUGIN_ID);
    snprintf(description.contribution_id,
             sizeof(description.contribution_id), "%s", CONTRIBUTION_ID);
    snprintf(description.title, sizeof(description.title), "%s",
             klog_tr("title"));
    snprintf(description.description, sizeof(description.description), "%s",
             klog_tr("about_desc"));
    snprintf(description.root_page_id, sizeof(description.root_page_id), "main");

    onion_status status = onion_ui_document_create(&description, out_document);
    if (status != ONION_OK) return status;
    status = add_nodes(*out_document, settings);
    if (status == ONION_OK) status = onion_ui_document_validate(*out_document);
    if (status != ONION_OK) {
        onion_ui_document_destroy(*out_document);
        *out_document = NULL;
    }
    return status;
}

static onion_status parse_port(const char *value, uint16_t *out_port) {
    char *end = NULL;
    errno = 0;
    const long port = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || port < 1 ||
        port > 65535) {
        return ONION_E_INVALID_ARGUMENT;
    }
    *out_port = (uint16_t)port;
    return ONION_OK;
}

onion_status plugin_ui_decode_action(onion_ui_handle handle,
                                     const onion_ui_event_v1 *event,
                                     plugin_ui_action *out_action) {
    if (handle == 0 || !event || !out_action) return ONION_E_INVALID_ARGUMENT;
    memset(out_action, 0, sizeof(*out_action));
    if (event->handle != handle ||
        strcmp(event->contribution_id, CONTRIBUTION_ID) != 0) {
        return ONION_E_NOT_FOUND;
    }

    if (strcmp(event->node_id, "enabled") == 0) {
        if (event->value_type != ONION_UI_VALUE_BOOL) {
            return ONION_E_INVALID_ARGUMENT;
        }
        out_action->kind = PLUGIN_UI_ACTION_SET_ENABLED;
        if (strcmp(event->value, "true") == 0 ||
            strcmp(event->value, "1") == 0) {
            out_action->enabled = 1;
            return ONION_OK;
        }
        if (strcmp(event->value, "false") == 0 ||
            strcmp(event->value, "0") == 0) {
            return ONION_OK;
        }
        return ONION_E_INVALID_ARGUMENT;
    }

    if (strcmp(event->node_id, "port") == 0) {
        if (event->value_type != ONION_UI_VALUE_INT) {
            return ONION_E_INVALID_ARGUMENT;
        }
        out_action->kind = PLUGIN_UI_ACTION_SET_PORT;
        return parse_port(event->value, &out_action->port);
    }

    if (strcmp(event->node_id, "restart") == 0) {
        if (event->value_type != ONION_UI_VALUE_NONE) {
            return ONION_E_INVALID_ARGUMENT;
        }
        out_action->kind = PLUGIN_UI_ACTION_RESTART;
        return ONION_OK;
    }
    return ONION_E_NOT_FOUND;
}

onion_status plugin_ui_set_enabled(const onion_host_services_v1 *services,
                                   onion_ui_handle handle, int enabled) {
    if (!services || handle == 0) return ONION_E_INVALID_ARGUMENT;
    return onion_ui_set_value(services, handle, "enabled", ONION_UI_VALUE_BOOL,
                              enabled ? "true" : "false");
}

onion_status plugin_ui_set_port(const onion_host_services_v1 *services,
                                onion_ui_handle handle, uint16_t port) {
    if (!services || handle == 0) return ONION_E_INVALID_ARGUMENT;
    char value[8];
    snprintf(value, sizeof(value), "%u", (unsigned)port);
    return onion_ui_set_value(
        services, handle, "port", ONION_UI_VALUE_INT, value);
}
