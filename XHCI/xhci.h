#pragma once

#include "kernel/quarma/stdtools.h"
#include "kernel/quarma/spinlock.h"
#include "drivers/usb/usb.h"

// USB HID device timing (microseconds) - Razer Mamba and similar mice
#define USB_HID_MOUSE_WAIT_MIN_US 600
#define USB_HID_MOUSE_WAIT_MAX_US 800

// XHCI Capability Registers (offset from base)
#define XHCI_CAP_CAPLENGTH      0x00  // 1 byte - Capability Register Length
#define XHCI_CAP_HCIVERSION     0x02  // 2 bytes - Interface Version Number
#define XHCI_CAP_HCSPARAMS1     0x04  // Structural Parameters 1
#define XHCI_CAP_HCSPARAMS2     0x08  // Structural Parameters 2
#define XHCI_CAP_HCSPARAMS3     0x0C  // Structural Parameters 3
#define XHCI_CAP_HCCPARAMS1     0x10  // Capability Parameters 1
#define XHCI_CAP_DBOFF          0x14  // Doorbell Offset
#define XHCI_CAP_RTSOFF         0x18  // Runtime Register Space Offset

// XHCI Operational Registers (offset from CAPLENGTH)
#define XHCI_OP_USBCMD          0x00  // USB Command
#define XHCI_OP_USBSTS          0x04  // USB Status
#define XHCI_OP_PAGESIZE        0x08  // Page Size
#define XHCI_OP_DNCTRL          0x14  // Device Notification Control
#define XHCI_OP_CRCR            0x18  // Command Ring Control (64-bit)
#define XHCI_OP_DCBAAP          0x30  // Device Context Base Address Array Pointer (64-bit)
#define XHCI_OP_CONFIG          0x38  // Configure

// USB Command Register bits
#define XHCI_CMD_RUN            (1 << 0)   // Run/Stop
#define XHCI_CMD_HCRST          (1 << 1)   // Host Controller Reset
#define XHCI_CMD_INTE           (1 << 2)   // Interrupter Enable
#define XHCI_CMD_HSEE           (1 << 3)   // Host System Error Enable
#define XHCI_CMD_EWE            (1 << 10)  // Enable Wrap Event

// USB Status Register bits
#define XHCI_STS_HCH            (1 << 0)   // HC Halted
#define XHCI_STS_HSE            (1 << 2)   // Host System Error
#define XHCI_STS_EINT           (1 << 3)   // Event Interrupt
#define XHCI_STS_PCD            (1 << 4)   // Port Change Detect
#define XHCI_STS_CNR            (1 << 11)  // Controller Not Ready

// Port Status and Control Register offsets (from operational base + 0x400)
#define XHCI_PORT_OFFSET        0x400
#define XHCI_PORTSC             0x00  // Port Status and Control
#define XHCI_PORTPMSC           0x04  // Port Power Management Status and Control
#define XHCI_PORTLI             0x08  // Port Link Info
#define XHCI_PORTHLPMC          0x0C  // Port Hardware LPM Control

// Port Status and Control Register bits
#define XHCI_PORTSC_CCS         (1 << 0)   // Current Connect Status
#define XHCI_PORTSC_PED         (1 << 1)   // Port Enabled/Disabled
#define XHCI_PORTSC_PR          (1 << 4)   // Port Reset
#define XHCI_PORTSC_PLS_MASK    (0xF << 5) // Port Link State
#define XHCI_PORTSC_PP          (1 << 9)   // Port Power
#define XHCI_PORTSC_SPEED_MASK  (0xF << 10)// Port Speed
#define XHCI_PORTSC_CSC         (1 << 17)  // Connect Status Change
#define XHCI_PORTSC_PEC         (1 << 18)  // Port Enabled/Disabled Change
#define XHCI_PORTSC_WRC         (1 << 19)  // Warm Port Reset Change
#define XHCI_PORTSC_PRC         (1 << 21)  // Port Reset Change
#define XHCI_PORTSC_PLC         (1 << 22)  // Port Link State Change
#define XHCI_PORTSC_CEC         (1 << 23)  // Port Config Error Change

// TRB (Transfer Request Block) Types
#define XHCI_TRB_NORMAL         1
#define XHCI_TRB_SETUP          2
#define XHCI_TRB_DATA           3
#define XHCI_TRB_STATUS         4
#define XHCI_TRB_LINK           6
#define XHCI_TRB_EVENT_DATA     7
#define XHCI_TRB_NOOP           8
#define XHCI_TRB_ENABLE_SLOT    9
#define XHCI_TRB_ADDRESS_DEV    11
#define XHCI_TRB_CONFIG_EP      12
#define XHCI_TRB_CONFIG_EP      12
#define XHCI_TRB_TRANSFER       32
#define XHCI_TRB_CMD_COMPLETE   33
#define XHCI_TRB_PORT_STATUS    34

// TRB flags
#define XHCI_TRB_CYCLE          (1 << 0)
#define XHCI_TRB_IOC            (1 << 5)   // Interrupt on Completion

