#include <onion/client.h>
#include <onion/plugin.h>
#include <onion/status.h>
#include <onion/transport.h>
#include <onion/ui.h>

#include <ps5/kernel.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "klog_i18n.h"
#include "klog_service.h"
#include "plugin_config.h"
#include "plugin_settings.h"
#include "plugin_ui.h"

#define CONNECT_ATTEMPTS 30
#define CONNECT_RETRY_US (250 * 1000)
#define EVENT_POLL_US (100 * 1000)
#define PLUGIN_AUTH_ID UINT64_C(0x4801000000000013)

extern const onion_plugin_descriptor_v1 onion_plugin_descriptor;

typedef struct plugin_app {
    onion_transport transport;
    onion_socket_transport socket;
    onion_client client;
    onion_host_services_v1 services;
    onion_ui_document *document;
    onion_ui_handle ui_handle;
    klog_service klog;
    plugin_settings settings;
    FILE *log_file;
    int transport_connected;
    int client_initialized;
    int klog_initialized;
} plugin_app;

static volatile sig_atomic_t running = 1;

static void request_stop(int signal_number) {
    (void)signal_number;
    running = 0;
}

static void log_message(plugin_app *app, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
    fflush(stdout);

    if (!app->log_file) return;
    va_start(arguments, format);
    vfprintf(app->log_file, format, arguments);
    va_end(arguments);
    fflush(app->log_file);
}

static onion_status connect_to_daemon(plugin_app *app) {
    for (int attempt = 1; attempt <= CONNECT_ATTEMPTS && running; ++attempt) {
        onion_status status = onion_socket_transport_connect(
            &app->transport, &app->socket, ONION_PLUGIN_IPC_SOCKET_PATH);
        if (status == ONION_OK) {
            app->transport_connected = 1;
            return ONION_OK;
        }
        if (attempt < CONNECT_ATTEMPTS) usleep(CONNECT_RETRY_US);
    }
    return ONION_E_IO;
}

static void save_settings(plugin_app *app) {
    if (plugin_settings_save(PLUGIN_SETTINGS_PATH, &app->settings) != 0) {
        log_message(app, "[%s] failed to save %s\n", PLUGIN_ID,
                    PLUGIN_SETTINGS_PATH);
    }
}

static onion_status start_plugin(plugin_app *app) {
    if (kernel_set_ucred_authid(getpid(), PLUGIN_AUTH_ID) != 0) {
        return ONION_E_PERMISSION;
    }

    onion_status status = connect_to_daemon(app);
    if (status != ONION_OK) return status;
    status = onion_client_init(&app->client, &app->transport);
    if (status != ONION_OK) return status;
    app->client_initialized = 1;

    status = onion_client_open_session(&app->client, &onion_plugin_descriptor);
    if (status == ONION_OK) {
        status = onion_client_make_services(&app->client, &app->services);
    }
    if (status != ONION_OK) return status;

    if (!klog_service_init(&app->klog, app->settings.port)) {
        return ONION_E_IO;
    }
    app->klog_initialized = 1;
    if (app->settings.enabled && !klog_service_start(&app->klog)) {
        app->settings.enabled = 0;
        save_settings(app);
        log_message(app, "[%s] failed to start klogsrv on TCP %u\n", PLUGIN_ID,
                    (unsigned)app->settings.port);
    }

    status = plugin_ui_create(&app->settings, &app->document);
    if (status == ONION_OK) {
        status = onion_ui_register(
            &app->services, app->document, &app->ui_handle);
    }
    return status;
}

static void stop_plugin(plugin_app *app) {
    if (app->ui_handle != 0) {
        (void)onion_ui_unregister(&app->services, app->ui_handle);
        app->ui_handle = 0;
    }
    onion_ui_document_destroy(app->document);
    app->document = NULL;
    if (app->klog_initialized) {
        klog_service_destroy(&app->klog);
        app->klog_initialized = 0;
    }
    if (app->client_initialized) {
        onion_client_deinit(&app->client);
        app->client_initialized = 0;
    }
    if (app->transport_connected) {
        onion_socket_transport_deinit(&app->transport);
        app->transport_connected = 0;
    }
}

