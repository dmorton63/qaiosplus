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
#include "QDrvVmwareSVGA.h"
#include "QKDrvManager.h"
#include "IDE/QKDrvIDE.h"
#include "PS2/QKDrvPS2Keyboard.h"
#include "PS2/QKDrvPS2Mouse.h"
#include "QWFramebuffer.h"
#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QKEventManager.h"
#include "QKEventListener.h"
#include "QKShutdownController.h"
#include "QKEntropy.h"
#include "QKSecureStore.h"
#include "QKSecurityCenter.h"
#include "QWControls/Leaf/Button.h"
#include "QDDesktop.h"
#define LIMINE_API_REVISION 2
#include "limine.h"
#include "QFSVFS.h"
#include "QFSFAT32.h"
#include "QFSFile.h"
#include "QFSVolumeManager.h"
#include "QKStorageRegistry.h"
#include "QKStorageProbe.h"
#include "QKMemoryBlockDevice.h"
#include "QKConsole.h"

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
    extern QC::u64 limine_firmware_type_request[];
    extern QC::u64 limine_rsdp_request[];
}

// Forward declaration (initBootTerminal() uses serialPrint()).
static void serialPrint(const char *msg);

// Forward declaration (initializeRamdiskFilesystem() calls fileIoDemo()).
static void fileIoDemo();

// Forward declaration (initializeRamdiskFilesystem() runs a SecureStore self-test).
static void secureStoreSelfTest();

// Forward declaration (initializeRamdiskFilesystem() may run a PCR mismatch negative test).
static void secureStorePcrMismatchTest();

LIMINE_DEPRECATED_IGNORE_START
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
LIMINE_DEPRECATED_IGNORE_END
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

static QKStorage::MemoryBlockDevice *g_ramdiskDevice = nullptr;

namespace
{

#pragma pack(push, 1)
    struct AcpiRsdp
    {
        char signature[8];
        QC::u8 checksum;
        char oemId[6];
        QC::u8 revision;
        QC::u32 rsdtAddress;
        QC::u32 length;
        QC::u64 xsdtAddress;
        QC::u8 extendedChecksum;
        QC::u8 reserved[3];
    };

    struct AcpiSdtHeader
    {
        char signature[4];
        QC::u32 length;
        QC::u8 revision;
        QC::u8 checksum;
        char oemId[6];
        char oemTableId[8];
        QC::u32 oemRevision;
        QC::u32 creatorId;
        QC::u32 creatorRevision;
    };

    struct AcpiTpm2TableBase
    {
        AcpiSdtHeader header;
        QC::u16 platformClass;
        QC::u16 reserved;
        QC::u64 controlArea;
        QC::u32 startMethod;
        QC::u8 startMethodParameters[12];
    };
#pragma pack(pop)

