// Limine Boot Protocol Header for QAIOS
// Uses Limine bootloader to load kernel directly in 64-bit mode

#pragma once

#include "../QCommon/QCTypes.h"

// Limine uses these sections for requests
#define LIMINE_REQUESTS_START_MARKER                                                                                      \
    __attribute__((used, section(".limine_requests_start_marker"))) volatile uint64_t limine_requests_start_marker[4] = { \
        0xf9562b2d5c95a6c8, 0x6a7b384944536bdc,                                                                           \
        0xf9562b2d5c95a6c8, 0x6a7b384944536bdc};

#define LIMINE_REQUESTS_END_MARKER                                                                                    \
    __attribute__((used, section(".limine_requests_end_marker"))) volatile uint64_t limine_requests_end_marker[2] = { \
        0xadc0e0531bb10d03, 0x9572709f31764c62};

// Base revision - must be exactly this format
#define LIMINE_BASE_REVISION                                                                         \
    __attribute__((used, section(".limine_requests"))) volatile uint64_t limine_base_revision[3] = { \
        0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 3};

// Common magic bytes for requests
#define LIMINE_COMMON_MAGIC_0 0xc7b1dd30df4c8b88ULL
#define LIMINE_COMMON_MAGIC_1 0x0a82e883a194f07bULL

// Request structures
struct limine_framebuffer
{
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;
    uint64_t mode_count;
    void **modes;
};

struct limine_framebuffer_response
{
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request
{
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

// Memory map types
#define LIMINE_MEMMAP_USABLE 0
#define LIMINE_MEMMAP_RESERVED 1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE 2
#define LIMINE_MEMMAP_ACPI_NVS 3
#define LIMINE_MEMMAP_BAD_MEMORY 4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES 6
#define LIMINE_MEMMAP_FRAMEBUFFER 7

struct limine_memmap_entry
{
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response
{
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request
{
    uint64_t id[4];
    uint64_t revision;
    struct limine_memmap_response *response;
};

// Declare requests (place in .limine_requests section)
#define LIMINE_FRAMEBUFFER_REQUEST                                                                                        \
    __attribute__((used, section(".limine_requests"))) volatile struct limine_framebuffer_request framebuffer_request = { \
        .id = {LIMINE_COMMON_MAGIC_0, LIMINE_COMMON_MAGIC_1,                                                              \
               0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL},                                                             \
        .revision = 0,                                                                                                    \
        .response = nullptr};

#define LIMINE_MEMMAP_REQUEST                                                                                   \
    __attribute__((used, section(".limine_requests"))) volatile struct limine_memmap_request memmap_request = { \
        .id = {LIMINE_COMMON_MAGIC_0, LIMINE_COMMON_MAGIC_1,                                                    \
               0x67cf3d9d378a806fULL, 0xe304acdfc50c3c62ULL},                                                   \
        .revision = 0,                                                                                          \
        .response = nullptr};

// HHDM (Higher Half Direct Map) structures
struct limine_hhdm_response
{
    uint64_t revision;
    uint64_t offset; // Virtual address offset to add to physical addresses
};

struct limine_hhdm_request
{
    uint64_t id[4];
    uint64_t revision;
    struct limine_hhdm_response *response;
};

#define LIMINE_HHDM_REQUEST                                                                                 \
    __attribute__((used, section(".limine_requests"))) volatile struct limine_hhdm_request hhdm_request = { \
        .id = {LIMINE_COMMON_MAGIC_0, LIMINE_COMMON_MAGIC_1,                                                \
               0x48dcf1cb8ad2b852ULL, 0x63984e959a98244bULL},                                               \
        .revision = 0,                                                                                      \
        .response = nullptr};

// Kernel Address structures (to get physical/virtual base)
struct limine_kernel_address_response
{
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};

struct limine_kernel_address_request
{
    uint64_t id[4];
    uint64_t revision;
    struct limine_kernel_address_response *response;
};

#define LIMINE_KERNEL_ADDRESS_REQUEST                                                                                        \
    __attribute__((used, section(".limine_requests"))) volatile struct limine_kernel_address_request kernel_addr_request = { \
        .id = {LIMINE_COMMON_MAGIC_0, LIMINE_COMMON_MAGIC_1,                                                                 \
               0x71ba76863cc55f63ULL, 0xb2644a48c516a487ULL},                                                                \
        .revision = 0,                                                                                                       \
        .response = nullptr};
