#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Symbols emitted by source/embedded_payloads.c (.incbin of the vendored
 * klogsrv payload). The blob is a plain PS5 ELF; it is written to disk and
 * launched through the OnionHEN private loader, never mapped in-process. */
extern const uint8_t klogsrv_elf_start[];
extern const uint8_t klogsrv_elf_end[];

static inline size_t klogsrv_elf_size(void) {
    return (size_t)(klogsrv_elf_end - klogsrv_elf_start);
}

#ifdef __cplusplus
}
#endif
