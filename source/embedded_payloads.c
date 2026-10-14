#include "klog_payload.h"

#ifndef KLOGSRV_ELF_PATH
#define KLOGSRV_ELF_PATH "assets/klogsrv-ps5.elf"
#endif

/* Embeds the vendored klogsrv payload ELF into the plugin binary the same way
 * OnionHEN embeds its own payloads (inline .incbin). The path comes from the
 * build system and is validated against the ELF magic at configure time. */
__asm__(
    ".global klogsrv_elf_start\n"
    ".type   klogsrv_elf_start, @object\n"
    ".align  16\n"
    "klogsrv_elf_start:\n"
    ".incbin \"" KLOGSRV_ELF_PATH "\"\n"
    ".global klogsrv_elf_end\n"
    ".type   klogsrv_elf_end, @object\n"
    "klogsrv_elf_end:\n"
    ".global klogsrv_elf_size\n"
    ".type   klogsrv_elf_size, @object\n"
    ".align  4\n"
    "klogsrv_elf_size:\n"
    ".int    klogsrv_elf_end - klogsrv_elf_start\n"
);
