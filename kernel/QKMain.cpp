// QAIOS Kernel Main Entry Point
// QKMain.cpp

#include "QCTypes.h"
#include "QCLogger.h"
#include "QCBuiltins.h"
#include "QCString.h"
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
#include "QKShutdownController.h"
#include "QWControls/Leaf/Button.h"
#include "QDDesktop.h"
#define LIMINE_API_REVISION 2
#include "limine.h"
#include "QFSVFS.h"
#include "QFSFAT32.h"
#include "QFSFile.h"

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
    extern QC::u64 limine_hhdm_request[];
    extern QC::u64 limine_kernel_address_request[];
    extern QC::u64 limine_module_request[];
    extern QC::u64 limine_terminal_request[];
}

// Forward declaration (initBootTerminal() uses serialPrint()).
static void serialPrint(const char *msg);

// Forward declaration (initializeRamdiskFilesystem() calls fileIoDemo()).
static void fileIoDemo();

static limine_terminal *g_bootTerm = nullptr;
static limine_terminal_write g_bootTermWrite = nullptr;

static bool initBootTerminal()
{
    auto *response = reinterpret_cast<limine_terminal_response *>(limine_terminal_request[5]);
    if (!response)
    {
        serialPrint("Limine terminal: no response\r\n");
        return false;
    }

    if (!response->write)
    {
        serialPrint("Limine terminal: no write function\r\n");
        return false;
    }

    if (response->terminal_count == 0 || !response->terminals)
    {
        serialPrint("Limine terminal: no terminals\r\n");
        return false;
    }

    g_bootTerm = response->terminals[0];
    g_bootTermWrite = response->write;
    serialPrint("Limine terminal: ready\r\n");
    return true;
}

static void bootTermPrint(const char *msg)
{
    if (g_bootTerm && g_bootTermWrite)
    {
        g_bootTermWrite(g_bootTerm, msg, QC::String::strlen(msg));
    }
}
// Global HHDM offset (physical to virtual mapping)
static QC::u64 g_hhdmOffset = 0;

// Kernel address mapping from Limine
static QC::u64 g_kernelPhysBase = 0;
static QC::u64 g_kernelVirtBase = 0;

// Get HHDM offset for physical-to-virtual translation
QC::u64 getHHDMOffset()
{
    return g_hhdmOffset;
}

// Convert physical address to virtual address (for RAM, via HHDM)
extern "C" QC::VirtAddr physToVirt(QC::PhysAddr phys)
{
    return static_cast<QC::VirtAddr>(phys + g_hhdmOffset);
}