    static void printHex64(const char *label, QC::u64 value)
    {
        serialPrint(label);
        serialPrint("0x");
        char hexbuf[17];
        for (int i = 15; i >= 0; --i)
        {
            int nibble = (value >> (i * 4)) & 0xF;
            hexbuf[15 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
        }
        hexbuf[16] = 0;
        serialPrint(hexbuf);
        serialPrint("\r\n");
    }

    static void printDecU32(const char *label, QC::u32 value)
    {
        serialPrint(label);
        serialPrintInt(static_cast<QC::i32>(value));
        serialPrint("\r\n");
    }

    static void printHex32Fixed(QC::u32 value)
    {
        char hexbuf[9];
        for (int i = 7; i >= 0; --i)
        {
            int nibble = (value >> (i * 4)) & 0xF;
            hexbuf[7 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
        }
        hexbuf[8] = 0;
        serialPrint(hexbuf);
    }

    static void printHex8Fixed(QC::u8 value)
    {
        const QC::u8 hi = (value >> 4) & 0xF;
        const QC::u8 lo = value & 0xF;
        char buf[3];
        buf[0] = hi < 10 ? static_cast<char>('0' + hi) : static_cast<char>('a' + (hi - 10));
        buf[1] = lo < 10 ? static_cast<char>('0' + lo) : static_cast<char>('a' + (lo - 10));
        buf[2] = 0;
        serialPrint(buf);
    }

    static QC::u16 readBe16(const QC::u8 *p)
    {
        return static_cast<QC::u16>((static_cast<QC::u16>(p[0]) << 8) | static_cast<QC::u16>(p[1]));
    }

    static QC::u32 readBe32(const QC::u8 *p)
    {
        return (static_cast<QC::u32>(p[0]) << 24) |
               (static_cast<QC::u32>(p[1]) << 16) |
               (static_cast<QC::u32>(p[2]) << 8) |
               static_cast<QC::u32>(p[3]);
    }

    static bool ensureHhdmMappedWithFlags(QC::PhysAddr phys, QC::usize size, QK::Memory::PageFlags flags)
    {
        if (phys == 0 || size == 0)
            return false;

        constexpr QC::usize kPageSize = 4096;
        QC::PhysAddr start = phys & ~(static_cast<QC::PhysAddr>(kPageSize - 1));
        QC::PhysAddr end = (phys + size + (kPageSize - 1)) & ~(static_cast<QC::PhysAddr>(kPageSize - 1));

        for (QC::PhysAddr p = start; p < end; p += kPageSize)
        {
            QC::VirtAddr v = physToVirt(p);
            if (!QK::Memory::VMM::instance().isMapped(v))
            {
                QC::Status status = QK::Memory::VMM::instance().map(v, p, flags);
                if (status != QC::Status::Success)
                {
                    serialPrint("ACPI: failed to map physical page\r\n");
                    return false;
                }
            }
        }

        return true;
    }

    static const char *firmwareTypeToString(QC::u64 t)
    {
        switch (t)
        {
        case LIMINE_FIRMWARE_TYPE_X86BIOS:
            return "x86 BIOS";
        case LIMINE_FIRMWARE_TYPE_UEFI32:
            return "UEFI32";
        case LIMINE_FIRMWARE_TYPE_UEFI64:
            return "UEFI64";
        case LIMINE_FIRMWARE_TYPE_SBI:
            return "SBI";
        default:
            return "UNKNOWN";
        }
    }

    static bool ensureHhdmMapped(QC::PhysAddr phys, QC::usize size)
    {
        auto flags = QK::Memory::PageFlags::Present | QK::Memory::PageFlags::Writable | QK::Memory::PageFlags::NoExecute;
        return ensureHhdmMappedWithFlags(phys, size, flags);
    }

    static bool ensureHhdmMappedMmio(QC::PhysAddr phys, QC::usize size)
    {
        auto flags = QK::Memory::PageFlags::Present | QK::Memory::PageFlags::Writable |
                     QK::Memory::PageFlags::NoExecute | QK::Memory::PageFlags::NoCache |
                     QK::Memory::PageFlags::WriteThrough;
        return ensureHhdmMappedWithFlags(phys, size, flags);
    }

    static bool spinWaitClears32(QC::VirtAddr addr, QC::u32 mask, QC::usize iterations)
    {
        for (QC::usize i = 0; i < iterations; ++i)
        {
            if ((QC::mmio_read32(addr) & mask) == 0)
                return true;
            QC::pause();
        }
        return false;
    }

    static bool spinWaitSet32(QC::VirtAddr addr, QC::u32 mask, QC::usize iterations)
    {
        for (QC::usize i = 0; i < iterations; ++i)
        {
            if ((QC::mmio_read32(addr) & mask) != 0)
                return true;
            QC::pause();
        }
        return false;
    }

    struct CrbCtx
    {
        QC::VirtAddr base;
        QC::usize off;

        QC::VirtAddr reg(QC::usize r) const
        {
            return base + off + r;
        }
    };

    struct TpmSecureStoreCtx
    {
        bool ready = false;
        CrbCtx ctx{};
    };

    static TpmSecureStoreCtx g_tpmSecureStore;
    static QC::u32 g_tpmLastRspCode = 0;

    static QC::u32 crbSubmitQuiet(const CrbCtx &ctx,
                                  const QC::u8 *cmd,
                                  QC::usize cmdLen,
                                  QC::u32 &rspLenOut,
                                  QC::PhysAddr &rspPhysOut)
    {
        constexpr QC::usize CTRL_REQ = 0x00;
        constexpr QC::usize CTRL_CANCEL = 0x08;
        constexpr QC::usize CTRL_START = 0x0C;
        constexpr QC::usize CMD_SIZE = 0x18;
        constexpr QC::usize CMD_PA_LOW = 0x1C;
        constexpr QC::usize CMD_PA_HIGH = 0x20;
        constexpr QC::usize RSP_SIZE = 0x24;
        constexpr QC::usize RSP_PA = 0x28;

        rspLenOut = 0;
        rspPhysOut = 0;

        QC::mmio_write32(ctx.reg(CTRL_REQ), QC::mmio_read32(ctx.reg(CTRL_REQ)) | 1u);
        if (!spinWaitClears32(ctx.reg(CTRL_REQ), 1u, 5'000'000))
        {
            return 0xFFFFFFFFu;
        }

        QC::u32 cmdSize = QC::mmio_read32(ctx.reg(CMD_SIZE));
        QC::u32 cmdLow = QC::mmio_read32(ctx.reg(CMD_PA_LOW));
        QC::u32 cmdHigh = QC::mmio_read32(ctx.reg(CMD_PA_HIGH));
        QC::u64 cmdPhys64 = (static_cast<QC::u64>(cmdHigh) << 32) | cmdLow;

        QC::u32 rspSize = QC::mmio_read32(ctx.reg(RSP_SIZE));
        QC::u64 rspPhys64 = QC::mmio_read64(ctx.reg(RSP_PA));

        if (cmdPhys64 == 0 || rspPhys64 == 0)
        {
            return 0xFFFFFFFFu;
        }

        if (cmdLen > cmdSize || cmdSize < 12 || rspSize < 10)
        {
            return 0xFFFFFFFFu;
        }

        QC::PhysAddr cmdPhys = static_cast<QC::PhysAddr>(cmdPhys64);
        QC::PhysAddr rspPhys = static_cast<QC::PhysAddr>(rspPhys64);

        if (!ensureHhdmMapped(cmdPhys, cmdSize) || !ensureHhdmMapped(rspPhys, rspSize))
        {
            return 0xFFFFFFFFu;
        }

        auto *cmdBuf = reinterpret_cast<volatile QC::u8 *>(physToVirt(cmdPhys));
        for (QC::usize i = 0; i < cmdLen; ++i)
        {
            cmdBuf[i] = cmd[i];
        }

        QC::write_barrier();
        QC::memory_barrier();

        QC::mmio_write32(ctx.reg(CTRL_START), 1u);
        if (!spinWaitClears32(ctx.reg(CTRL_START), 1u, 50'000'000))
        {
            QC::mmio_write32(ctx.reg(CTRL_CANCEL), 1u);
            (void)spinWaitClears32(ctx.reg(CTRL_START), 1u, 5'000'000);
            return 0xFFFFFFFFu;
        }

        QC::read_barrier();
        QC::memory_barrier();

        auto *rspBuf = reinterpret_cast<const QC::u8 *>(physToVirt(rspPhys));
        QC::u32 rspLen = readBe32(rspBuf + 2);
        QC::u32 rspCode = readBe32(rspBuf + 6);

        rspLenOut = rspLen;
        rspPhysOut = rspPhys;

        QC::mmio_write32(ctx.reg(CTRL_REQ), QC::mmio_read32(ctx.reg(CTRL_REQ)) | 2u);
        (void)spinWaitClears32(ctx.reg(CTRL_REQ), 2u, 5'000'000);

        return rspCode;
    }

    static QC::u16 readBe16Local(const QC::u8 *p)
    {
        return static_cast<QC::u16>((static_cast<QC::u16>(p[0]) << 8) | static_cast<QC::u16>(p[1]));
    }

    static void writeBe16Local(QC::u8 *p, QC::u16 v)
    {
        p[0] = static_cast<QC::u8>((v >> 8) & 0xFF);
        p[1] = static_cast<QC::u8>(v & 0xFF);
    }

    static void writeBe32Local(QC::u8 *p, QC::u32 v)
    {
        p[0] = static_cast<QC::u8>((v >> 24) & 0xFF);
        p[1] = static_cast<QC::u8>((v >> 16) & 0xFF);
        p[2] = static_cast<QC::u8>((v >> 8) & 0xFF);
        p[3] = static_cast<QC::u8>(v & 0xFF);
    }

    struct TpmBufWriter
    {
        QC::u8 *buf;
        QC::usize cap;
        QC::usize len;

        bool push(const void *data, QC::usize n)
        {
            if (!data || len + n > cap)
                return false;
            const QC::u8 *p = static_cast<const QC::u8 *>(data);
            for (QC::usize i = 0; i < n; ++i)
                buf[len + i] = p[i];
            len += n;
            return true;
        }

        bool u8(QC::u8 v)
        {
            return push(&v, 1);
        }

        bool be16(QC::u16 v)
        {
            QC::u8 tmp[2];
            writeBe16Local(tmp, v);
            return push(tmp, 2);
        }

        bool be32(QC::u32 v)
        {
            QC::u8 tmp[4];
            writeBe32Local(tmp, v);
            return push(tmp, 4);
        }
    };

    static bool tpmRspParams(const QC::u8 *rsp, QC::u32 rspLen, const QC::u8 *&params, QC::u32 &paramsLen)
    {
        params = nullptr;
        paramsLen = 0;
        if (!rsp || rspLen < 10)
            return false;

        const QC::u16 tag = readBe16Local(rsp);
        if (tag == 0x8001)
        {
            params = rsp + 10;
            paramsLen = (rspLen >= 10) ? (rspLen - 10) : 0;
            return true;
        }

        if (tag != 0x8002)
            return false;

        // Expected layout for TPM_ST_SESSIONS response:
        // header(10) + parameterSize(4) + parameters(parameterSize) + authArea(...)
        if (rspLen >= 14)
        {
            const QC::u32 parameterSize = readBe32(rsp + 10);
            if (14u + parameterSize <= rspLen)
            {
                params = rsp + 14;
                paramsLen = parameterSize;
                return true;
            }
        }

        // Fallback: treat the remainder as parameters.
        if (rspLen < 10)
            return false;
        params = rsp + 10;
        paramsLen = rspLen - 10;
        return true;
    }

    static bool tpmAppendSessionAuthHandle(TpmBufWriter &w, QC::u32 sessionHandle);

    static bool tpmAppendPwSessionAuth(TpmBufWriter &w)
    {
        // TPM_RS_PW session with empty password.
        return tpmAppendSessionAuthHandle(w, 0x40000009u);
    }

    static bool tpmAppendSessionAuthHandle(TpmBufWriter &w, QC::u32 sessionHandle)
    {
        // sessionHandle (4) + nonce size (2=0) + sessionAttributes (1=0) + hmac size (2=0)
        if (!w.be32(sessionHandle))
            return false;
        if (!w.be16(0))
            return false;
        if (!w.u8(0))
            return false;
        if (!w.be16(0))
            return false;
        return true;
    }

    static QC::Status tpmStartPolicySession(const CrbCtx &ctx, bool trial, QC::u32 &outSession)
    {
        // TPM2_StartAuthSession
        QC::u8 cmd[128];
        TpmBufWriter w{cmd, sizeof(cmd), 0};

        constexpr QC::u16 TPM_ST_NO_SESSIONS = 0x8001;
        constexpr QC::u32 TPM_CC_StartAuthSession = 0x00000176u;
        constexpr QC::u32 TPM_RH_NULL = 0x40000007u;
        constexpr QC::u16 TPM_ALG_NULL = 0x0010;
        constexpr QC::u16 TPM_ALG_SHA256 = 0x000B;

        if (!w.be16(TPM_ST_NO_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_StartAuthSession))
            return QC::Status::Error;

        // handles: tpmKey, bind
        if (!w.be32(TPM_RH_NULL) || !w.be32(TPM_RH_NULL))
            return QC::Status::Error;

        // nonceCaller: TPM2B_NONCE
        // Some TPM implementations reject a zero-length nonce for StartAuthSession.
        QC::u8 nonceCaller[16];
        for (QC::usize i = 0; i < sizeof(nonceCaller); ++i)
            nonceCaller[i] = static_cast<QC::u8>(0xA5u ^ static_cast<QC::u8>(i));
        if (!w.be16(static_cast<QC::u16>(sizeof(nonceCaller))) || !w.push(nonceCaller, sizeof(nonceCaller)))
            return QC::Status::Error;
        // encryptedSalt: empty TPM2B_ENCRYPTED_SECRET
        if (!w.be16(0))
            return QC::Status::Error;
        // sessionType
        if (!w.u8(trial ? static_cast<QC::u8>(0x03) : static_cast<QC::u8>(0x01)))
            return QC::Status::Error;
        // symmetric: TPMT_SYM_DEF (algorithm = NULL)
        if (!w.be16(TPM_ALG_NULL))
            return QC::Status::Error;
        // authHash
        if (!w.be16(TPM_ALG_SHA256))
            return QC::Status::Error;

        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        if (!ensureHhdmMapped(rspPhys, rspLen))
            return QC::Status::Error;

        const QC::u8 *rsp = reinterpret_cast<const QC::u8 *>(physToVirt(rspPhys));
        const QC::u8 *params = nullptr;
        QC::u32 paramsLen = 0;
        if (!tpmRspParams(rsp, rspLen, params, paramsLen))
            return QC::Status::Error;
        if (!params || paramsLen < 4)
            return QC::Status::Error;

        outSession = readBe32(params);
        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status tpmPolicyPCR(const CrbCtx &ctx, QC::u32 policySession)
    {
        // TPM2_PolicyPCR
        QC::u8 cmd[96];
        TpmBufWriter w{cmd, sizeof(cmd), 0};

        constexpr QC::u16 TPM_ST_NO_SESSIONS = 0x8001;
        constexpr QC::u32 TPM_CC_PolicyPCR = 0x0000017Fu;
        constexpr QC::u16 TPM_ALG_SHA256 = 0x000B;

        if (!w.be16(TPM_ST_NO_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_PolicyPCR))
            return QC::Status::Error;
        if (!w.be32(policySession))
            return QC::Status::Error;

        // pcrDigest: empty TPM2B_DIGEST (size=0) => bind policy to current PCR state
        if (!w.be16(0))
            return QC::Status::Error;

        // pcrs: TPML_PCR_SELECTION (count=1), select PCR 7 in SHA256 bank
        if (!w.be32(1))
            return QC::Status::Error;
        if (!w.be16(TPM_ALG_SHA256))
            return QC::Status::Error;
        if (!w.u8(3))
            return QC::Status::Error;
        // pcrSelect[3]
        if (!w.u8(0x80) || !w.u8(0x00) || !w.u8(0x00))
            return QC::Status::Error;

        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status tpmPcrExtendSha256(const CrbCtx &ctx, QC::u32 pcrIndex, const QC::u8 digest[32])
    {
        // TPM2_PCR_Extend
        QC::u8 cmd[128];
        TpmBufWriter w{cmd, sizeof(cmd), 0};

        constexpr QC::u16 TPM_ST_SESSIONS = 0x8002;
        constexpr QC::u32 TPM_CC_PCR_Extend = 0x00000182u;
        constexpr QC::u16 TPM_ALG_SHA256 = 0x000B;

        // PCR handles are 0x00000000.. for PCR 0.. (TPM2.0)
        const QC::u32 pcrHandle = pcrIndex;

        if (!w.be16(TPM_ST_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_PCR_Extend))
            return QC::Status::Error;
        if (!w.be32(pcrHandle))
            return QC::Status::Error;

        // authorizationSize + authorizationArea (empty password)
        if (!w.be32(9))
            return QC::Status::Error;
        if (!tpmAppendPwSessionAuth(w))
            return QC::Status::Error;

        // digests: TPML_DIGEST_VALUES(count=1, alg=SHA256, digest[32])
        if (!w.be32(1))
            return QC::Status::Error;
        if (!w.be16(TPM_ALG_SHA256))
            return QC::Status::Error;
        if (!w.push(digest, 32))
            return QC::Status::Error;

        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status tpmPolicyGetDigest(const CrbCtx &ctx, QC::u32 policySession, QC::u8 outDigest[32])
    {
        // TPM2_PolicyGetDigest
        QC::u8 cmd[64];
        TpmBufWriter w{cmd, sizeof(cmd), 0};

        constexpr QC::u16 TPM_ST_NO_SESSIONS = 0x8001;
        constexpr QC::u32 TPM_CC_PolicyGetDigest = 0x00000189u;

        if (!w.be16(TPM_ST_NO_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_PolicyGetDigest))
            return QC::Status::Error;
        if (!w.be32(policySession))
            return QC::Status::Error;

        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        if (!ensureHhdmMapped(rspPhys, rspLen))
            return QC::Status::Error;

        const QC::u8 *rsp = reinterpret_cast<const QC::u8 *>(physToVirt(rspPhys));
        const QC::u8 *params = nullptr;
        QC::u32 paramsLen = 0;
        if (!tpmRspParams(rsp, rspLen, params, paramsLen))
            return QC::Status::Error;
        if (!params || paramsLen < 2)
            return QC::Status::Error;

        const QC::u16 sz = readBe16Local(params);
        if (sz != 32)
            return QC::Status::Error;
        if (2u + sz > paramsLen)
            return QC::Status::Error;
        for (int i = 0; i < 32; ++i)
            outDigest[i] = params[2 + i];

        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status tpmCreatePrimaryStorageKey(const CrbCtx &ctx, QC::u32 &outHandle)
    {
        // Minimal SRK template (RSA 2048, AES-128-CFB inner wrapper).
        QC::u8 cmd[512];
        TpmBufWriter w{cmd, sizeof(cmd), 0};

        constexpr QC::u16 TPM_ST_SESSIONS = 0x8002;
        constexpr QC::u32 TPM_CC_CreatePrimary = 0x00000131u;
        constexpr QC::u32 TPM_RH_OWNER = 0x40000001u;

        // Header placeholder.
        if (!w.be16(TPM_ST_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_CreatePrimary))
            return QC::Status::Error;

        // Handles.
        if (!w.be32(TPM_RH_OWNER))
            return QC::Status::Error;

        // authorizationSize + authorizationArea (single password session).
        if (!w.be32(9))
            return QC::Status::Error;
        if (!tpmAppendPwSessionAuth(w))
            return QC::Status::Error;

        // inSensitive: TPM2B_SENSITIVE_CREATE with empty auth + empty data
        if (!w.be16(4) || !w.be16(0) || !w.be16(0))
            return QC::Status::Error;

        // inPublic: TPM2B_PUBLIC
        const QC::usize inPublicSizeOffset = w.len;
        if (!w.be16(0))
            return QC::Status::Error;
        const QC::usize inPublicStart = w.len;

        // TPMT_PUBLIC
        constexpr QC::u16 TPM_ALG_RSA = 0x0001;
        constexpr QC::u16 TPM_ALG_SHA256 = 0x000B;
        constexpr QC::u16 TPM_ALG_AES = 0x0006;
        constexpr QC::u16 TPM_ALG_CFB = 0x0043;
        constexpr QC::u16 TPM_ALG_NULL = 0x0010;

        constexpr QC::u32 TPMA_FIXEDTPM = 0x00000002u;
        constexpr QC::u32 TPMA_FIXEDPARENT = 0x00000010u;
        constexpr QC::u32 TPMA_SENSITIVEDATAORIGIN = 0x00000020u;
        constexpr QC::u32 TPMA_USERWITHAUTH = 0x00000040u;
        constexpr QC::u32 TPMA_NODA = 0x00000400u;
        constexpr QC::u32 TPMA_RESTRICTED = 0x00010000u;
        constexpr QC::u32 TPMA_DECRYPT = 0x00020000u;

        const QC::u32 objectAttributes = TPMA_FIXEDTPM | TPMA_FIXEDPARENT | TPMA_SENSITIVEDATAORIGIN |
                                         TPMA_USERWITHAUTH | TPMA_NODA | TPMA_RESTRICTED | TPMA_DECRYPT;

        if (!w.be16(TPM_ALG_RSA) || !w.be16(TPM_ALG_SHA256) || !w.be32(objectAttributes))
            return QC::Status::Error;

        // authPolicy: empty
        if (!w.be16(0))
            return QC::Status::Error;

        // parameters: TPMT_RSA_PARMS
        // symmetric: TPMT_SYM_DEF_OBJECT (AES 128 CFB)
        if (!w.be16(TPM_ALG_AES) || !w.be16(128) || !w.be16(TPM_ALG_CFB))
            return QC::Status::Error;
        // scheme: NULL
        if (!w.be16(TPM_ALG_NULL))
            return QC::Status::Error;
        // keyBits, exponent
        if (!w.be16(2048) || !w.be32(0))
            return QC::Status::Error;
        // unique: empty
        if (!w.be16(0))
            return QC::Status::Error;

        const QC::u16 inPublicSize = static_cast<QC::u16>(w.len - inPublicStart);
        writeBe16Local(cmd + inPublicSizeOffset, inPublicSize);

        // outsideInfo: empty TPM2B_DATA
        if (!w.be16(0))
            return QC::Status::Error;

        // creationPCR: TPML_PCR_SELECTION count=0
        if (!w.be32(0))
            return QC::Status::Error;

        // Patch command size.
        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        if (rspLen < 14)
            return QC::Status::Error;
        if (!ensureHhdmMapped(rspPhys, rspLen))
            return QC::Status::Error;

        const QC::u8 *rsp = reinterpret_cast<const QC::u8 *>(physToVirt(rspPhys));
        const QC::u8 *params = nullptr;
        QC::u32 paramsLen = 0;
        if (!tpmRspParams(rsp, rspLen, params, paramsLen))
            return QC::Status::Error;
        if (!params || paramsLen < 4)
            return QC::Status::Error;

        outHandle = readBe32(params);
        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status tpmFlushContext(const CrbCtx &ctx, QC::u32 handle)
    {
        QC::u8 cmd[64];
        TpmBufWriter w{cmd, sizeof(cmd), 0};
        constexpr QC::u16 TPM_ST_NO_SESSIONS = 0x8001;
        constexpr QC::u32 TPM_CC_FlushContext = 0x00000165u;

        if (!w.be16(TPM_ST_NO_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_FlushContext))
            return QC::Status::Error;
        if (!w.be32(handle))
            return QC::Status::Error;
        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status tpmCreateSealedObject(const CrbCtx &ctx,
                                            QC::u32 parentHandle,
                                            const QC::u8 *secret,
                                            QC::usize secretLen,
                                            const QC::u8 policyDigest[32],
                                            QC::Vector<QC::u8> &outPrivate2b,
                                            QC::Vector<QC::u8> &outPublic2b)
    {
        if (!secret || secretLen == 0 || secretLen > 64)
            return QC::Status::InvalidParam;

        QC::u8 cmd[768];
        TpmBufWriter w{cmd, sizeof(cmd), 0};
        constexpr QC::u16 TPM_ST_SESSIONS = 0x8002;
        constexpr QC::u32 TPM_CC_Create = 0x00000153u;
        constexpr QC::u16 TPM_ALG_KEYEDHASH = 0x0008;
        constexpr QC::u16 TPM_ALG_SHA256 = 0x000B;
        constexpr QC::u16 TPM_ALG_NULL = 0x0010;

        constexpr QC::u32 TPMA_FIXEDTPM = 0x00000002u;
        constexpr QC::u32 TPMA_FIXEDPARENT = 0x00000010u;
        constexpr QC::u32 TPMA_ADMINWITHPOLICY = 0x00000080u;
        constexpr QC::u32 TPMA_NODA = 0x00000400u;
        const QC::u32 objectAttributes = TPMA_FIXEDTPM | TPMA_FIXEDPARENT | TPMA_ADMINWITHPOLICY | TPMA_NODA;

        if (!w.be16(TPM_ST_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_Create))
            return QC::Status::Error;
        if (!w.be32(parentHandle))
            return QC::Status::Error;

        // authorizationSize + authorizationArea first.
        if (!w.be32(9))
            return QC::Status::Error;
        if (!tpmAppendPwSessionAuth(w))
            return QC::Status::Error;

        // inSensitive: TPM2B_SENSITIVE_CREATE with empty auth + data=secret
        const QC::u16 sensInnerSize = static_cast<QC::u16>(2 + 0 + 2 + secretLen);
        if (!w.be16(sensInnerSize))
            return QC::Status::Error;
        if (!w.be16(0))
            return QC::Status::Error;
        if (!w.be16(static_cast<QC::u16>(secretLen)))
            return QC::Status::Error;
        if (!w.push(secret, secretLen))
            return QC::Status::Error;

        // inPublic: TPM2B_PUBLIC
        const QC::usize inPublicSizeOffset = w.len;
        if (!w.be16(0))
            return QC::Status::Error;
        const QC::usize inPublicStart = w.len;

        if (!w.be16(TPM_ALG_KEYEDHASH) || !w.be16(TPM_ALG_SHA256) || !w.be32(objectAttributes))
            return QC::Status::Error;
        // authPolicy: SHA256 policy digest (bind to PCR state)
        if (!w.be16(32))
            return QC::Status::Error;
        if (!w.push(policyDigest, 32))
            return QC::Status::Error;
        // parameters: TPMT_KEYEDHASH_PARMS -> scheme NULL
        if (!w.be16(TPM_ALG_NULL))
            return QC::Status::Error;
        // unique: empty digest
        if (!w.be16(0))
            return QC::Status::Error;

        const QC::u16 inPublicSize = static_cast<QC::u16>(w.len - inPublicStart);
        writeBe16Local(cmd + inPublicSizeOffset, inPublicSize);

        // outsideInfo: empty
        if (!w.be16(0))
            return QC::Status::Error;
        // creationPCR: count=0
        if (!w.be32(0))
            return QC::Status::Error;

        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        if (!ensureHhdmMapped(rspPhys, rspLen))
            return QC::Status::Error;
        const QC::u8 *rsp = reinterpret_cast<const QC::u8 *>(physToVirt(rspPhys));

        const QC::u8 *params = nullptr;
        QC::u32 paramsLen = 0;
        if (!tpmRspParams(rsp, rspLen, params, paramsLen))
            return QC::Status::Error;
        if (!params || paramsLen < 4)
            return QC::Status::Error;

        // outPrivate TPM2B_PRIVATE, outPublic TPM2B_PUBLIC
        QC::usize off = 0;
        const QC::u16 privSize = readBe16Local(params + off);
        off += 2;
        if (off + privSize > paramsLen)
            return QC::Status::Error;
        outPrivate2b.clear();
        outPrivate2b.resize(static_cast<QC::usize>(2 + privSize));
        // copy size field + blob
        outPrivate2b[0] = params[0];
        outPrivate2b[1] = params[1];
        for (QC::usize i = 0; i < privSize; ++i)
            outPrivate2b[2 + i] = params[off + i];
        off += privSize;

        if (off + 2 > paramsLen)
            return QC::Status::Error;
        const QC::u16 pubSize = readBe16Local(params + off);
        off += 2;
        if (off + pubSize > paramsLen)
            return QC::Status::Error;
        outPublic2b.clear();
        outPublic2b.resize(static_cast<QC::usize>(2 + pubSize));
        outPublic2b[0] = params[off - 2];
        outPublic2b[1] = params[off - 1];
        for (QC::usize i = 0; i < pubSize; ++i)
            outPublic2b[2 + i] = params[off + i];
        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status tpmLoadSealedObject(const CrbCtx &ctx,
                                          QC::u32 parentHandle,
                                          const QC::Vector<QC::u8> &inPrivate2b,
                                          const QC::Vector<QC::u8> &inPublic2b,
                                          QC::u32 &outHandle)
    {
        if (inPrivate2b.size() < 2 || inPublic2b.size() < 2)
            return QC::Status::InvalidParam;

        QC::u8 cmd[1024];
        TpmBufWriter w{cmd, sizeof(cmd), 0};
        constexpr QC::u16 TPM_ST_SESSIONS = 0x8002;
        constexpr QC::u32 TPM_CC_Load = 0x00000157u;

        if (!w.be16(TPM_ST_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_Load))
            return QC::Status::Error;
        if (!w.be32(parentHandle))
            return QC::Status::Error;
        if (!w.be32(9))
            return QC::Status::Error;
        if (!tpmAppendPwSessionAuth(w))
            return QC::Status::Error;

        if (!w.push(inPrivate2b.data(), inPrivate2b.size()))
            return QC::Status::Error;
        if (!w.push(inPublic2b.data(), inPublic2b.size()))
            return QC::Status::Error;

        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        if (!ensureHhdmMapped(rspPhys, rspLen))
            return QC::Status::Error;
        const QC::u8 *rsp = reinterpret_cast<const QC::u8 *>(physToVirt(rspPhys));
        const QC::u8 *params = nullptr;
        QC::u32 paramsLen = 0;
        if (!tpmRspParams(rsp, rspLen, params, paramsLen))
            return QC::Status::Error;
        if (!params || paramsLen < 4)
            return QC::Status::Error;
        outHandle = readBe32(params);
        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status tpmUnsealWithAuthSession(const CrbCtx &ctx,
                                               QC::u32 objectHandle,
                                               QC::u32 authSessionHandle,
                                               QC::u8 *out,
                                               QC::usize outLen)
    {
        if (!out || outLen == 0)
            return QC::Status::InvalidParam;

        QC::u8 cmd[128];
        TpmBufWriter w{cmd, sizeof(cmd), 0};
        constexpr QC::u16 TPM_ST_SESSIONS = 0x8002;
        constexpr QC::u32 TPM_CC_Unseal = 0x0000015Eu;

        if (!w.be16(TPM_ST_SESSIONS) || !w.be32(0) || !w.be32(TPM_CC_Unseal))
            return QC::Status::Error;
        if (!w.be32(objectHandle))
            return QC::Status::Error;
        if (!w.be32(9))
            return QC::Status::Error;
        if (!tpmAppendSessionAuthHandle(w, authSessionHandle))
            return QC::Status::Error;
        writeBe32Local(cmd + 2, static_cast<QC::u32>(w.len));

        QC::u32 rspLen = 0;
        QC::PhysAddr rspPhys = 0;
        QC::u32 rspCode = crbSubmitQuiet(ctx, cmd, w.len, rspLen, rspPhys);
        g_tpmLastRspCode = rspCode;
        if (rspCode != 0)
            return QC::Status::Error;
        if (!ensureHhdmMapped(rspPhys, rspLen))
            return QC::Status::Error;
        const QC::u8 *rsp = reinterpret_cast<const QC::u8 *>(physToVirt(rspPhys));
        const QC::u8 *params = nullptr;
        QC::u32 paramsLen = 0;
        if (!tpmRspParams(rsp, rspLen, params, paramsLen))
            return QC::Status::Error;
        if (!params || paramsLen < 2)
            return QC::Status::Error;

        const QC::u16 sz = readBe16Local(params);
        if (static_cast<QC::usize>(sz) != outLen)
            return QC::Status::Error;
        if (2u + sz > paramsLen)
            return QC::Status::Error;
        for (QC::usize i = 0; i < outLen; ++i)
            out[i] = params[2 + i];
        g_tpmLastRspCode = 0;
        return QC::Status::Success;
    }

    static QC::Status secureStoreTpmSealWrapKey(void *user,
                                                const QC::u8 *wrapKey,
                                                QC::usize wrapKeyLen,
                                                QC::Vector<QC::u8> &outBlob)
    {
        if (!user || !wrapKey || wrapKeyLen != 32)
            return QC::Status::InvalidParam;

        auto *dev = static_cast<TpmSecureStoreCtx *>(user);
        if (!dev->ready)
            return QC::Status::Busy;

        // Build a PCR policy digest (PCR7, SHA256 bank).
        // Use a POLICY session here; some TPM implementations reject PolicyGetDigest on TRIAL sessions.
        QC::u32 policyDigestSession = 0;
        QC::Status st = tpmStartPolicySession(dev->ctx, false, policyDigestSession);
        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: StartAuthSession(POLICY-DIGEST) failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            return st;
        }

        st = tpmPolicyPCR(dev->ctx, policyDigestSession);
        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: PolicyPCR(POLICY-DIGEST) failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            (void)tpmFlushContext(dev->ctx, policyDigestSession);
            return st;
        }

        QC::u8 policyDigest[32];
        st = tpmPolicyGetDigest(dev->ctx, policyDigestSession, policyDigest);
        (void)tpmFlushContext(dev->ctx, policyDigestSession);
        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: PolicyGetDigest failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            return st;
        }

        QC::u32 primary = 0;
        st = tpmCreatePrimaryStorageKey(dev->ctx, primary);
        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: CreatePrimary failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            return st;
        }

        QC::Vector<QC::u8> priv2b;
        QC::Vector<QC::u8> pub2b;
        st = tpmCreateSealedObject(dev->ctx, primary, wrapKey, wrapKeyLen, policyDigest, priv2b, pub2b);

        (void)tpmFlushContext(dev->ctx, primary);

        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: Create failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            if (priv2b.size() > 0)
                QC::String::memset(priv2b.data(), 0, priv2b.size());
            if (pub2b.size() > 0)
                QC::String::memset(pub2b.data(), 0, pub2b.size());
            priv2b.clear();
            pub2b.clear();
            return st;
        }

        // Blob format: 'W''K''T''1' + verLE32(1) + privLenLE32 + pubLenLE32 + priv2b + pub2b
        const QC::u32 ver = 1;
        const QC::u32 privLen = static_cast<QC::u32>(priv2b.size());
        const QC::u32 pubLen = static_cast<QC::u32>(pub2b.size());
        outBlob.clear();
        outBlob.resize(4 + 4 + 4 + 4 + priv2b.size() + pub2b.size());

        auto putLe32 = [](QC::u8 *p, QC::u32 v)
        {
            p[0] = static_cast<QC::u8>(v & 0xFF);
            p[1] = static_cast<QC::u8>((v >> 8) & 0xFF);
            p[2] = static_cast<QC::u8>((v >> 16) & 0xFF);
            p[3] = static_cast<QC::u8>((v >> 24) & 0xFF);
        };

        outBlob[0] = 'W';
        outBlob[1] = 'K';
        outBlob[2] = 'T';
        outBlob[3] = '1';
        putLe32(outBlob.data() + 4, ver);
        putLe32(outBlob.data() + 8, privLen);
        putLe32(outBlob.data() + 12, pubLen);

        QC::usize o = 16;
        for (QC::usize i = 0; i < priv2b.size(); ++i)
            outBlob[o++] = priv2b[i];
        for (QC::usize i = 0; i < pub2b.size(); ++i)
            outBlob[o++] = pub2b[i];

        // Wipe temporaries.
        QC::String::memset(priv2b.data(), 0, priv2b.size());
        QC::String::memset(pub2b.data(), 0, pub2b.size());
        priv2b.clear();
        pub2b.clear();
        return QC::Status::Success;
    }

    static QC::Status secureStoreTpmUnsealWrapKey(void *user,
                                                  const QC::Vector<QC::u8> &blob,
                                                  QC::u8 *outWrapKey,
                                                  QC::usize outWrapKeyLen)
    {
        if (!user || !outWrapKey || outWrapKeyLen != 32)
            return QC::Status::InvalidParam;

        auto *dev = static_cast<TpmSecureStoreCtx *>(user);
        if (!dev->ready)
            return QC::Status::Busy;

        // Start a policy session and satisfy PolicyPCR for the current boot state.
        QC::u32 policySession = 0;
        QC::Status st = tpmStartPolicySession(dev->ctx, false, policySession);
        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: StartAuthSession(POLICY) failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            return st;
        }

        st = tpmPolicyPCR(dev->ctx, policySession);
        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: PolicyPCR failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            (void)tpmFlushContext(dev->ctx, policySession);
            return st;
        }

        if (blob.size() < 16)
            return QC::Status::Error;
        if (!(blob[0] == 'W' && blob[1] == 'K' && blob[2] == 'T' && blob[3] == '1'))
            return QC::Status::Error;

        auto getLe32 = [](const QC::u8 *p) -> QC::u32
        {
            return static_cast<QC::u32>(p[0]) |
                   (static_cast<QC::u32>(p[1]) << 8) |
                   (static_cast<QC::u32>(p[2]) << 16) |
                   (static_cast<QC::u32>(p[3]) << 24);
        };

        const QC::u32 ver = getLe32(blob.data() + 4);
        if (ver != 1)
            return QC::Status::Error;
        const QC::u32 privLen = getLe32(blob.data() + 8);
        const QC::u32 pubLen = getLe32(blob.data() + 12);
        const QC::usize total = 16u + static_cast<QC::usize>(privLen) + static_cast<QC::usize>(pubLen);
        if (total != blob.size())
            return QC::Status::Error;

        QC::Vector<QC::u8> priv2b;
        QC::Vector<QC::u8> pub2b;
        priv2b.resize(privLen);
        pub2b.resize(pubLen);

        QC::usize o = 16;
        for (QC::usize i = 0; i < priv2b.size(); ++i)
            priv2b[i] = blob[o++];
        for (QC::usize i = 0; i < pub2b.size(); ++i)
            pub2b[i] = blob[o++];

        QC::u32 primary = 0;
        st = tpmCreatePrimaryStorageKey(dev->ctx, primary);
        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: CreatePrimary failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            QC::String::memset(priv2b.data(), 0, priv2b.size());
            QC::String::memset(pub2b.data(), 0, pub2b.size());
            priv2b.clear();
            pub2b.clear();
            return st;
        }

        QC::u32 obj = 0;
        st = tpmLoadSealedObject(dev->ctx, primary, priv2b, pub2b, obj);

        QC::String::memset(priv2b.data(), 0, priv2b.size());
        QC::String::memset(pub2b.data(), 0, pub2b.size());
        priv2b.clear();
        pub2b.clear();

        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: Load failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
            (void)tpmFlushContext(dev->ctx, primary);
            (void)tpmFlushContext(dev->ctx, policySession);
            return st;
        }

        st = tpmUnsealWithAuthSession(dev->ctx, obj, policySession, outWrapKey, outWrapKeyLen);
        if (st != QC::Status::Success)
        {
            serialPrint("SecureStoreTPM: Unseal failed (rsp=0x");
            printHex32Fixed(g_tpmLastRspCode);
            serialPrint(")\r\n");
        }
        (void)tpmFlushContext(dev->ctx, obj);
        (void)tpmFlushContext(dev->ctx, primary);
        (void)tpmFlushContext(dev->ctx, policySession);
        return st;
    }

    static QC::u32 crbSubmit(const CrbCtx &ctx,
                             const QC::u8 *cmd,
                             QC::usize cmdLen,
                             QC::u32 &rspLenOut,
                             QC::PhysAddr &rspPhysOut)
    {
        // NOTE: ACPI TPM2 controlArea typically points at the CRB control area,
        // which is PTP register base + 0x40. Therefore, offsets below are
        // relative to controlArea (i.e. original PTP offsets minus 0x40).
        constexpr QC::usize CTRL_REQ = 0x00;
        constexpr QC::usize CTRL_STS = 0x04;
        constexpr QC::usize CTRL_CANCEL = 0x08;
        constexpr QC::usize CTRL_START = 0x0C;
        constexpr QC::usize CMD_SIZE = 0x18;
        constexpr QC::usize CMD_PA_LOW = 0x1C;
        constexpr QC::usize CMD_PA_HIGH = 0x20;
        constexpr QC::usize RSP_SIZE = 0x24;
        constexpr QC::usize RSP_PA = 0x28;

        rspLenOut = 0;
        rspPhysOut = 0;

        QC::mmio_write32(ctx.reg(CTRL_REQ), QC::mmio_read32(ctx.reg(CTRL_REQ)) | 1u);
        if (!spinWaitClears32(ctx.reg(CTRL_REQ), 1u, 5'000'000))
        {
            serialPrint("TPM2: CMD_READY timeout\r\n");
            return 0xFFFFFFFFu;
        }

        QC::u32 cmdSize = QC::mmio_read32(ctx.reg(CMD_SIZE));
        QC::u32 cmdLow = QC::mmio_read32(ctx.reg(CMD_PA_LOW));
        QC::u32 cmdHigh = QC::mmio_read32(ctx.reg(CMD_PA_HIGH));
        QC::u64 cmdPhys64 = (static_cast<QC::u64>(cmdHigh) << 32) | cmdLow;

        QC::u32 rspSize = QC::mmio_read32(ctx.reg(RSP_SIZE));
        QC::u64 rspPhys64 = QC::mmio_read64(ctx.reg(RSP_PA));

        printHex64("TPM2: cmdBuf phys ", cmdPhys64);
        printDecU32("TPM2: cmdBuf size ", cmdSize);
        printHex64("TPM2: rspBuf phys ", rspPhys64);
        printDecU32("TPM2: rspBuf size ", rspSize);

        if (cmdPhys64 == 0 || rspPhys64 == 0)
        {
            serialPrint("TPM2: invalid CRB buffer address\r\n");
            return 0xFFFFFFFFu;
        }

        if (cmdLen > cmdSize || cmdSize < 12 || rspSize < 10)
        {
            serialPrint("TPM2: invalid CRB buffer sizes\r\n");
            return 0xFFFFFFFFu;
        }

        QC::PhysAddr cmdPhys = static_cast<QC::PhysAddr>(cmdPhys64);
        QC::PhysAddr rspPhys = static_cast<QC::PhysAddr>(rspPhys64);

        if (!ensureHhdmMapped(cmdPhys, cmdSize) || !ensureHhdmMapped(rspPhys, rspSize))
        {
            serialPrint("TPM2: failed to map cmd/rsp buffers\r\n");
            return 0xFFFFFFFFu;
        }

        auto *cmdBuf = reinterpret_cast<volatile QC::u8 *>(physToVirt(cmdPhys));
        for (QC::usize i = 0; i < cmdLen; ++i)
        {
            cmdBuf[i] = cmd[i];
        }

        QC::write_barrier();
        QC::memory_barrier();

        QC::mmio_write32(ctx.reg(CTRL_START), 1u);
        if (!spinWaitClears32(ctx.reg(CTRL_START), 1u, 50'000'000))
        {
            serialPrint("TPM2: START timeout; issuing CANCEL\r\n");
            QC::mmio_write32(ctx.reg(CTRL_CANCEL), 1u);
            (void)spinWaitClears32(ctx.reg(CTRL_START), 1u, 5'000'000);
            return 0xFFFFFFFFu;
        }

        QC::u32 sts = QC::mmio_read32(ctx.reg(CTRL_STS));
        serialPrint("TPM2: CTRL_STS 0x");
        printHex32Fixed(sts);
        serialPrint("\r\n");

        QC::read_barrier();
        QC::memory_barrier();

        auto *rspBuf = reinterpret_cast<const QC::u8 *>(physToVirt(rspPhys));
        QC::u32 rspLen = readBe32(rspBuf + 2);
        QC::u32 rspCode = readBe32(rspBuf + 6);

        rspLenOut = rspLen;
        rspPhysOut = rspPhys;

        printDecU32("TPM2: rspLen ", rspLen);
        serialPrint("TPM2: rspCode 0x");
        printHex32Fixed(rspCode);
        serialPrint("\r\n");

        QC::mmio_write32(ctx.reg(CTRL_REQ), QC::mmio_read32(ctx.reg(CTRL_REQ)) | 2u);
        (void)spinWaitClears32(ctx.reg(CTRL_REQ), 2u, 5'000'000);

        return rspCode;
    }

    static void tryTpm2CrbStartup(QC::u32 startMethod, QC::PhysAddr controlAreaPhys)
    {
        if (controlAreaPhys == 0)
        {
            serialPrint("TPM2: no control area\r\n");
            return;
        }

        if (!(startMethod == 6 || startMethod == 7))
        {
            serialPrint("TPM2: start method not CRB-style; skipping TPM commands\r\n");
            return;
        }

        QC::PhysAddr pagePhys = controlAreaPhys & ~static_cast<QC::PhysAddr>(0xFFF);
        if (!ensureHhdmMappedMmio(pagePhys, 0x1000))
        {
            serialPrint("TPM2: failed to map control area\r\n");
            return;
        }

        CrbCtx ctx{.base = physToVirt(pagePhys), .off = static_cast<QC::usize>(controlAreaPhys & 0xFFF)};

        serialPrint("TPM2: attempting TPM2_Startup via CRB\r\n");
        const QC::u8 startupCmd[12] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00};
        QC::u32 startupRspLen = 0;
        QC::PhysAddr startupRspPhys = 0;
        QC::u32 startupRspCode = crbSubmit(ctx, startupCmd, sizeof(startupCmd), startupRspLen, startupRspPhys);
        if (startupRspCode == 0xFFFFFFFFu)
        {
            serialPrint("TPM2: Startup transport failed\r\n");
            return;
        }

        if (startupRspCode == 0x00000100)
        {
            serialPrint("TPM2: TPM_RC_INITIALIZE (already started)\r\n");
        }
        else if (startupRspCode != 0)
        {
            serialPrint("TPM2: Startup failed\r\n");
            return;
        }
        else
        {
            serialPrint("TPM2: Startup OK\r\n");
        }

        serialPrint("TPM2: attempting TPM2_GetRandom(16)\r\n");
        const QC::u8 getRandomCmd[12] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x01, 0x7B, 0x00, 0x10};
        QC::u32 randRspLen = 0;
        QC::PhysAddr randRspPhys = 0;
        QC::u32 randRspCode = crbSubmit(ctx, getRandomCmd, sizeof(getRandomCmd), randRspLen, randRspPhys);
        if (randRspCode != 0)
        {
            serialPrint("TPM2: GetRandom failed\r\n");
            return;
        }

