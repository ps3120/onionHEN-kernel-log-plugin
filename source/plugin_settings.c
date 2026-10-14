#include "plugin_settings.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plugin_config.h"

void plugin_settings_defaults(plugin_settings *settings) {
    if (!settings) return;
    settings->enabled = 1;
    settings->port = PLUGIN_DEFAULT_PORT;
}

static void trim_line_end(char *value) {
    size_t end = strlen(value);
    while (end > 0 && (value[end - 1] == '\n' || value[end - 1] == '\r' ||
                       value[end - 1] == ' ' || value[end - 1] == '\t')) {
        value[--end] = '\0';
    }
}

int plugin_settings_load(const char *path, plugin_settings *settings) {
    if (!path || !settings) return -1;
    plugin_settings_defaults(settings);

    FILE *file = fopen(path, "r");
    if (!file) return errno == ENOENT ? 0 : -1;

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        trim_line_end(line);
        if (strncmp(line, "enabled=", 8) == 0) {
            const char *value = line + 8;
            if (strcmp(value, "true") == 0) settings->enabled = 1;
            if (strcmp(value, "false") == 0) settings->enabled = 0;
            continue;
        }
        if (strncmp(line, "port=", 5) == 0) {
            char *end = NULL;
            errno = 0;
            const long port = strtol(line + 5, &end, 10);
            if (errno == 0 && end != line + 5 && *end == '\0' && port >= 1 &&
                port <= 65535) {
                settings->port = (uint16_t)port;
            }
        }
    }
    return fclose(file) == 0 ? 0 : -1;
}

int plugin_settings_save(const char *path, const plugin_settings *settings) {
    if (!path || !settings || settings->port == 0) return -1;

    char temporary_path[512];
    const int path_length = snprintf(
        temporary_path, sizeof(temporary_path), "%s.tmp", path);
    if (path_length < 0 || (size_t)path_length >= sizeof(temporary_path)) {
        return -1;
    }

    FILE *file = fopen(temporary_path, "w");
    if (!file) return -1;

    int result = fprintf(file, "enabled=%s\nport=%u\n",
                         settings->enabled ? "true" : "false",
                         (unsigned)settings->port) < 0
                     ? -1
                     : 0;
    if (result == 0 && fflush(file) != 0) result = -1;
    if (result == 0 && fsync(fileno(file)) != 0) result = -1;
    if (fclose(file) != 0) result = -1;
    if (result == 0 && rename(temporary_path, path) != 0) result = -1;
    if (result != 0) unlink(temporary_path);
    return result;
}