// Transfer Request Block structure
typedef struct __attribute__((packed)) {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

// Event Ring Segment Table Entry
typedef struct __attribute__((packed)) {
    uint64_t ring_segment_base_address;
    uint16_t ring_segment_size;
    uint16_t reserved1;
    uint32_t reserved2;
} xhci_erst_entry_t;

// Device Context structure (simplified)
typedef struct __attribute__((packed)) {
    uint32_t slot_context[8];
    uint32_t endpoint_contexts[31][8];  // Up to 31 endpoints
} xhci_device_context_t;

// Input Context structure
typedef struct __attribute__((packed)) {
    uint32_t input_control_context[8];
    xhci_device_context_t device_context;
} xhci_input_context_t;

// XHCI Controller structure
typedef struct {
    uintptr_t mmio_base;          // Memory-mapped I/O base address (64-bit)
    uint8_t cap_length;           // Capability register length
    uint32_t operational_base;    // Operational registers base
    uint32_t runtime_base;        // Runtime registers base
    uint32_t doorbell_base;       // Doorbell array base
    uint32_t max_slots;           // Maximum device slots
    uint32_t max_ports;           // Maximum root hub ports
    uint32_t max_intrs;           // Maximum interrupters
    
    xhci_trb_t *command_ring;     // Command ring buffer
    xhci_trb_t *event_ring;       // Event ring buffer
    xhci_erst_entry_t *erst;      // Event Ring Segment Table
    uint64_t *dcbaap;             // Device Context Base Address Array
    
    uint8_t command_cycle;        // Command ring cycle bit
    uint8_t event_cycle;          // Event ring cycle bit
    uint32_t command_index;       // Current command ring index
    uint32_t event_index;         // Current event ring index
    
    uint8_t pci_bus;
    uint8_t pci_slot;
    uint8_t pci_func;
    
    // Per-device transfer rings (simple array for now)
    xhci_trb_t *device_transfer_rings[256];  // One per slot
    uint32_t transfer_ring_indices[256];     // Current enqueue index per slot
    uint32_t transfer_ring_dequeue[256];     // Current dequeue index per slot (tracks completed TRBs)
    uint8_t transfer_ring_cycles[256];       // Cycle bit per slot
    uint8_t transfer_ring_dequeue_cycles[256]; // Dequeue cycle bit per slot
    
    // Synchronization for event ring access
    spinlock_t event_ring_lock;              // Protects event ring state
    
    // Work queue for deferred processing (outside spinlock)
    struct {
        uint8_t slot_id;
        uint8_t endpoint_id;
        uint8_t completion_code;
    } work_queue[256];
    uint32_t work_queue_head;
    uint32_t work_queue_tail;
    
    // Statistics counters
    uint32_t stats_events_drained;
    uint32_t stats_queued_work_items;
    uint32_t stats_ring_empty_hits;
    uint32_t stats_total_events;
    
    // Wrap detection state
    bool cycle_flipped_this_pass;
} xhci_controller_t;

// Function prototypes
int xhci_init(void);
int xhci_pci_init(void);
xhci_controller_t* xhci_find_controller(void);
int xhci_reset(xhci_controller_t *xhci);
int xhci_start(xhci_controller_t *xhci);
int xhci_stop(xhci_controller_t *xhci);
int xhci_detect_ports(xhci_controller_t *xhci);
int xhci_reset_port(xhci_controller_t *xhci, uint8_t port);
int xhci_enable_slot(xhci_controller_t *xhci);
int xhci_address_device(xhci_controller_t *xhci, uint8_t slot, uint8_t port);
int xhci_configure_endpoint(xhci_controller_t *xhci, uint8_t slot);
int xhci_configure_endpoint_params(xhci_controller_t *xhci, uint8_t slot,
                                   uint16_t max_packet_size,
                                   uint8_t interval);
int xhci_enumerate_devices(xhci_controller_t *xhci);
int xhci_queue_transfer(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, void *buffer, uint16_t length);
void xhci_poll_events(xhci_controller_t *xhci);
xhci_controller_t* xhci_get_controller(void);

// Low-level I/O helpers (64-bit address support)
static inline uint32_t xhci_read32(uintptr_t addr) {
    return *(volatile uint32_t*)addr;
}

static inline void xhci_write32(uintptr_t addr, uint32_t value) {
    *(volatile uint32_t*)addr = value;
}

static inline uint64_t xhci_read64(uintptr_t addr) {
    uint32_t low = *(volatile uint32_t*)addr;
    uint32_t high = *(volatile uint32_t*)(addr + 4);
    return ((uint64_t)high << 32) | low;
}

static inline void xhci_write64(uintptr_t addr, uint64_t value) {
    *(volatile uint32_t*)addr = (uint32_t)(value & 0xFFFFFFFF);
    *(volatile uint32_t*)(addr + 4) = (uint32_t)(value >> 32);
}