// Convert kernel virtual address to physical address
extern "C" QC::PhysAddr kernelVirtToPhys(QC::VirtAddr virt)
{
    return static_cast<QC::PhysAddr>(virt - g_kernelVirtBase + g_kernelPhysBase);
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

// Simple page allocator for early USB - returns PHYSICAL address
extern "C" QC::PhysAddr earlyAllocatePage()
{
    if (earlyDMAOffset + 4096 > sizeof(earlyDMABuffer))
        return 0;
    QC::VirtAddr virt = reinterpret_cast<QC::VirtAddr>(&earlyDMABuffer[earlyDMAOffset]);
    earlyDMAOffset += 4096;
    return kernelVirtToPhys(virt);
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
    // Mirror to Limine boot terminal (if present). Safe even before initBootTerminal().
    bootTermPrint(msg);

    while (*msg)
    {
        // Wait for transmit buffer empty
        while ((QC::inb(0x3F8 + 5) & 0x20) == 0)
            ;
        QC::outb(0x3F8, *msg);
        ++msg;
    }
}

static void serialPrintInt(QC::i32 value)
{
    char buffer[16];
    int pos = 0;
    bool negative = value < 0;
    QC::u32 magnitude = negative ? static_cast<QC::u32>(-value) : static_cast<QC::u32>(value);

    do
    {
        buffer[pos++] = static_cast<char>('0' + (magnitude % 10));
        magnitude /= 10;
    } while (magnitude != 0 && pos < static_cast<int>(sizeof(buffer)) - 1);

    if (negative && pos < static_cast<int>(sizeof(buffer)) - 1)
    {
        buffer[pos++] = '-';
    }

    buffer[pos] = '\0';

    for (int i = 0; i < pos / 2; ++i)
    {
        char tmp = buffer[i];
        buffer[i] = buffer[pos - 1 - i];
        buffer[pos - 1 - i] = tmp;
    }

    serialPrint(buffer);
}

class RamDiskBlockDevice : public QFS::BlockDevice
{
public:
    RamDiskBlockDevice(QC::u8 *base, QC::u64 size, QC::usize sectorSize)
        : m_base(base), m_size(size), m_sectorSize(sectorSize)
    {
    }

    QC::usize sectorSize() const override { return m_sectorSize; }
    QC::u64 sectorCount() const override { return m_size / m_sectorSize; }

    QC::Status readSector(QC::u64 sector, void *buffer) override
    {
        return readSectors(sector, 1, buffer);
    }

    QC::Status writeSector(QC::u64 sector, const void *buffer) override
    {
        return writeSectors(sector, 1, buffer);
    }

    QC::Status readSectors(QC::u64 sector, QC::usize count, void *buffer) override
    {
        if (!buffer)
            return QC::Status::InvalidParam;

        QC::u64 offset = sector * m_sectorSize;
        QC::u64 bytes = static_cast<QC::u64>(count) * m_sectorSize;
        if (offset + bytes > m_size)
            return QC::Status::InvalidParam;

        QC::String::memcpy(buffer, m_base + offset, static_cast<QC::usize>(bytes));
        return QC::Status::Success;
    }

    QC::Status writeSectors(QC::u64 sector, QC::usize count, const void *buffer) override
    {
        if (!buffer)
            return QC::Status::InvalidParam;

        QC::u64 offset = sector * m_sectorSize;
        QC::u64 bytes = static_cast<QC::u64>(count) * m_sectorSize;
        if (offset + bytes > m_size)
            return QC::Status::InvalidParam;

        QC::String::memcpy(m_base + offset, buffer, static_cast<QC::usize>(bytes));
        return QC::Status::Success;
    }

private:
    QC::u8 *m_base;
    QC::u64 m_size;
    QC::usize m_sectorSize;
};

static RamDiskBlockDevice *g_ramdiskDevice = nullptr;
static QFS::FAT32 *g_ramdiskFs = nullptr;
static QFS::VFS *g_vfs = nullptr;
static constexpr QC::usize kRamdiskSectorSize = 512;

static const limine_module_response *moduleResponse()
{
    return reinterpret_cast<const limine_module_response *>(limine_module_request[5]);
}

static const limine_file *findRamdiskModule()
{
    const limine_module_response *response = moduleResponse();
    if (!response || response->module_count == 0 || !response->modules)
    {
        return nullptr;
    }

    const limine_file *fallback = nullptr;
    for (QC::u64 i = 0; i < response->module_count; ++i)
    {
        const limine_file *candidate = response->modules[i];
        if (!candidate)
            continue;

        if (candidate->cmdline && QC::String::strcmp(candidate->cmdline, "ramdisk") == 0)
        {
            return candidate;
        }

        if (!fallback)
        {
            fallback = candidate;
        }
    }

    return fallback;
}

static bool ensureVfsReady()
{
    if (g_vfs)
        return true;

    g_vfs = &QFS::VFS::instance();
    g_vfs->initialize();
    serialPrint("VFS initialized\r\n");
    return true;
}

static bool initializeRamdiskFilesystem()
{
    if (g_ramdiskFs)
        return true;

    if (!ensureVfsReady())
        return false;

    const limine_file *ramdisk = findRamdiskModule();
    if (!ramdisk)
    {
        serialPrint("No ramdisk module provided by Limine\r\n");
        return false;
    }

    auto *base = reinterpret_cast<QC::u8 *>(ramdisk->address);
    QC::u64 size = ramdisk->size;

    if (!base || size == 0)
    {
        serialPrint("Ramdisk module is empty or null\r\n");
        return false;
    }

    g_ramdiskDevice = new RamDiskBlockDevice(base, size, kRamdiskSectorSize);
    g_ramdiskFs = new QFS::FAT32(g_ramdiskDevice);

    QC::Status status = g_vfs->mount("/", g_ramdiskFs);
    if (status != QC::Status::Success)
    {
        serialPrint("Failed to mount ramdisk filesystem\r\n");
        return false;
    }

    serialPrint("Ramdisk mounted at /\r\n");
    fileIoDemo();
    return true;
}

static void readHelloFileDemo()
{
    if (!g_vfs)
        return;

    QFS::File *file = g_vfs->open("/HELLO.TXT", QFS::OpenMode::Read);
    if (!file)
    {
        serialPrint("Failed to open /HELLO.TXT\r\n");
        return;
    }

    char buffer[256];
    QC::isize bytesRead = file->read(buffer, sizeof(buffer) - 1);
    if (bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        serialPrint("/HELLO.TXT contents: ");
        serialPrint(buffer);
        serialPrint("\r\n");
    }
    else
    {
        serialPrint("Read returned no data for /HELLO.TXT\r\n");
    }

    g_vfs->close(file);
}

static void fileIoDemo()
{
    if (!g_vfs)
        return;

    serialPrint("Root dir listing:\r\n");
    QFS::Directory *dir = g_vfs->openDir("/");
    if (dir)
    {
        QFS::DirEntry entry;
        while (dir->read(&entry))
        {
            serialPrint("  ");
            serialPrint(entry.name);
            serialPrint("\r\n");
        }
        g_vfs->closeDir(dir);
    }

    const char *path = "/QFSDEMO.TXT";
    QFS::File *out = g_vfs->open(path, QFS::OpenMode::Write | QFS::OpenMode::Create | QFS::OpenMode::Truncate);
    if (!out)
    {
        serialPrint("Failed to create demo file\r\n");
        return;
    }

    const char *msg = "QAIOS+ FileIO demo\n";
    out->write(msg, QC::String::strlen(msg));
    g_vfs->close(out);

    QFS::File *in = g_vfs->open(path, QFS::OpenMode::Read);
    if (!in)
    {
        serialPrint("Failed to open demo file for read\r\n");
        return;
    }

    char buffer[64];
    QC::isize bytes = in->read(buffer, sizeof(buffer) - 1);
    if (bytes > 0)
    {
        buffer[bytes] = '\0';
        serialPrint("Demo file contents: ");
        serialPrint(buffer);
        serialPrint("\r\n");
    }
    g_vfs->close(in);
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
    if (initBootTerminal())
    {
        bootTermPrint("Boot terminal initialized\r\n");
    }
    // Get HHDM offset from Limine (needed for MMIO mapping)
    QC::u64 *hhdm_response = reinterpret_cast<QC::u64 *>(limine_hhdm_request[5]);
    if (hhdm_response != nullptr)
    {
        // HHDM response: [0] = revision, [1] = offset
        g_hhdmOffset = hhdm_response[1];
        serialPrint("HHDM offset: 0x");
        // Print a simplified hex
        char hexbuf[17];
        for (int i = 15; i >= 0; --i)
        {
            int nibble = (g_hhdmOffset >> (i * 4)) & 0xF;
            hexbuf[15 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
        }
        hexbuf[16] = 0;
        serialPrint(hexbuf);
        serialPrint("\r\n");
    }
    else
    {
        serialPrint("WARNING: No HHDM response from Limine!\r\n");
    }

    // Get kernel address info from Limine (needed for virt-to-phys conversion)
    QC::u64 *kaddr_response = reinterpret_cast<QC::u64 *>(limine_kernel_address_request[5]);
    if (kaddr_response != nullptr)
    {
        // Kernel address response: [0] = revision, [1] = physical_base, [2] = virtual_base
        g_kernelPhysBase = kaddr_response[1];
        g_kernelVirtBase = kaddr_response[2];
        serialPrint("Kernel phys base: 0x");
        char hexbuf[17];
        for (int i = 15; i >= 0; --i)
        {
            int nibble = (g_kernelPhysBase >> (i * 4)) & 0xF;
            hexbuf[15 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
        }
        hexbuf[16] = 0;
        serialPrint(hexbuf);
        serialPrint("\r\nKernel virt base: 0x");
        for (int i = 15; i >= 0; --i)
        {
            int nibble = (g_kernelVirtBase >> (i * 4)) & 0xF;
            hexbuf[15 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
        }
        hexbuf[16] = 0;
        serialPrint(hexbuf);
        serialPrint("\r\n");
    }
    else
    {
        serialPrint("WARNING: No kernel_address response from Limine!\r\n");
    }

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
        char hexbuf[3] = {static_cast<char>('0' + (revision % 10)), '\r', '\n'};
        serialPrint(hexbuf);

        serialPrint("  Count: ");
        hexbuf[0] = static_cast<char>('0' + (fb_count % 10));
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

            serialPrint("Bringing up filesystem...\r\n");
            if (initializeRamdiskFilesystem())
            {
                serialPrint("Filesystem ready\r\n");
                readHelloFileDemo();
            }
            else
            {
                serialPrint("Filesystem initialization failed\r\n");
            }

            // Initialize the event system
            QK::Event::EventManager::instance().initialize();
            serialPrint("Event system initialized\r\n");

            // Bring up shutdown controller early so it can register event listeners
            QK::Shutdown::Controller::instance();
            serialPrint("Shutdown controller ready\r\n");

            // Initialize timer (100 Hz tick for main loop)
            serialPrint("Initializing timer...\r\n");
            QDrv::Timer::instance().initialize(100);
            serialPrint("Timer initialized\r\n");

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
                    auto logClick = [&](const char *label)
                    {
                        serialPrint(label);
                        serialPrint(" at (");
                        serialPrintInt(mouse->x());
                        serialPrint(", ");
                        serialPrintInt(mouse->y());
                        serialPrint(") delta (");
                        serialPrintInt(report.x);
                        serialPrint(", ");
                        serialPrintInt(report.y);
                        serialPrint(")\r\n");
                    };
                    
                    // Post mouse move event
                    eventMgr.postMouseMove(mouse->x(), mouse->y(), report.x, report.y);
                    
                    bool leftBtn = report.buttons & 0x01;
                    bool rightBtn = report.buttons & 0x02;
                    
                    // Check for button state changes
                    if (leftBtn && !prevLeftBtn) {
                        logClick("Left click");
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonDown, 
                                                 QK::Event::MouseButton::Left,
                                                 mouse->x(), mouse->y(), QK::Event::Modifiers::None);
                    }
                    if (!leftBtn && prevLeftBtn) {
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonUp,
                                                 QK::Event::MouseButton::Left,
                                                 mouse->x(), mouse->y(), QK::Event::Modifiers::None);
                    }
                    if (rightBtn && !prevRightBtn) {
                        logClick("Right click");
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

            // Create desktop using QDDesktop module
            serialPrint("Creating desktop...\r\n");
            static QD::Desktop desktop;
            desktop.initialize(width, height);
            serialPrint("Desktop initialized\r\n");

            // Paint desktop controls into the desktop window
            desktop.paint();

            // Initial render
            QW::WindowManager::instance().render();
            serialPrint("Initial render complete!\r\n");

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
                    serialPrint("Ctrl+Q pressed - requesting shutdown!\r\n");
                    QK::Event::EventManager::instance().postShutdownEvent(
                        QK::Event::Type::ShutdownRequest,
                        static_cast<QC::u32>(QK::Shutdown::Reason::KeyboardShortcut));
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

                // Also explicitly poll keyboard (debug)
                QKDrv::PS2::Keyboard::instance().poll();

                // Process pending events
                QK::Event::EventManager::instance().processEvents();

                // Repaint desktop and render
                desktop.paint();
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
