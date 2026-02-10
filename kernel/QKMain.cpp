// QAIOS Kernel Main Entry Point
// QKMain.cpp

#include "QCTypes.h"
#include "QCLogger.h"
#include "QCBuiltins.h"
#include "QKKernel.h"
#include "QKMemPMM.h"
#include "QKMemVMM.h"
#include "QKMemHeap.h"
#include "QArchGDT.h"
#include "QArchIDT.h"
#include "QArchCPU.h"
#include "QArchPCI.h"
#include "QKInterrupts.h"
#include "QDrvTimer.h"
#include "QKDrvManager.h"
#include "PS2/QKDrvPS2Keyboard.h"
#include "PS2/QKDrvPS2Mouse.h"
#include "QWFramebuffer.h"
#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QKEventManager.h"
#include "QKEventListener.h"
#include "QWCtrlButton.h"

// Limine requests are defined in QKBoot.asm

// Boot information (unused with Limine - info comes from requests)
struct BootInfo
{
    QC::u32 magic;
    QC::u32 size;
};

// External symbols from linker
extern "C"
{
    extern QC::u8 _kernel_start[];
    extern QC::u8 _kernel_end[];
    extern QC::u8 _bss_start[];
    extern QC::u8 _bss_end[];

    // Limine requests from QKBoot.asm
    extern QC::u64 limine_framebuffer_request[];
}

// Helper to clear BSS
static void clearBSS()
{
    QC::u8 *bss = _bss_start;
    while (bss < _bss_end)
    {
        *bss++ = 0;
    }
}

// Early heap buffer - 32MB static allocation for heap before PMM is ready
alignas(4096) static QC::u8 earlyHeapBuffer[32 * 1024 * 1024];

// Early DMA buffer for USB - 1MB, separate from heap (identity-mapped)
alignas(4096) static QC::u8 earlyDMABuffer[1 * 1024 * 1024];
static QC::usize earlyDMAOffset = 0;

// Simple page allocator for early USB (identity-mapped, so virt == phys)
extern "C" QC::PhysAddr earlyAllocatePage()
{
    if (earlyDMAOffset + 4096 > sizeof(earlyDMABuffer))
        return 0;
    QC::PhysAddr addr = reinterpret_cast<QC::PhysAddr>(&earlyDMABuffer[earlyDMAOffset]);
    earlyDMAOffset += 4096;
    return addr;
}

// Early console output (before logger is initialized)
static void earlyPrint(const char *msg)
{
    volatile QC::u16 *video = reinterpret_cast<volatile QC::u16 *>(0xB8000);
    static int pos = 0;

    while (*msg)
    {
        if (*msg == '\n')
        {
            pos = ((pos / 80) + 1) * 80;
        }
        else
        {
            video[pos++] = static_cast<QC::u16>(*msg) | 0x0F00;
        }
        ++msg;
    }
}