        if (randRspLen < 12)
        {
            serialPrint("TPM2: GetRandom response too short\r\n");
            return;
        }

        if (!ensureHhdmMapped(randRspPhys, randRspLen))
        {
            serialPrint("TPM2: failed to map GetRandom response\r\n");
            return;
        }

        auto *rspBuf = reinterpret_cast<const QC::u8 *>(physToVirt(randRspPhys));
        const QC::u16 bytesSize = readBe16(rspBuf + 10);
        serialPrint("TPM2: GetRandom bytes ");
        serialPrintInt(bytesSize);
        serialPrint("\r\n");

        const QC::usize avail = (randRspLen > 12) ? (randRspLen - 12) : 0;
        const QC::usize toDump = (bytesSize < avail) ? bytesSize : avail;

        serialPrint("TPM2: RAND ");
        for (QC::usize i = 0; i < toDump; ++i)
        {
            printHex8Fixed(rspBuf[12 + i]);
        }
        serialPrint("\r\n");

        // Feed TPM-provided random bytes into the kernel entropy pool.
        if (toDump > 0)
        {
            QK::Entropy::addEntropy(rspBuf + 12, toDump);

            // Enable TPM-backed wrap key sealing for SecureStore.
            g_tpmSecureStore.ctx = ctx;
            g_tpmSecureStore.ready = true;

            auto scCfg = QK::SecureStore::defaultConfig();
            scCfg.tpmUser = &g_tpmSecureStore;
            scCfg.tpmSealWrapKey = &secureStoreTpmSealWrapKey;
            scCfg.tpmUnsealWrapKey = &secureStoreTpmUnsealWrapKey;
            QK::SecureStore::setDefaultConfig(scCfg);
            serialPrint("SecureStore: TPM wrap-key enabled\r\n");
        }
    }

    static void acpiEnumerateTables(QC::PhysAddr rsdpPhys)
    {
        if (rsdpPhys == 0)
        {
            serialPrint("ACPI: no RSDP address\r\n");
            return;
        }

        printHex64("ACPI: RSDP phys ", rsdpPhys);
        if (!ensureHhdmMapped(rsdpPhys, sizeof(AcpiRsdp)))
        {
            serialPrint("ACPI: RSDP mapping failed\r\n");
            return;
        }

        auto *rsdp = reinterpret_cast<const AcpiRsdp *>(physToVirt(rsdpPhys));
        if (QC::String::memcmp(rsdp->signature, "RSD PTR ", 8) != 0)
        {
            serialPrint("ACPI: invalid RSDP signature\r\n");
            return;
        }

        serialPrint("ACPI: RSDP OK\r\n");
        serialPrint("ACPI: using ");

        bool useXsdt = (rsdp->revision >= 2) && (rsdp->xsdtAddress != 0);
        QC::PhysAddr sdtPhys = useXsdt ? static_cast<QC::PhysAddr>(rsdp->xsdtAddress)
                                       : static_cast<QC::PhysAddr>(rsdp->rsdtAddress);

        serialPrint(useXsdt ? "XSDT\r\n" : "RSDT\r\n");
        if (sdtPhys == 0)
        {
            serialPrint("ACPI: SDT address is null\r\n");
            return;
        }

        if (!ensureHhdmMapped(sdtPhys, sizeof(AcpiSdtHeader)))
        {
            serialPrint("ACPI: SDT header mapping failed\r\n");
            return;
        }

        auto *sdt = reinterpret_cast<const AcpiSdtHeader *>(physToVirt(sdtPhys));
        printHex64("ACPI: SDT phys ", sdtPhys);

        if (!ensureHhdmMapped(sdtPhys, sdt->length))
        {
            serialPrint("ACPI: SDT mapping failed\r\n");
            return;
        }

        QC::usize entrySize = useXsdt ? sizeof(QC::u64) : sizeof(QC::u32);
        if (sdt->length < sizeof(AcpiSdtHeader) || ((sdt->length - sizeof(AcpiSdtHeader)) % entrySize) != 0)
        {
            serialPrint("ACPI: SDT length invalid\r\n");
            return;
        }

        QC::usize entryCount = (sdt->length - sizeof(AcpiSdtHeader)) / entrySize;
        serialPrint("ACPI: table signatures:\r\n");

        bool foundTpm2 = false;
        QC::PhysAddr tpm2Phys = 0;
        const QC::u8 *entries = reinterpret_cast<const QC::u8 *>(sdt) + sizeof(AcpiSdtHeader);

        for (QC::usize i = 0; i < entryCount; ++i)
        {
            QC::PhysAddr tablePhys = 0;
            if (useXsdt)
                tablePhys = static_cast<QC::PhysAddr>(reinterpret_cast<const QC::u64 *>(entries)[i]);
            else
                tablePhys = static_cast<QC::PhysAddr>(reinterpret_cast<const QC::u32 *>(entries)[i]);

            if (tablePhys == 0)
                continue;

            if (!ensureHhdmMapped(tablePhys, sizeof(AcpiSdtHeader)))
                continue;

            auto *hdr = reinterpret_cast<const AcpiSdtHeader *>(physToVirt(tablePhys));

            char sig[5] = {hdr->signature[0], hdr->signature[1], hdr->signature[2], hdr->signature[3], 0};
            serialPrint("  - ");
            serialPrint(sig);
            serialPrint("\r\n");

            if (QC::String::memcmp(hdr->signature, "TPM2", 4) == 0)
            {
                foundTpm2 = true;
                tpm2Phys = tablePhys;
            }
        }

        serialPrint(foundTpm2 ? "ACPI: TPM2 table present\r\n" : "ACPI: TPM2 table NOT present\r\n");
        if (!foundTpm2 || !tpm2Phys)
            return;

        serialPrint("ACPI: TPM2 details\r\n");
        if (!ensureHhdmMapped(tpm2Phys, sizeof(AcpiSdtHeader)))
        {
            serialPrint("ACPI: TPM2 header mapping failed\r\n");
            return;
        }

        auto *tpm2Hdr = reinterpret_cast<const AcpiSdtHeader *>(physToVirt(tpm2Phys));
        if (tpm2Hdr->length < sizeof(AcpiTpm2TableBase))
        {
            serialPrint("ACPI: TPM2 length too small\r\n");
            return;
        }

        if (!ensureHhdmMapped(tpm2Phys, tpm2Hdr->length))
        {
            serialPrint("ACPI: TPM2 mapping failed\r\n");
            return;
        }

        auto *tpm2 = reinterpret_cast<const AcpiTpm2TableBase *>(physToVirt(tpm2Phys));
        printDecU32("  platformClass: ", static_cast<QC::u32>(tpm2->platformClass));
        printDecU32("  startMethod: ", tpm2->startMethod);
        if (tpm2->startMethod == 6 || tpm2->startMethod == 7)
            serialPrint("  startMethodHint: CRB\r\n");
        printHex64("  controlArea phys ", tpm2->controlArea);

        if (tpm2->controlArea)
        {
            QC::PhysAddr controlPhys = static_cast<QC::PhysAddr>(tpm2->controlArea);
            if (ensureHhdmMappedMmio(controlPhys & ~static_cast<QC::PhysAddr>(0xFFF), 4096))
            {
                serialPrint("  controlArea mapped\r\n");
                if (tpm2->startMethod == 6 || tpm2->startMethod == 7)
                {
                    serialPrint("TPM2: CRB control area dump (first 0x100 bytes)\r\n");
                    volatile const QC::u8 *base = reinterpret_cast<volatile const QC::u8 *>(physToVirt(controlPhys & ~static_cast<QC::PhysAddr>(0xFFF)));
                    QC::usize startOff = static_cast<QC::usize>(controlPhys & 0xFFF);

                    for (QC::usize off = 0; off < 0x100; off += 16)
                    {
                        serialPrint("  +0x");
                        QC::u32 o = static_cast<QC::u32>(off);
                        char obuf[4];
                        for (int i = 2; i >= 0; --i)
                        {
                            int nibble = (o >> (i * 4)) & 0xF;
                            obuf[2 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
                        }
                        obuf[3] = 0;
                        serialPrint(obuf);
                        serialPrint(": ");

                        for (QC::usize w = 0; w < 4; ++w)
                        {
                            const QC::u32 *p = reinterpret_cast<const QC::u32 *>(reinterpret_cast<QC::VirtAddr>(base + startOff + off + w * 4));
                            QC::u32 v = *p;
                            printHex32Fixed(v);
                            if (w != 3)
                                serialPrint(" ");
                        }
                        serialPrint("\r\n");
                    }

                    tryTpm2CrbStartup(tpm2->startMethod, controlPhys);
                }
            }
            else
            {
                serialPrint("  controlArea map failed\r\n");
            }
        }

        constexpr QC::usize kOptionalOffset = sizeof(AcpiTpm2TableBase);
        if (tpm2Hdr->length >= kOptionalOffset + sizeof(QC::u32) + sizeof(QC::u64))
        {
            const QC::u8 *base = reinterpret_cast<const QC::u8 *>(tpm2);
            QC::u32 laml = *reinterpret_cast<const QC::u32 *>(base + kOptionalOffset);
            QC::u64 lasa = *reinterpret_cast<const QC::u64 *>(base + kOptionalOffset + sizeof(QC::u32));
            printDecU32("  laml: ", laml);
            printHex64("  lasa phys ", lasa);
        }
        else
        {
            serialPrint("  eventLog: none\r\n");
        }
    }

} // namespace

