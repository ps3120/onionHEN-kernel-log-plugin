/* Kernel Log plugin: OnionHEN private loader (:9020) socket client.
 * Mirrors the wire protocol used by the OnionHEN daemon's plugin manager:
 *   - "file:<abs_path>[?args=<uri-encoded>]\n" request line
 *   - "OK <pid>\n" response after ELF mapping/relocation completes
 */

#include "elfldr_client.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x20000
#endif

#define ELFLDR_CONNECT_TIMEOUT_MS 2000
#define ELFLDR_RESPONSE_TIMEOUT_MS (120 * 1000)

static int connect_loader(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(0x7f000001u);

    const int flags = fcntl(fd, F_GETFL, 0);
    const int nonblock_ok =
        flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
    if (!nonblock_ok) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(fd);
            return -1;
        }
        return fd;
    }

    const int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        close(fd);
        return -1;
    }
    if (rc != 0) {
        struct pollfd pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.fd = fd;
        pfd.events = POLLOUT;
        if (poll(&pfd, 1, ELFLDR_CONNECT_TIMEOUT_MS) <= 0) {
            close(fd);
            return -1;
        }
        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
            close(fd);
            return -1;
        }
    }

    (void)fcntl(fd, F_SETFL, flags);
    return fd;
}

static int send_all(int fd, const void *buf, size_t size) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t sent = 0;
    while (sent < size) {
        const ssize_t n = send(fd, p + sent, size - sent, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

int elfldr_client_available(void) {
    int fd = connect_loader((uint16_t)KLOG_ELFLDR_PORT);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static int uri_encode_component(const char *input, char *out,
                                size_t out_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;
    if (!out || out_size == 0) return -1;
    if (!input) input = "";

    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        const int safe = (*p >= 'a' && *p <= 'z') ||
                         (*p >= 'A' && *p <= 'Z') ||
                         (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ||
                         *p == '.' || *p == '~';
        const size_t needed = safe ? 1u : 3u;
        if (used + needed >= out_size) return -1;
        if (safe) {
            out[used++] = (char)*p;
        } else {
            out[used++] = '%';
            out[used++] = hex[*p >> 4];
            out[used++] = hex[*p & 0x0f];
        }
    }
    out[used] = '\0';
    return 0;
}

static int read_pid_response(int fd) {
    char buf[64];
    size_t used = 0;
    int complete = 0;
    struct timeval timeout;
    /* The private loader replies only after ELF mapping/relocation completes.
     * Keep the exact-PID channel open long enough for large payloads. */
    timeout.tv_sec = ELFLDR_RESPONSE_TIMEOUT_MS / 1000;
    timeout.tv_usec = 0;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (used < sizeof(buf) - 1) {
        const ssize_t n = recv(fd, buf + used, sizeof(buf) - 1 - used, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        used += (size_t)n;
        if (memchr(buf, '\n', used) != NULL) {
            complete = 1;
            break;
        }
    }
    buf[used] = '\0';

    if (!complete || strncmp(buf, "OK ", 3) != 0) return -1;

    char *end = NULL;
    errno = 0;
    const long pid = strtol(buf + 3, &end, 10);
    if (errno != 0 || end == buf + 3 || pid <= 1 || pid > INT_MAX ||
        *end != '\n' || end[1] != '\0') {
        return -1;
    }
    return (int)pid;
}

int elfldr_client_launch_file_get_pid(const char *abs_path, const char *args) {
    if (!abs_path || abs_path[0] != '/') return -1;

    char encoded_args[768];
    if (args && args[0] &&
        uri_encode_component(args, encoded_args, sizeof(encoded_args)) != 0) {
        return -1;
    }

    char line[1280];
    int length = args && args[0]
                     ? snprintf(line, sizeof(line), "file:%s?args=%s\n",
                                abs_path, encoded_args)
                     : snprintf(line, sizeof(line), "file:%s\n", abs_path);
    if (length <= 0 || (size_t)length >= sizeof(line)) return -1;

    int fd = connect_loader((uint16_t)KLOG_ELFLDR_PORT);
    if (fd < 0) return -1;

    if (send_all(fd, line, (size_t)length) != 0) {
        close(fd);
        return -1;
    }

    const int pid = read_pid_response(fd);
    close(fd);
    return pid;
}