// Serial port output for debugging
static void serialInit()
{
    // Initialize COM1 at 0x3F8
    QC::outb(0x3F8 + 1, 0x00); // Disable interrupts
    QC::outb(0x3F8 + 3, 0x80); // Enable DLAB
    QC::outb(0x3F8 + 0, 0x03); // Baud divisor low (38400 baud)
    QC::outb(0x3F8 + 1, 0x00); // Baud divisor high
    QC::outb(0x3F8 + 3, 0x03); // 8N1
    QC::outb(0x3F8 + 2, 0xC7); // Enable FIFO
    QC::outb(0x3F8 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static void serialPrint(const char *msg)
{
    while (*msg)
    {
        // Wait for transmit buffer empty
        while ((QC::inb(0x3F8 + 5) & 0x20) == 0)
            ;
        QC::outb(0x3F8, *msg);
        ++msg;
    }
}

// Kernel panic
extern "C" [[noreturn]] void kernel_panic(const char *message)
{
    // Disable interrupts
    asm volatile("cli");

    earlyPrint("\n\n*** KERNEL PANIC ***\n");
    earlyPrint(message);

    // Halt forever
    for (;;)
    {
        asm volatile("hlt");
    }
}

// C++ global constructors
using ConstructorFunc = void (*)();
extern "C" ConstructorFunc __init_array_start[];
extern "C" ConstructorFunc __init_array_end[];

static void callConstructors()
{
    for (ConstructorFunc *ctor = __init_array_start; ctor < __init_array_end; ++ctor)
    {
        (*ctor)();
    }
}

// Kernel main entry point
extern "C" void kernel_main(QC::u32 magic, BootInfo *bootInfo)
{
    // Initialize serial first for debug output
    serialInit();
    serialPrint("\r\n=== QAIOS Kernel ===\r\n");
    serialPrint("Serial initialized, kernel starting...\r\n");

    // Limine already clears BSS for us, skip clearBSS()
    // clearBSS();
    serialPrint("BSS (skipped - Limine does it)\r\n");

    serialPrint("About to call CPU init\r\n");

    // Initialize CPU features first
    QArch::CPU::instance().initialize();
    serialPrint("CPU initialized\r\n");

    // Set up GDT
    QArch::GDT::instance().initialize();
    serialPrint("GDT initialized\r\n");

    // Set up IDT
    QArch::IDT::instance().initialize();
    serialPrint("IDT initialized\r\n");

    // Initialize interrupt manager (sets up PIC)
    QK::InterruptManager::instance().initialize();
    serialPrint("InterruptManager initialized\r\n");

    // For now, skip subsystem initialization and just prove the kernel runs
    // by drawing something to the framebuffer
    serialPrint("Kernel init complete - entering halt loop\r\n");

    // Enable interrupts
    asm volatile("sti");

    // Draw to Limine framebuffer if available
    // Access the framebuffer response from our Limine request

    QC::u64 *fb_response = reinterpret_cast<QC::u64 *>(limine_framebuffer_request[5]);

    if (fb_response != nullptr)
    {
        serialPrint("Framebuffer response received!\r\n");

        // Limine response structure:
        // [0] = revision
        // [1] = framebuffer_count
        // [2] = framebuffers array pointer
        QC::u64 revision = fb_response[0];
        QC::u64 fb_count = fb_response[1];

        serialPrint("  Revision: ");
        // Simple hex print
        char hexbuf[3] = {'0' + static_cast<char>(revision % 10), '\r', '\n'};
        serialPrint(hexbuf);

        serialPrint("  Count: ");
        hexbuf[0] = '0' + static_cast<char>(fb_count % 10);
        serialPrint(hexbuf);

        if (fb_count > 0)
        {
            serialPrint("Getting framebuffer pointer...\r\n");

            // Get framebuffers array (pointer to pointer)
            QC::u64 **fb_array = reinterpret_cast<QC::u64 **>(fb_response[2]);

            serialPrint("Got fb_array\r\n");

            // Get first framebuffer struct
            QC::u64 *fb = fb_array[0];

            serialPrint("Got fb struct\r\n");

            // Limine framebuffer struct layout:
            // [0] = address (void*)
            // [1] = width (uint64_t)
            // [2] = height (uint64_t)
            // [3] = pitch (uint64_t)
            // [4] = bpp (uint16_t, but padded)
            QC::uptr fbAddress = static_cast<QC::uptr>(fb[0]);
            QC::u32 width = static_cast<QC::u32>(fb[1]);
            QC::u32 height = static_cast<QC::u32>(fb[2]);
            QC::u32 pitch = static_cast<QC::u32>(fb[3]);

            serialPrint("Initializing QWindowing...\r\n");

            // Initialize heap first - required for memory allocations
            serialPrint("Initializing heap...\r\n");
            QK::Memory::Heap::instance().initialize(
                reinterpret_cast<QC::VirtAddr>(earlyHeapBuffer),
                sizeof(earlyHeapBuffer));
            serialPrint("Heap initialized\r\n");

            // Initialize the event system
            QK::Event::EventManager::instance().initialize();
            serialPrint("Event system initialized\r\n");

            // Initialize PCI bus and enumerate devices
            serialPrint("Initializing PCI...\r\n");
            QArch::PCI::instance().initialize();
            serialPrint("PCI initialized\r\n");

            // Initialize driver manager (probes USB and PS/2)
            serialPrint("Initializing drivers...\r\n");
            QKDrv::Manager::instance().setScreenSize(width, height);
            QKDrv::Manager::instance().initialize();
            serialPrint("Drivers initialized\r\n");

            // Create and initialize framebuffer
            static QW::Framebuffer framebuffer;
            framebuffer.initialize(fbAddress, width, height, pitch, QW::PixelFormat::ARGB8888);
            serialPrint("Framebuffer initialized\r\n");

            // Initialize window manager
            serialPrint("About to initialize WindowManager...\r\n");
            QW::WindowManager::instance().initialize(&framebuffer);
            serialPrint("WindowManager initialized\r\n");

            // Get active mouse driver from manager
            serialPrint("Setting up mouse...\r\n");
            auto *mouseDriver = QKDrv::Manager::instance().mouseDriver();

            // Debug: Print screen dimensions
            serialPrint("Screen: ");
            char dimBuf[32];
            int idx = 0;
            if (width >= 1000)
                dimBuf[idx++] = '0' + (width / 1000) % 10;
            if (width >= 100)
                dimBuf[idx++] = '0' + (width / 100) % 10;
            if (width >= 10)
                dimBuf[idx++] = '0' + (width / 10) % 10;
            dimBuf[idx++] = '0' + width % 10;
            dimBuf[idx++] = 'x';
            if (height >= 1000)
                dimBuf[idx++] = '0' + (height / 1000) % 10;
            if (height >= 100)
                dimBuf[idx++] = '0' + (height / 100) % 10;
            if (height >= 10)
                dimBuf[idx++] = '0' + (height / 10) % 10;
            dimBuf[idx++] = '0' + height % 10;
            dimBuf[idx++] = '\r';
            dimBuf[idx++] = '\n';
            dimBuf[idx] = 0;
            serialPrint(dimBuf);

            // Debug: Print button location
            serialPrint("Button at: ");
            idx = 0;
            QC::u32 btnX = width - 120;
            if (btnX >= 1000)
                dimBuf[idx++] = '0' + (btnX / 1000) % 10;
            if (btnX >= 100)
                dimBuf[idx++] = '0' + (btnX / 100) % 10;
            if (btnX >= 10)
                dimBuf[idx++] = '0' + (btnX / 10) % 10;
            dimBuf[idx++] = '0' + btnX % 10;
            dimBuf[idx++] = ',';
            dimBuf[idx++] = '1';
            dimBuf[idx++] = '0';
            dimBuf[idx++] = '-';
            dimBuf[idx++] = '4';
            dimBuf[idx++] = '0';
            dimBuf[idx++] = '\r';
            dimBuf[idx++] = '\n';
            dimBuf[idx] = 0;
            serialPrint(dimBuf);

            // Track previous button state for generating down/up events
            static bool prevLeftBtn = false;
            static bool prevRightBtn = false;

            // Set up mouse callback using the new driver system
            if (mouseDriver)
            {
                mouseDriver->setCallback([](const QKDrv::MouseReport &report)
                                         {
                    auto *mouse = QKDrv::Manager::instance().mouseDriver();
                    if (!mouse) return;
                    
                    auto &eventMgr = QK::Event::EventManager::instance();
                    
                    // Post mouse move event
                    eventMgr.postMouseMove(mouse->x(), mouse->y(), report.x, report.y);
                    
                    bool leftBtn = report.buttons & 0x01;
                    bool rightBtn = report.buttons & 0x02;
                    
                    // Check for button state changes
                    if (leftBtn && !prevLeftBtn) {
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonDown, 
                                                 QK::Event::MouseButton::Left,
                                                 mouse->x(), mouse->y(), QK::Event::Modifiers::None);
                        serialPrint("Left click!\r\n");
                    }
                    if (!leftBtn && prevLeftBtn) {
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonUp,
                                                 QK::Event::MouseButton::Left,
                                                 mouse->x(), mouse->y(), QK::Event::Modifiers::None);
                    }
                    if (rightBtn && !prevRightBtn) {
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonDown,
                                                 QK::Event::MouseButton::Right,
                                                 mouse->x(), mouse->y(), QK::Event::Modifiers::None);
                    }
                    if (!rightBtn && prevRightBtn) {
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonUp,
                                                 QK::Event::MouseButton::Right,
                                                 mouse->x(), mouse->y(), QK::Event::Modifiers::None);
                    }
                    
                    prevLeftBtn = leftBtn;
                    prevRightBtn = rightBtn; });
            }
            serialPrint("Mouse configured\r\n");

            // Set up keyboard callback using the new driver system
            serialPrint("Setting up keyboard...\r\n");
            auto &ps2Keyboard = QKDrv::PS2::Keyboard::instance();
            ps2Keyboard.setPS2Callback([](const QKDrv::PS2::KeyEvent &evt)
                                       {
                serialPrint("Key event: ");
                if (evt.pressed) serialPrint("DOWN ");
                else serialPrint("UP ");
                if (evt.ctrl) serialPrint("CTRL+");
                // Print character if printable
                if (evt.character >= 32 && evt.character < 127) {
                    char buf[2] = {evt.character, 0};
                    serialPrint(buf);
                }
                serialPrint("\r\n");
                
                auto &eventMgr = QK::Event::EventManager::instance();
                
                QK::Event::Modifiers mods = QK::Event::Modifiers::None;
                if (evt.shift) mods = mods | QK::Event::Modifiers::Shift;
                if (evt.ctrl) mods = mods | QK::Event::Modifiers::Ctrl;
                if (evt.alt) mods = mods | QK::Event::Modifiers::Alt;
                
                eventMgr.postKeyEvent(
                    evt.pressed ? QK::Event::Type::KeyDown : QK::Event::Type::KeyUp,
                    static_cast<QC::u8>(evt.key),  // scancode
                    static_cast<QC::u8>(evt.key),  // keycode
                    evt.character,
                    mods,
                    false  // isRepeat
                ); });
            serialPrint("Keyboard initialized\r\n");

            // Create desktop window (fills the screen)
            serialPrint("Creating desktop window...\r\n");
            QW::Window *desktop = QW::WindowManager::instance().createWindow(
                "Desktop",
                QW::Rect{0, 0, width, height});

            if (desktop == nullptr)
            {
                serialPrint("ERROR: Failed to create desktop window!\r\n");
            }
            else
            {
                serialPrint("Setting window flags...\r\n");
                desktop->setFlags(QW::WindowFlags::Visible); // No decorations for desktop
                serialPrint("Clearing window...\r\n");
                desktop->clear(QW::Color(0, 128, 128, 255)); // Teal background
                serialPrint("Desktop window created\r\n");

                // Create shutdown button (TEST - remove later)
                serialPrint("Creating shutdown button...\r\n");
                QW::Rect shutdownBtnRect{static_cast<QC::i32>(width) - 120, 10, 100, 30};

                // Draw button directly on window
                desktop->fillRect(shutdownBtnRect, QW::Color(200, 60, 60, 255)); // Red background
                // Draw simple border
                for (QC::u32 bx = 0; bx < shutdownBtnRect.width; ++bx)
                {
                    desktop->setPixel(shutdownBtnRect.x + bx, shutdownBtnRect.y, QW::Color(100, 30, 30, 255));
                    desktop->setPixel(shutdownBtnRect.x + bx, shutdownBtnRect.y + shutdownBtnRect.height - 1, QW::Color(100, 30, 30, 255));
                }
                for (QC::u32 by = 0; by < shutdownBtnRect.height; ++by)
                {
                    desktop->setPixel(shutdownBtnRect.x, shutdownBtnRect.y + by, QW::Color(100, 30, 30, 255));
                    desktop->setPixel(shutdownBtnRect.x + shutdownBtnRect.width - 1, shutdownBtnRect.y + by, QW::Color(100, 30, 30, 255));
                }
                serialPrint("Shutdown button drawn\r\n");
            }

            // Initial render
            QW::WindowManager::instance().render();
            serialPrint("Initial render complete!\r\n");

            // Shutdown button bounds for click detection (static to persist for lambda)
            static QW::Rect shutdownBtnRect{static_cast<QC::i32>(width) - 120, 10, 100, 30};

            // Register event listener for mouse clicks on shutdown button
            QK::Event::EventListener shutdownListener;
            shutdownListener.categoryMask = QK::Event::Category::Input;
            shutdownListener.eventType = QK::Event::Type::MouseButtonDown;
            shutdownListener.handler = [](const QK::Event::Event &event, void *userData) -> bool
            {
                auto *btnRect = static_cast<QW::Rect *>(userData);
                const auto &mouse = event.asMouse();

                // Check if click is inside button bounds
                if (mouse.button == QK::Event::MouseButton::Left)
                {
                    serialPrint("Click at (");
                    // Simple coordinate print
                    char buf[32];
                    int idx = 0;
                    int x = mouse.x, y = mouse.y;
                    if (x >= 100)
                    {
                        buf[idx++] = '0' + (x / 100) % 10;
                    }
                    if (x >= 10)
                    {
                        buf[idx++] = '0' + (x / 10) % 10;
                    }
                    buf[idx++] = '0' + x % 10;
                    buf[idx++] = ',';
                    if (y >= 100)
                    {
                        buf[idx++] = '0' + (y / 100) % 10;
                    }
                    if (y >= 10)
                    {
                        buf[idx++] = '0' + (y / 10) % 10;
                    }
                    buf[idx++] = '0' + y % 10;
                    buf[idx++] = ')';
                    buf[idx++] = '\r';
                    buf[idx++] = '\n';
                    buf[idx] = 0;
                    serialPrint(buf);

                    bool inside = mouse.x >= btnRect->x &&
                                  mouse.x < btnRect->x + static_cast<QC::i32>(btnRect->width) &&
                                  mouse.y >= btnRect->y &&
                                  mouse.y < btnRect->y + static_cast<QC::i32>(btnRect->height);
                    if (inside)
                    {
                        serialPrint("Shutdown button clicked!\r\n");
                        // QEMU/Bochs ACPI shutdown: write to port 0x604
                        QC::outw(0x604, 0x2000); // ACPI S5 (power off)
                        // Fallback: halt
                        QC::cli();
                        for (;;)
                        {
                            QC::halt();
                        }
                        return true;
                    }
                }
                return false;
            };
            shutdownListener.userData = &shutdownBtnRect;
            QK::Event::ListenerId shutdownId = QK::Event::EventManager::instance().addListener(shutdownListener);
            if (shutdownId == QK::Event::InvalidListenerId)
            {
                serialPrint("ERROR: Failed to register shutdown listener!\r\n");
            }
            else
            {
                serialPrint("Shutdown button listener registered\r\n");
            }

            // Register keyboard listener for Ctrl+Q shutdown
            QK::Event::EventListener ctrlQListener;
            ctrlQListener.categoryMask = QK::Event::Category::Input;
            ctrlQListener.eventType = QK::Event::Type::KeyDown;
            ctrlQListener.handler = [](const QK::Event::Event &event, void *) -> bool
            {
                serialPrint("KeyDown listener called!\r\n");
                const auto &key = event.asKey();

                // Check for Q key with Ctrl modifier
                if (key.keycode == static_cast<QC::u8>(QKDrv::PS2::Key::Q) &&
                    QK::Event::hasModifier(key.modifiers, QK::Event::Modifiers::Ctrl))
                {
                    serialPrint("Ctrl+Q pressed - shutting down!\r\n");
                    // QEMU/Bochs ACPI shutdown
                    QC::outw(0x604, 0x2000);
                    QC::cli();
                    for (;;)
                    {
                        QC::halt();
                    }
                    return true;
                }
                return false;
            };
            ctrlQListener.userData = nullptr;
            QK::Event::ListenerId ctrlQId = QK::Event::EventManager::instance().addListener(ctrlQListener);
            if (ctrlQId == QK::Event::InvalidListenerId)
            {
                serialPrint("ERROR: Failed to register Ctrl+Q listener!\r\n");
            }
            else
            {
                serialPrint("Ctrl+Q shutdown listener registered\r\n");
            }

            // Main loop - process events and render
            serialPrint("Entering main loop...\r\n");

            while (true)
            {
                // Poll all active drivers
                QKDrv::Manager::instance().poll();

                // Process pending events
                QC::usize processed = QK::Event::EventManager::instance().processEvents();
                if (processed > 0)
                {
                    serialPrint("Processed events: ");
                    char buf[16];
                    buf[0] = '0' + (processed % 10);
                    buf[1] = '\r';
                    buf[2] = '\n';
                    buf[3] = 0;
                    serialPrint(buf);
                }

                // Render (compositor draws cursor from WindowManager mouse position)
                QW::WindowManager::instance().render();

                // Halt until next interrupt
                asm volatile("hlt");
            }
        }
        else
        {
            serialPrint("No framebuffers available!\r\n");
        }
    }
    else
    {
        serialPrint("No framebuffer response!\r\n");
    }

    // Halt forever (fallback if no framebuffer)
    serialPrint("Halting...\r\n");
    for (;;)
    {
        asm volatile("hlt");
    }

