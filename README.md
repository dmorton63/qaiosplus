# QAIOS+ Kernel

A modular x86-64 kernel with unified naming conventions, built with Limine bootloader.

## Project Structure

```
qaiosplus-ue-naming/
├── kernel/                  # Kernel executable target
│   ├── QKMain.cpp          # Entry point (kernel_main)
│   ├── QKBoot.asm          # Assembly bootstrap
│   ├── linker.ld           # Linker script
│   └── drivers/            # Hardware drivers (new structure)
│       ├── QKDrvBase.h     # Base driver interfaces
│       ├── QKDrvManager.*  # Driver probing/selection
│       ├── PS2/            # PS/2 keyboard/mouse drivers
│       ├── UHCI/           # USB 1.1 controller driver
│       └── XHCI/           # USB 3.0 controller driver
│
├── QCommon/                 # Common utilities library
│   ├── QCTypes.h           # Type definitions (u8, i32, PhysAddr, etc.)
│   ├── QCString.*          # String/memory operations
│   ├── QCLogger.*          # Logging system
│   └── QCVector.h          # Dynamic array template
│
├── QKernel/                 # Kernel services library
│   ├── QKInterrupts.*      # IDT, IRQ handlers, PIC
│   ├── QKScheduler.*       # Task scheduling
│   └── QKTaskManager.*     # Process management
│
├── QKMemory/                # Memory management
│   ├── QKMemPMM.*          # Physical memory manager
│   ├── QKMemVMM.*          # Virtual memory manager
│   ├── QKMemHeap.*         # Kernel heap allocator
│   └── QKMemTranslator.*   # Address translation/MMIO mapping
│
├── QArch/                   # Architecture-specific code
│   ├── QArchGDT.*          # Global Descriptor Table
│   ├── QArchIDT.*          # Interrupt Descriptor Table
│   ├── QArchPCI.*          # PCI bus enumeration
│   └── QArchPort.*         # I/O port operations
│
├── QEvent/                  # Event system
│   ├── QKEventManager.*    # Event dispatch
│   ├── QKEventQueue.*      # Event queue
│   └── QKEventListener.*   # Event handlers
│
├── QDrivers/                # Hardware drivers (Timer, BGA)
│   ├── QDrvTimer.*         # PIT/APIC timer
│   └── QDrvBGA.*           # Bochs Graphics Adapter
│
├── QWindowing/              # Windowing system
│   ├── QWWindowManager.*   # Window management
│   ├── QWWindow.*          # Window class
│   ├── QWFramebuffer.*     # Framebuffer access
│   └── QWCompositor.*      # Compositing/cursor
│
├── QWControls/              # UI controls
│   ├── QWCtrlButton.*      # Button widget
│   ├── QWCtrlLabel.*       # Label widget
│   └── QWCtrlTextBox.*     # Text input
│
├── QFileSystem/             # File system (VFS, FAT32)
├── QNetwork/                # Networking (stub)
├── QPR/                     # Process runtime (stub)
└── QQuantum/                # Quantum computing (stub)
```

## Naming Conventions

### Modules
- `QC*` - **Q**AIOS **C**ommon (shared utilities)
- `QK*` - **Q**AIOS **K**ernel (kernel services)
- `QArch*` - **Q**AIOS **Arch**itecture (x86-64 specific)
- `QDrv*` - **Q**AIOS **Drv**ivers (hardware drivers)
- `QKDrv*` - **Q**AIOS **K**ernel **Drv**ivers (kernel-level input drivers)
- `QW*` - **Q**AIOS **W**indowing
- `QWCtrl*` - **Q**AIOS **W**indowing **Ctrl**ontrols
- `QFS*` - **Q**AIOS **F**ile **S**ystem

### Files
- Headers: `Q<Module><Class>.h`
- Implementation: `Q<Module><Class>.cpp`
- Example: `QKDrvPS2Mouse.h`, `QKDrvPS2Mouse.cpp`

### Namespaces
```cpp
namespace QC { }       // Common
namespace QK { }       // Kernel
namespace QArch { }    // Architecture
namespace QDrv { }     // Drivers
namespace QKDrv { }    // Kernel drivers
namespace QW { }       // Windowing
```

## Driver Architecture

### Input Driver Hierarchy
```
QKDrv::DriverBase           # Base interface
├── QKDrv::MouseDriver      # Mouse interface (relative/absolute)
└── QKDrv::KeyboardDriver   # Keyboard interface

QKDrv::PS2::Mouse           # PS/2 mouse implementation
QKDrv::PS2::Keyboard        # PS/2 keyboard implementation
QKDrv::UHCI::Controller     # USB 1.1 controller
QKDrv::XHCI::Controller     # USB 3.0 controller
```

### Driver Manager
The `QKDrv::Manager` probes for available controllers and selects the best driver:
1. **USB** (xHCI, then UHCI) - preferred for tablet/absolute positioning
2. **PS/2** - fallback for keyboard/mouse

## Building

```bash
# Configure
cmake -B build -S .

# Build
cmake --build build

# Output: build/qaios.elf, build/qaios.bin
```

### Build Tasks (VS Code)
- **Build Kernel**: `cmake --build build`
- **Configure CMake**: `cmake -B build -S .`
- **Clean**: `rm -rf build`

## Running (QEMU)

```bash
# Basic run
qemu-system-x86_64 -kernel build/qaios.elf -serial stdio

# With USB tablet (for absolute mouse positioning)
qemu-system-x86_64 -kernel build/qaios.elf -serial stdio -usb -device usb-tablet

# Create ISO with Limine
./build.sh
qemu-system-x86_64 -cdrom qaios.iso -serial stdio
```

## Recent Changes (Feb 9, 2026)

### Driver Refactoring
- Created unified driver structure under `kernel/drivers/`
- Implemented `QKDrv::Manager` for driver probing and selection
- Separated drivers by controller type: PS2, UHCI, XHCI
- Removed old `QUSB/` module (replaced by kernel/drivers/UHCI, XHCI)
- Removed old `drivers/usb/` C code
- Cleaned up `QDrivers/` to only keep Timer and BGA

### Input System
- PS/2 mouse/keyboard now use new `QKDrv::PS2::*` classes
- Mouse callback uses `QKDrv::MouseReport` (supports both relative and absolute)
- Keyboard uses `QKDrv::PS2::KeyEvent` with full modifier support
- Mouse cursor starts at screen center (not corner)

### Boot Fixes (earlier session)
- Fixed IDT selector (0x28 for Limine's 64-bit CS)
- Fixed IRQ stubs (dummy error code for consistency)
- Enabled IRQ2 cascade for slave PIC (mouse IRQ12)
- Fixed mouse axis inversion for VM compatibility

## Key Components

### Memory
- **Early heap**: 32MB static buffer before PMM
- **Early DMA**: 1MB identity-mapped buffer for USB
- **PMM**: Page frame allocator (4KB pages)
- **VMM**: Virtual memory with paging

### Events
- Event queue with type-based filtering
- Listener registration with category masks
- Mouse/keyboard events routed through `QK::Event::EventManager`

### Windowing
- Compositor with software cursor (12x16 arrow)
- Window manager with focus tracking
- Desktop window with shutdown button (Ctrl+Q or button click)

## License

MIT License

## Author

QAIOS Project
# qaiosplus
