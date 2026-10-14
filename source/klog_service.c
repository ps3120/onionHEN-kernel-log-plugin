/* Kernel Log plugin: synchronized klogsrv child lifecycle.
 *
 * The vendored klogsrv payload (Serving /dev/klog over TCP) ships embedded in
 * the plugin ELF. Starting the service writes the blob to disk, launches it
 * through the OnionHEN private loader, and records the exact child PID so a
 * crashed plugin can clean up the orphaned server on its next start.
 */

#include "klog_service.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "elfldr_client.h"
#include "klog_log.h"
#include "klog_payload.h"
#include "plugin_config.h"

#define KILL_WAIT_ATTEMPTS 10
#define KILL_WAIT_US (50 * 1000)

static void mkdir_parent(const char *abs_path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", abs_path);
    char *slash = strrchr(tmp, '/');
    if (!slash || slash == tmp) return;
    *slash = '\0';
    mkdir(tmp, 0777);
}

static int process_alive(pid_t pid) { return pid > 1 && kill(pid, 0) == 0; }

static void terminate_process(pid_t pid) {
    if (pid <= 1 || pid == getpid()) return;
    (void)kill(pid, SIGTERM);
    for (int attempt = 0; attempt < KILL_WAIT_ATTEMPTS && process_alive(pid);
         ++attempt) {
        usleep(KILL_WAIT_US);
    }
    if (process_alive(pid)) (void)kill(pid, SIGKILL);
}

static pid_t read_recorded_pid(void) {
    FILE *file = fopen(KLOGSRV_PID_PATH, "r");
    if (!file) return -1;

    char value[32];
    if (!fgets(value, sizeof(value), file)) {
        fclose(file);
        return -1;
    }
    fclose(file);

    char *end = NULL;
    errno = 0;
    const long pid = strtol(value, &end, 10);
    if (errno != 0 || end == value || pid <= 1 || pid > 0x7fffffffL) return -1;
    return (pid_t)pid;
}

static void persist_pid(pid_t pid) {
    if (pid <= 1) {
        unlink(KLOGSRV_PID_PATH);
        return;
    }
    mkdir_parent(KLOGSRV_PID_PATH);
    FILE *file = fopen(KLOGSRV_PID_PATH, "w");
    if (!file) return;
    (void)fprintf(file, "%d\n", (int)pid);
    fclose(file);
}

/* A previous plugin instance may have been killed while klogsrv kept running;
 * clean the orphan up before spawning a fresh server. */
static void kill_orphan(void) {
    const pid_t orphan = read_recorded_pid();
    if (orphan <= 1) return;
    if (!process_alive(orphan)) {
        unlink(KLOGSRV_PID_PATH);
        return;
    }
    terminate_process(orphan);
    unlink(KLOGSRV_PID_PATH);
}

static int write_payload_file(void) {
    mkdir_parent(KLOGSRV_RUNTIME_PATH);

    FILE *file = fopen(KLOGSRV_RUNTIME_PATH, "wb");
    if (!file) return 0;

    const size_t size = klogsrv_elf_size();
    const size_t written = fwrite(klogsrv_elf_start, 1, size, file);
    if (written != size || fflush(file) != 0) {
        fclose(file);
        unlink(KLOGSRV_RUNTIME_PATH);
        return 0;
    }
    if (fclose(file) != 0) {
        unlink(KLOGSRV_RUNTIME_PATH);
        return 0;
    }
    return 1;
}

int klog_service_init(klog_service *service, uint16_t port) {
    if (!service || port == 0) return 0;
    memset(service, 0, sizeof(*service));
    if (pthread_mutex_init(&service->mutex, NULL) != 0) return 0;
    service->port = port;
    service->initialized = 1;
    return 1;
}

void klog_service_stop(klog_service *service) {
    if (!service || !service->initialized) return;

    pthread_mutex_lock(&service->mutex);
    const pid_t child = service->child_pid;
    service->child_pid = 0;
    service->running = 0;
    pthread_mutex_unlock(&service->mutex);

    if (child > 1) {
        terminate_process(child);
        unlink(KLOGSRV_PID_PATH);
        KLOG_LOG_PRINTF("klogsrv stopped (pid %d)\n", (int)child);
    }
}

void klog_service_destroy(klog_service *service) {
    if (!service || !service->initialized) return;
    klog_service_stop(service);
    pthread_mutex_destroy(&service->mutex);
    memset(service, 0, sizeof(*service));
}

int klog_service_start(klog_service *service) {
    if (!service || !service->initialized) return 0;
    klog_service_stop(service);
    kill_orphan();

    if (!elfldr_client_available()) {
        KLOG_LOG_PRINTF("private elfldr :%u unavailable\n", KLOG_ELFLDR_PORT);
        return 0;
    }
    if (!write_payload_file()) {
        KLOG_LOG_PRINTF("failed to write %s\n", KLOGSRV_RUNTIME_PATH);
        return 0;
    }

    char args[32];
    snprintf(args, sizeof(args), "-p %u", (unsigned)service->port);
    const int child = elfldr_client_launch_file_get_pid(KLOGSRV_RUNTIME_PATH,
                                                        args);
    if (child <= 1) {
        KLOG_LOG_PRINTF("failed to launch %s on TCP %u\n",
                        KLOGSRV_RUNTIME_PATH, (unsigned)service->port);
        return 0;
    }

    pthread_mutex_lock(&service->mutex);
    service->child_pid = child;
    service->running = 1;
    pthread_mutex_unlock(&service->mutex);
    persist_pid((pid_t)child);

    KLOG_LOG_PRINTF("klogsrv started on TCP %u (pid %d)\n",
                    (unsigned)service->port, child);
    return 1;
}

int klog_service_reconfigure(klog_service *service, uint16_t port) {
    if (!service || !service->initialized || port == 0) return 0;

    pthread_mutex_lock(&service->mutex);
    const uint16_t previous_port = service->port;
    const int was_running = service->running;
    if (!was_running) service->port = port;
    pthread_mutex_unlock(&service->mutex);
    if (!was_running) return 1;

    pthread_mutex_lock(&service->mutex);
    service->port = port;
    pthread_mutex_unlock(&service->mutex);
    if (klog_service_start(service)) return 1;

    pthread_mutex_lock(&service->mutex);
    service->port = previous_port;
    pthread_mutex_unlock(&service->mutex);
    (void)klog_service_start(service);
    return 0;
}

int klog_service_restart(klog_service *service) {
    if (!service || !service->initialized) return 0;
    return klog_service_start(service);
}

int klog_service_running(klog_service *service) {
    if (!service || !service->initialized) return 0;
    pthread_mutex_lock(&service->mutex);
    const int running = service->running;
    const pid_t child = service->child_pid;
    pthread_mutex_unlock(&service->mutex);
    return running && process_alive(child);
}

uint16_t klog_service_port(klog_service *service) {
    if (!service || !service->initialized) return 0;
    pthread_mutex_lock(&service->mutex);
    const uint16_t port = service->port;
    pthread_mutex_unlock(&service->mutex);
    return port;
}