#if 0
    // Initialize physical memory manager (skipped for now)
    // TODO: Parse memory map from boot info properly
    QC::usize totalMemory = 128 * 1024 * 1024; // Assume 128MB for now
    QK::Memory::MemoryRegion regions[2] = {
        {reinterpret_cast<QC::u64>(_kernel_start),
         reinterpret_cast<QC::uptr>(_kernel_end) - reinterpret_cast<QC::uptr>(_kernel_start),
         QK::Memory::MemoryRegion::Type::Kernel},
        {reinterpret_cast<QC::u64>(_kernel_end),
         totalMemory - reinterpret_cast<QC::uptr>(_kernel_end),
         QK::Memory::MemoryRegion::Type::Available}};
    QK::Memory::PMM::instance().initialize(regions, 2);
    serialPrint("PMM initialized\r\n");

    // Initialize virtual memory
    QK::Memory::VMM::instance().initialize();
    serialPrint("VMM initialized\r\n");

    // Initialize heap - allocate pages from PMM for heap
    constexpr QC::usize HEAP_SIZE = 8 * 1024 * 1024; // 8MB heap
    QC::PhysAddr heapPhys = QK::Memory::PMM::instance().allocatePages(HEAP_SIZE / QK::Memory::PAGE_SIZE);
    QC::VirtAddr heapBase = reinterpret_cast<QC::VirtAddr>(heapPhys); // Identity mapped for now
    QK::Memory::Heap::instance().initialize(heapBase, HEAP_SIZE);
    serialPrint("Heap initialized\r\n");

    // Call C++ constructors
    callConstructors();
    serialPrint("Constructors called\r\n");

    // Initialize logger (now that heap is available)
    QC::Logger::instance().setLevel(QC::LogLevel::Debug);
    QC_LOG_INFO("Kernel", "QAIOS Kernel v0.1.0");
    QC_LOG_INFO("Kernel", "Kernel loaded at 0x%llx - 0x%llx",
                reinterpret_cast<QC::u64>(_kernel_start),
                reinterpret_cast<QC::u64>(_kernel_end));

    // Initialize PCI
    QArch::PCI::instance().initialize();

    // Initialize timer
    QDrv::Timer::instance().initialize(1000); // 1000 Hz

    // Initialize driver manager (probes USB and PS/2)
    QKDrv::Manager::instance().initialize();

    // Enable interrupts
    asm volatile("sti");
    QC_LOG_INFO("Kernel", "Interrupts enabled");

    // Initialize kernel
    QK::Kernel::instance().initialize();

    QC_LOG_INFO("Kernel", "Kernel initialization complete");
    QC_LOG_INFO("Kernel", "Entering main loop");

    // Main kernel loop
    QK::Kernel::instance().run();

    // Should never reach here
    kernel_panic("Kernel main loop exited unexpectedly");
#endif
}
