#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// The emulator allocates from the host heap; report desktop-scale numbers so the
// firmware never takes its low-memory paths.
#define MALLOC_CAP_DEFAULT (1 << 0)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_SPIRAM (1 << 10)

inline size_t heap_caps_get_free_size(uint32_t) { return 8 * 1024 * 1024; }
inline size_t heap_caps_get_total_size(uint32_t) { return 16 * 1024 * 1024; }
inline size_t heap_caps_get_minimum_free_size(uint32_t) { return 6 * 1024 * 1024; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return 6 * 1024 * 1024; }
inline void* heap_caps_malloc(size_t size, uint32_t) { return std::malloc(size); }
inline void* heap_caps_calloc(size_t n, size_t size, uint32_t) { return std::calloc(n, size); }
inline void heap_caps_free(void* ptr) { std::free(ptr); }