static QFS::VFS *g_vfs = nullptr;
static constexpr QC::usize kRamdiskSectorSize = 512;

enum class StartupMode : QC::u8
{
    Desktop,
    Terminal,
    Safe,
    Recovery,
    Installer,
    Network
};

static StartupMode g_startupMode = StartupMode::Desktop;

static QK::SecurityCenter::Mode g_scMode = QK::SecurityCenter::Mode::Bypass;

static bool g_ideSharedProbeEnabled = false;

static char g_bootSaveTermValue[256] = {0};
static bool g_powerOffAfterSaveTerm = false;
static bool g_bootSaveTermDone = false;

static const char *startupModeName(StartupMode mode)
{
    switch (mode)
    {
    case StartupMode::Desktop:
        return "DESKTOP";
    case StartupMode::Terminal:
        return "TERMINAL";
    case StartupMode::Safe:
        return "SAFE";
    case StartupMode::Recovery:
        return "RECOVERY";
    case StartupMode::Installer:
        return "INSTALLER";
    case StartupMode::Network:
        return "NETWORK";
    default:
        return "UNKNOWN";
    }
}

static bool isWhitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static QC::usize stringLength(const char *str)
{
    if (!str)
        return 0;

    QC::usize len = 0;
    while (str[len] != '\0')
    {
        ++len;
    }
    return len;
}

