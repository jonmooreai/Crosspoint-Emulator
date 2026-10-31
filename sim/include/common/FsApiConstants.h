#pragma once

// Use the host's <fcntl.h> values for the flags it defines, so every translation
// unit agrees on them whether or not it also pulls in <fcntl.h> itself. The sim
// maps these onto fopen modes, so the exact values do not matter.
#include <fcntl.h>

#ifndef O_AT_END
#define O_AT_END 0x40000000
#endif
#ifndef O_READ
#define O_READ O_RDONLY
#endif
#ifndef O_WRITE
#define O_WRITE O_WRONLY
#endif

typedef int oflag_t;