static onion_status apply_action(plugin_app *app,
                                 const plugin_ui_action *action) {
    switch (action->kind) {
    case PLUGIN_UI_ACTION_SET_ENABLED:
        if (action->enabled == app->settings.enabled) return ONION_OK;
        if (action->enabled) {
            if (!klog_service_start(&app->klog)) {
                (void)plugin_ui_set_enabled(
                    &app->services, app->ui_handle, 0);
                return ONION_E_IO;
            }
        } else {
            klog_service_stop(&app->klog);
        }
        app->settings.enabled = action->enabled;
        save_settings(app);
        return ONION_OK;

    case PLUGIN_UI_ACTION_SET_PORT: {
        const uint16_t previous_port = app->settings.port;
        if (action->port == previous_port) return ONION_OK;
        if (!klog_service_reconfigure(&app->klog, action->port)) {
            (void)plugin_ui_set_port(
                &app->services, app->ui_handle, previous_port);
            return ONION_E_IO;
        }
        app->settings.port = action->port;
        save_settings(app);
        return ONION_OK;
    }

    case PLUGIN_UI_ACTION_RESTART:
        if (!klog_service_restart(&app->klog)) {
            (void)plugin_ui_set_enabled(
                &app->services, app->ui_handle, 0);
            app->settings.enabled = 0;
            save_settings(app);
            return ONION_E_IO;
        }
        app->settings.enabled = 1;
        save_settings(app);
        return plugin_ui_set_enabled(&app->services, app->ui_handle, 1);

    case PLUGIN_UI_ACTION_NONE:
        return ONION_E_NOT_FOUND;
    }
    return ONION_E_NOT_FOUND;
}

static int run_event_loop(plugin_app *app) {
    unsigned int language_refresh_ticks = 0;
    while (running) {
        if (++language_refresh_ticks >= 20) { /* every ~2 seconds (20 * 100ms) */
            language_refresh_ticks = 0;
            if (klog_i18n_refresh()) {
                log_message(app, "[%s] system language changed, updating UI\n",
                            PLUGIN_ID);
                if (app->ui_handle != 0) {
                    (void)onion_ui_unregister(&app->services, app->ui_handle);
                    app->ui_handle = 0;
                }
                if (app->document) {
                    onion_ui_document_destroy(app->document);
                    app->document = NULL;
                }
                if (plugin_ui_create(&app->settings, &app->document) ==
                    ONION_OK) {
                    (void)onion_ui_register(
                        &app->services, app->document, &app->ui_handle);
                }
            }
        }

        onion_ui_event_v1 event;
        onion_status status = onion_client_poll_ui_event(&app->client, &event);
        if (status == ONION_E_NOT_FOUND) {
            usleep(EVENT_POLL_US);
            continue;
        }
        if (status != ONION_OK) {
            log_message(app, "[%s] UI event poll failed: %s\n", PLUGIN_ID,
                        onion_status_string(status));
            return 1;
        }

        plugin_ui_action action;
        status = plugin_ui_decode_action(app->ui_handle, &event, &action);
        if (status == ONION_OK) status = apply_action(app, &action);
        if (status != ONION_OK && status != ONION_E_NOT_FOUND) {
            log_message(app, "[%s] action %s failed: %s\n", PLUGIN_ID,
                        event.node_id, onion_status_string(status));
        }
    }
    return 0;
}

int main(void) {
    plugin_app app = {0};
    plugin_settings_defaults(&app.settings);
    if (plugin_settings_load(PLUGIN_SETTINGS_PATH, &app.settings) != 0) {
        fprintf(stderr, "[%s] failed to read %s; using defaults\n", PLUGIN_ID,
                PLUGIN_SETTINGS_PATH);
    }
    app.log_file = fopen(PLUGIN_LOG_PATH, "a");

    signal(SIGINT, request_stop);
    signal(SIGTERM, request_stop);
    signal(SIGPIPE, SIG_IGN);
    (void)syscall(SYS_thr_set_name, -1, "KLOG00001.elf");
    log_message(&app, "[%s] starting %s %s\n", PLUGIN_ID, PLUGIN_NAME,
                PLUGIN_VERSION);

    const onion_status status = start_plugin(&app);
    int exit_code = 1;
    if (status == ONION_OK) {
        log_message(&app, "[%s] ready; enabled=%d port=%u handle=%llu\n",
                    PLUGIN_ID, app.settings.enabled,
                    (unsigned)app.settings.port,
                    (unsigned long long)app.ui_handle);
        exit_code = run_event_loop(&app);
    } else {
        log_message(&app, "[%s] startup failed: %s\n", PLUGIN_ID,
                    onion_status_string(status));
    }

    stop_plugin(&app);
    log_message(&app, "[%s] stopped with exit_code=%d\n", PLUGIN_ID, exit_code);
    if (app.log_file) fclose(app.log_file);
    return exit_code;
}