static char toLowerChar(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return static_cast<char>(ch + ('a' - 'A'));
    return ch;
}

static bool equalsIgnoreCase(const char *a, const char *b)
{
    if (!a || !b)
        return false;

    while (*a && *b)
    {
        if (toLowerChar(*a) != toLowerChar(*b))
            return false;
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static void trimTrailingWhitespace(char *str)
{
    if (!str)
        return;

    QC::isize len = static_cast<QC::isize>(stringLength(str));
    while (len > 0 && isWhitespace(str[len - 1]))
    {
        str[len - 1] = '\0';
        --len;
    }
}

static char *trimWhitespace(char *str)
{
    if (!str)
        return str;

    while (*str && isWhitespace(*str))
    {
        ++str;
    }

    trimTrailingWhitespace(str);
    return str;
}

static void stripInlineComment(char *value)
{
    if (!value)
        return;

    for (char *ptr = value; *ptr; ++ptr)
    {
        if (*ptr == '#' || *ptr == ';' || (*ptr == '/' && *(ptr + 1) == '/'))
        {
            *ptr = '\0';
            break;
        }
    }

    trimTrailingWhitespace(value);
}

static StartupMode parseStartupModeValue(const char *value)
{
    if (!value || *value == '\0')
        return StartupMode::Desktop;

    if (equalsIgnoreCase(value, "DESKTOP"))
        return StartupMode::Desktop;
    if (equalsIgnoreCase(value, "TERMINAL"))
        return StartupMode::Terminal;
    if (equalsIgnoreCase(value, "SAFE"))
        return StartupMode::Safe;
    if (equalsIgnoreCase(value, "RECOVERY"))
        return StartupMode::Recovery;
    if (equalsIgnoreCase(value, "INSTALLER"))
        return StartupMode::Installer;
    if (equalsIgnoreCase(value, "NETWORK"))
        return StartupMode::Network;

    serialPrint("Unknown startup MODE value: ");
    serialPrint(value);
    serialPrint(" (defaulting to DESKTOP)\r\n");
    return StartupMode::Desktop;
}

static bool parseBoolValue(const char *value, bool defaultValue)
{
    if (!value || *value == '\0')
        return defaultValue;

    if (equalsIgnoreCase(value, "1") || equalsIgnoreCase(value, "TRUE") || equalsIgnoreCase(value, "YES") || equalsIgnoreCase(value, "ON"))
        return true;
    if (equalsIgnoreCase(value, "0") || equalsIgnoreCase(value, "FALSE") || equalsIgnoreCase(value, "NO") || equalsIgnoreCase(value, "OFF"))
        return false;

    return defaultValue;
}

static QK::SecurityCenter::Mode parseScModeValue(const char *value)
{
    if (!value || *value == '\0')
        return QK::SecurityCenter::Mode::Bypass;

    if (equalsIgnoreCase(value, "BYPASS"))
        return QK::SecurityCenter::Mode::Bypass;
    if (equalsIgnoreCase(value, "ENFORCE"))
        return QK::SecurityCenter::Mode::Enforce;

    serialPrint("Unknown SC_MODE value: ");
    serialPrint(value);
    serialPrint(" (defaulting to BYPASS)\r\n");
    return QK::SecurityCenter::Mode::Bypass;
}

static void handleStartupConfigLine(char *line)
{
    if (!line)
        return;

    char *trimmed = trimWhitespace(line);
    if (*trimmed == '\0')
        return;

    if (*trimmed == '#' || (*trimmed == '/' && *(trimmed + 1) == '/'))
        return;

    char *key = nullptr;
    char *value = nullptr;
    char *delimiter = nullptr;
    for (char *ptr = trimmed; *ptr; ++ptr)
    {
        if (*ptr == '=')
        {
            delimiter = ptr;
            break;
        }
    }

    if (delimiter)
    {
        *delimiter = '\0';
        key = trimWhitespace(trimmed);
        value = trimWhitespace(delimiter + 1);
    }
    else
    {
        // Support whitespace-delimited key/value pairs like "MODE TERMINAL"
        char *split = trimmed;
        while (*split && !isWhitespace(*split))
        {
            ++split;
        }

        if (!*split)
            return;

        *split = '\0';
        key = trimWhitespace(trimmed);
        value = trimWhitespace(split + 1);
    }

    stripInlineComment(value);

    if (*key == '\0' || *value == '\0')
        return;

    if (equalsIgnoreCase(key, "MODE"))
    {
        g_startupMode = parseStartupModeValue(value);
        return;
    }

    if (equalsIgnoreCase(key, "SC_MODE"))
    {
        g_scMode = parseScModeValue(value);
        return;
    }

    if (equalsIgnoreCase(key, "SC_BYPASS"))
    {
        const bool bypass = parseBoolValue(value, true);
        g_scMode = bypass ? QK::SecurityCenter::Mode::Bypass : QK::SecurityCenter::Mode::Enforce;
        return;
    }

    if (equalsIgnoreCase(key, "IDE_SHARED"))
    {
        g_ideSharedProbeEnabled = parseBoolValue(value, false);
        return;
    }

    if (equalsIgnoreCase(key, "SAVETERM"))
    {
        QC::String::strncpy(g_bootSaveTermValue, value, sizeof(g_bootSaveTermValue) - 1);
        g_bootSaveTermValue[sizeof(g_bootSaveTermValue) - 1] = '\0';
        g_bootSaveTermDone = false;
        return;
    }

    if (equalsIgnoreCase(key, "POWEROFF_AFTER_SAVETERM"))
    {
        g_powerOffAfterSaveTerm = parseBoolValue(value, false);
        return;
    }
}

static void loadStartupConfiguration()
{
    if (!g_vfs)
        return;

    QFS::File *file = g_vfs->open("/startup.cfg", QFS::OpenMode::Read);
    if (!file)
    {
        serialPrint("startup.cfg not found; defaulting to DESKTOP\r\n");
        g_startupMode = StartupMode::Desktop;
        return;
    }

    char chunk[128];
    char lineBuffer[256];
    QC::usize lineLength = 0;

    QC::isize bytesRead = 0;
    while ((bytesRead = file->read(chunk, sizeof(chunk))) > 0)
    {
        for (QC::isize i = 0; i < bytesRead; ++i)
        {
            char ch = chunk[i];
            if (ch == '\r')
                continue;

            if (ch == '\n')
            {
                lineBuffer[lineLength] = '\0';
                handleStartupConfigLine(lineBuffer);
                lineLength = 0;
                continue;
            }

            if (lineLength + 1 < sizeof(lineBuffer))
            {
                lineBuffer[lineLength++] = ch;
            }
        }
    }

    if (lineLength > 0)
    {
        lineBuffer[lineLength] = '\0';
        handleStartupConfigLine(lineBuffer);
    }

    g_vfs->close(file);

    serialPrint("Startup mode loaded: ");
    serialPrint(startupModeName(g_startupMode));
    serialPrint("\r\n");

    serialPrint("Security Center mode loaded: ");
    serialPrint(QK::SecurityCenter::modeName(g_scMode));
    serialPrint("\r\n");

    serialPrint("IDE_SHARED loaded: ");
    serialPrint(g_ideSharedProbeEnabled ? "ON" : "OFF");
    serialPrint("\r\n");
}

static void bootSaveTermOnceIfConfigured()
{
    if (g_bootSaveTermDone)
        return;
    if (g_bootSaveTermValue[0] == '\0')
        return;
    if (equalsIgnoreCase(g_bootSaveTermValue, "0"))
        return;

    g_bootSaveTermDone = true;

    if (!QFS::VolumeManager::instance().isMounted("QFS_SHARED"))
    {
        serialPrint("SAVETERM: /shared not mounted; skipping\r\n");
        return;
    }

    char cmd[320];
    QC::String::memset(cmd, 0, sizeof(cmd));
    if (equalsIgnoreCase(g_bootSaveTermValue, "1"))
    {
        QC::String::strncpy(cmd, "saveterm", sizeof(cmd) - 1);
    }
    else
    {
        QC::String::strncpy(cmd, "saveterm ", sizeof(cmd) - 1);
        QC::usize used = QC::String::strlen(cmd);
        QC::String::strncpy(cmd + used, g_bootSaveTermValue, sizeof(cmd) - 1 - used);
    }

    QK::Console::executeLine(cmd);

    if (g_powerOffAfterSaveTerm)
    {
        QK::Shutdown::Controller::instance().requestShutdown(QK::Shutdown::Reason::SystemPolicy);
    }
}

[[noreturn]] static void enterTerminalOnlyLoop()
{
    serialPrint("Entering console-only startup path (mode: ");
    serialPrint(startupModeName(g_startupMode));
    serialPrint(")\r\n");

    while (true)
    {
        QKDrv::Manager::instance().poll();
        QKDrv::PS2::Keyboard::instance().poll();
        asm volatile("hlt");
    }
}

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
    if (!ensureVfsReady())
        return false;

    auto &volumeManager = QFS::VolumeManager::instance();
    constexpr const char *kRamdiskVolumeName = "QFS_RAMDISK0";
    if (volumeManager.isMounted(kRamdiskVolumeName))
    {
        loadStartupConfiguration();
        QKDrv::IDE::setSharedProbeEnabled(g_ideSharedProbeEnabled);
        QK::SecurityCenter::instance().initialize(g_scMode);
        return true;
    }

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

    if (!g_ramdiskDevice)
    {
        g_ramdiskDevice = new QKStorage::MemoryBlockDevice(base, size, kRamdiskSectorSize);
    }

    QKStorage::BlockDeviceRegistration ramdiskReg{};
    ramdiskReg.name = kRamdiskVolumeName;
    ramdiskReg.mountPath = "/";
    ramdiskReg.fsKind = QFS::FileSystemKind::FAT32;
    ramdiskReg.device = g_ramdiskDevice;

    QC::Status registerStatus = QKStorage::registerBlockDevice(ramdiskReg);
    if (registerStatus != QC::Status::Success && registerStatus != QC::Status::Busy)
    {
        serialPrint("Failed to register ramdisk volume\r\n");
        return false;
    }

    if (!volumeManager.isMounted(kRamdiskVolumeName))
    {
        QC::Status mountStatus = volumeManager.mountVolume(kRamdiskVolumeName);
        if (mountStatus != QC::Status::Success)
        {
            serialPrint("Failed to mount ramdisk filesystem\r\n");
            return false;
        }
    }

    serialPrint("Ramdisk mounted at /\r\n");
    fileIoDemo();
    secureStoreSelfTest();
    secureStorePcrMismatchTest();
    loadStartupConfiguration();
    QKDrv::IDE::setSharedProbeEnabled(g_ideSharedProbeEnabled);
    QK::SecurityCenter::instance().initialize(g_scMode);
    return true;
}

static void secureStoreSelfTest()
{
    serialPrint("SecureStore: self-test...\r\n");

    QC::Status st = QK::SecureStore::ensureBaseDir();
    if (st != QC::Status::Success)
    {
        serialPrint("SecureStore: FAIL (ensureBaseDir)\r\n");
        return;
    }

    QC::u8 plain[96];
    (void)QK::Entropy::fillRandom(plain, sizeof(plain));

    st = QK::SecureStore::writeSealedBlob("SSTEST.BIN", plain, sizeof(plain));
    if (st != QC::Status::Success)
    {
        serialPrint("SecureStore: FAIL (writeSealedBlob)\r\n");
        return;
    }

    QC::Vector<QC::u8> out;
    st = QK::SecureStore::readSealedBlob("SSTEST.BIN", out);
    if (st != QC::Status::Success)
    {
        serialPrint("SecureStore: FAIL (readSealedBlob)\r\n");
        (void)QK::SecureStore::removeBlob("SSTEST.BIN");
        return;
    }

    bool ok = (out.size() == sizeof(plain));
    if (ok)
    {
        ok = (QC::String::memcmp(out.data(), plain, sizeof(plain)) == 0);
    }

    (void)QK::SecureStore::removeBlob("SSTEST.BIN");
    QC::String::memset(plain, 0, sizeof(plain));
    if (out.size() > 0)
        QC::String::memset(out.data(), 0, out.size());
    out.clear();

    serialPrint(ok ? "SecureStore: PASS\r\n" : "SecureStore: FAIL (mismatch)\r\n");
}

static bool shouldRunPcrMismatchTest()
{
    if (!g_vfs)
        return false;
    QFS::File *f = g_vfs->open("/PCRTEST.FLG", QFS::OpenMode::Read);
    if (!f)
        return false;
    delete f;
    return true;
}

static void secureStorePcrMismatchTest()
{
    if (!shouldRunPcrMismatchTest())
        return;

    if (!g_tpmSecureStore.ready)
    {
        serialPrint("SecureStore: PCR mismatch test SKIP (no TPM)\r\n");
        return;
    }

    serialPrint("SecureStore: PCR mismatch test...\r\n");

    // Ensure we have a sealed blob under the current PCR state.
    QC::u8 plain[64];
    (void)QK::Entropy::fillRandom(plain, sizeof(plain));
    QC::Status st = QK::SecureStore::writeSealedBlob("PCRNEG.BIN", plain, sizeof(plain));
    if (st != QC::Status::Success)
    {
        serialPrint("SecureStore: PCR mismatch test FAIL (write)\r\n");
        return;
    }

    // Mutate PCR7 so the policy should no longer match.
    QC::u8 extendDigest[32];
    for (QC::usize i = 0; i < sizeof(extendDigest); ++i)
        extendDigest[i] = static_cast<QC::u8>(0x42u ^ static_cast<QC::u8>(i));

    st = tpmPcrExtendSha256(g_tpmSecureStore.ctx, 7, extendDigest);
    if (st != QC::Status::Success)
    {
        serialPrint("SecureStoreTPM: PCR_Extend failed (rsp=0x");
        printHex32Fixed(g_tpmLastRspCode);
        serialPrint(")\r\n");
        serialPrint("SecureStore: PCR mismatch test FAIL (extend)\r\n");
        (void)QK::SecureStore::removeBlob("PCRNEG.BIN");
        return;
    }

    // Attempt to read; expected to fail because wrap-key unseal should be blocked.
    QC::Vector<QC::u8> out;
    st = QK::SecureStore::readSealedBlob("PCRNEG.BIN", out);

    const bool expectedFail = (st != QC::Status::Success);
    if (expectedFail)
        serialPrint("SecureStore: PCR mismatch test PASS (unseal blocked)\r\n");
    else
        serialPrint("SecureStore: PCR mismatch test FAIL (unexpected unseal)\r\n");

    // Cleanup and recovery: remove the now-unusable wrap-key blob so the next
    // SecureStore use can reseal under the new PCR state.
    (void)QK::SecureStore::removeBlob("PCRNEG.BIN");
    (void)QK::SecureStore::removeBlob("WRAPKEY.TPM");
    (void)QK::SecureStore::removeBlob("WRAPKEY.BIN");
    QC::String::memset(plain, 0, sizeof(plain));
    if (out.size() > 0)
        QC::String::memset(out.data(), 0, out.size());
    out.clear();
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

    QK::Console::initialize(serialPrint);
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

    // Step 1: Determine firmware type (UEFI vs BIOS)
    auto *fwResp = reinterpret_cast<limine_firmware_type_response *>(limine_firmware_type_request[5]);
    if (fwResp)
    {
        serialPrint("Firmware: ");
        serialPrint(firmwareTypeToString(fwResp->firmware_type));
        serialPrint("\r\n");
    }
    else
    {
        serialPrint("Firmware: unknown (no response)\r\n");
    }

    // Steps 2-3: Get RSDP and enumerate ACPI tables; detect TPM2 presence
    auto *rsdpResp = reinterpret_cast<limine_rsdp_response *>(limine_rsdp_request[5]);
    if (rsdpResp && rsdpResp->address)
    {
        acpiEnumerateTables(static_cast<QC::PhysAddr>(rsdpResp->address));
    }
    else
    {
        serialPrint("ACPI: no RSDP response\r\n");
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

            QKStorage::probeLimineModules();

            // Set up keyboard callback so the console works in every startup mode
            serialPrint("Setting up keyboard...\r\n");
            auto &ps2Keyboard = QKDrv::PS2::Keyboard::instance();
            ps2Keyboard.setPS2Callback([](const QKDrv::PS2::KeyEvent &evt)
                                       {
                // In Desktop mode, keyboard input is owned by the windowing/event system.
                // Routing keys to the serial console too causes accidental command execution.
                if (g_startupMode != StartupMode::Desktop)
                {
                    QK::Console::handleKeyEvent(evt);
                    return;
                }

                auto &eventMgr = QK::Event::EventManager::instance();

                QK::Event::Modifiers mods = QK::Event::Modifiers::None;
                if (evt.shift)
                    mods = mods | QK::Event::Modifiers::Shift;
                if (evt.ctrl)
                    mods = mods | QK::Event::Modifiers::Ctrl;
                if (evt.alt)
                    mods = mods | QK::Event::Modifiers::Alt;

                eventMgr.postKeyEvent(
                    evt.pressed ? QK::Event::Type::KeyDown : QK::Event::Type::KeyUp,
                    static_cast<QC::u8>(evt.key),
                    static_cast<QC::u8>(evt.key),
                    evt.character,
                    mods,
                    false); });
            serialPrint("Keyboard initialized\r\n");

            // Desktop owns keyboard input; keep serial console non-interactive.
            QK::Console::setInputEnabled(g_startupMode != StartupMode::Desktop);

            if (g_startupMode != StartupMode::Desktop)
            {
                serialPrint("Startup mode ");
                serialPrint(startupModeName(g_startupMode));
                serialPrint(" selected - skipping desktop bring-up\r\n");
                enterTerminalOnlyLoop();
            }

            // Create and initialize framebuffer
            static QW::Framebuffer framebuffer;

            // If we're running under VMware SVGA II (QEMU `-vga vmware`), the device exposes
            // the authoritative pitch via SVGA_REG_BYTES_PER_LINE. Compare it with Limine's
            // pitch and use SVGA's value when it looks safer.
            {
                auto &svga = QDrv::VmwareSVGA::instance();
                if (svga.initialize())
                {
                    const QC::u32 svgaPitch = svga.bytesPerLine();
                    const QC::u32 svgaFbSize = svga.framebufferSizeBytes();
                    QC_LOG_INFO("QKMain", "Framebuffer pitch: limine=%u svga=%u (fb_size=%u)", pitch, svgaPitch, svgaFbSize);

                    const QC::u32 minPitch = width * 4u; // ARGB8888
                    if (svgaPitch >= minPitch && svgaPitch <= (1024u * 1024u))
                    {
                        const QC::u64 needed = static_cast<QC::u64>(svgaPitch) * static_cast<QC::u64>(height);
                        if (svgaFbSize == 0 || needed <= svgaFbSize)
                        {
                            if (svgaPitch != pitch)
                            {
                                QC_LOG_WARN("QKMain", "Overriding Limine pitch %u -> SVGA bytes-per-line %u", pitch, svgaPitch);
                                pitch = svgaPitch;
                            }
                        }
                        else
                        {
                            QC_LOG_WARN("QKMain", "SVGA pitch rejected: need=%llu > fb_size=%u", (unsigned long long)needed, svgaFbSize);
                        }
                    }
                }
            }

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

            // Track previous pointer position for generating deltas in absolute mode.
            static bool prevPosValid = false;
            static QC::i32 prevX = 0;
            static QC::i32 prevY = 0;

            // Set up mouse callback using the new driver system
            if (mouseDriver)
            {
                mouseDriver->setCallback([](const QKDrv::MouseReport &report)
                                         {
                    auto *mouse = QKDrv::Manager::instance().mouseDriver();
                    if (!mouse) return;
                    
                    auto &eventMgr = QK::Event::EventManager::instance();

                    // For absolute devices (USB tablet), report.x/y are screen coordinates.
                    // For relative devices (PS/2 mouse), report.x/y are deltas and mouse->x/y are absolute.
                    const QC::i32 curX = report.isAbsolute ? report.x : mouse->x();
                    const QC::i32 curY = report.isAbsolute ? report.y : mouse->y();

                    QC::i32 dx = 0;
                    QC::i32 dy = 0;
                    if (report.isAbsolute)
                    {
                        if (prevPosValid)
                        {
                            dx = curX - prevX;
                            dy = curY - prevY;
                        }
                        prevX = curX;
                        prevY = curY;
                        prevPosValid = true;
                    }
                    else
                    {
                        dx = report.x;
                        dy = report.y;
                    }

                    // Movement telemetry is useful for driver bring-up, but it's very noisy.
                    // Log:
                    //  - always when buttons change
                    //  - periodically on movement (dx/dy != 0)
                    //  - and a slow heartbeat even when idle
                    static QC::u32 s_mouseReportCount = 0;
                    static QC::u32 s_mouseMoveCount = 0;
                    static QC::u8 s_prevButtons = 0;
                    ++s_mouseReportCount;

                    const bool buttonsChanged = (report.buttons != s_prevButtons);
                    s_prevButtons = report.buttons;

                    const bool moved = (dx != 0) || (dy != 0);
                    bool logThis = false;
                    if (buttonsChanged)
                    {
                        logThis = true;
                    }
                    else if (moved)
                    {
                        ++s_mouseMoveCount;
                        // Log every Nth movement report.
                        logThis = ((s_mouseMoveCount % 20u) == 0u);
                    }
                    else
                    {
                        // Slow heartbeat so we know input is still flowing.
                        logThis = ((s_mouseReportCount % 600u) == 0u);
                    }

                    if (logThis)
                    {
                        serialPrint("Mouse report (");
                        serialPrint(report.isAbsolute ? "abs" : "rel");
                        serialPrint(") pos(");
                        serialPrintInt(curX);
                        serialPrint(",");
                        serialPrintInt(curY);
                        serialPrint(") d(");
                        serialPrintInt(dx);
                        serialPrint(",");
                        serialPrintInt(dy);
                        serialPrint(") buttons=");
                        serialPrintInt(report.buttons);
                        serialPrint("\r\n");
                    }

                    auto logClick = [&](const char *label)
                    {
                        // NOTE: A click packet often has dx/dy = 0; that's normal for relative mice.
                        serialPrint(label);
                        serialPrint(" at (");
                        serialPrintInt(curX);
                        serialPrint(", ");
                        serialPrintInt(curY);
                        serialPrint(") ");
                        serialPrint(report.isAbsolute ? "abs" : "rel");
                        serialPrint("\r\n");
                    };

                    bool leftBtn = report.buttons & 0x01;
                    bool rightBtn = report.buttons & 0x02;

                    // Post mouse move event first so hover state is up-to-date.
                    eventMgr.postMouseMove(curX, curY, dx, dy);
                    
                    // Check for button state changes
                    if (leftBtn && !prevLeftBtn) {
                        logClick("Left click");
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonDown, 
                                                 QK::Event::MouseButton::Left,
                                                 curX, curY, QK::Event::Modifiers::None);
                    }
                    if (!leftBtn && prevLeftBtn) {
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonUp,
                                                 QK::Event::MouseButton::Left,
                                                 curX, curY, QK::Event::Modifiers::None);
                    }
                    if (rightBtn && !prevRightBtn) {
                        logClick("Right click");
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonDown,
                                                 QK::Event::MouseButton::Right,
                                                 curX, curY, QK::Event::Modifiers::None);
                    }
                    if (!rightBtn && prevRightBtn) {
                        eventMgr.postMouseButton(QK::Event::Type::MouseButtonUp,
                                                 QK::Event::MouseButton::Right,
                                                 curX, curY, QK::Event::Modifiers::None);
                    }
                    
                    prevLeftBtn = leftBtn;
                    prevRightBtn = rightBtn; });
            }
            serialPrint("Mouse configured\r\n");

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
