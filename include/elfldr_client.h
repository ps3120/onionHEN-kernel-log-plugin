#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Port of the OnionHEN private runtime loader (onion_elfldr.elf). Plugins are
 * themselves launched through it, so it is guaranteed to be listening while
 * this plugin runs. */
#define KLOG_ELFLDR_PORT 9020u

/**
 * Launch a payload ELF from an absolute path through the private loader.
 * `args` (optional, may be NULL) is URI-encoded and passed to the payload's
 * argv. Blocks until the loader finishes mapping the ELF and returns the
 * exact child PID, or -1 on failure.
 */
int elfldr_client_launch_file_get_pid(const char *abs_path, const char *args);

/**
 * Cheap connectivity probe for the private loader.
 */
int elfldr_client_available(void);

#ifdef __cplusplus
}
#endif
