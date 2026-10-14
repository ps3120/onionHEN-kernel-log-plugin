#pragma once

#include <pthread.h>

#include <stdint.h>

typedef struct klog_service {
    pthread_mutex_t mutex;
    uint16_t port;
    int child_pid;
    int initialized;
    int running;
} klog_service;

int klog_service_init(klog_service *service, uint16_t port);
void klog_service_stop(klog_service *service);
void klog_service_destroy(klog_service *service);
int klog_service_start(klog_service *service);
int klog_service_reconfigure(klog_service *service, uint16_t port);
int klog_service_restart(klog_service *service);
int klog_service_running(klog_service *service);
uint16_t klog_service_port(klog_service *service);
