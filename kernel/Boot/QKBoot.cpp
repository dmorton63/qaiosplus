#include "Boot/QKBoot.h"

#include "Boot/Limine/LimineRequests.h"
#include "Boot/Memory/AddressMapping.h"
#include "Boot/Memory/EarlyMemory.h"

#include "Boot/Acpi/AcpiTables.h"
#include "Boot/Arch/ArchInit.h"
#include "Boot/Desktop/DesktopSession.h"
#include "Boot/Tpm/TpmSecureStore.h"

namespace
{
    QKBoot::FLogFn g_Log = nullptr;
    QKBoot::LimineRequests g_Req{};
    bool g_DesktopPrepared = false;

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

}

namespace QKBoot
{
    void setLogFn(FLogFn log)
    {
        g_Log = log;
    }

    void setLimineRequests(const LimineRequests &req)
    {
        g_Req = req;
    }

    void initializeMemory()
    {
        QK::Boot::Memory::InitFromLimineRequests(g_Req.hhdm, g_Req.kernelAddress, g_Log);
    }

    void initializeDrivers()
    {
        if (g_Log)
        {
            const limine_firmware_type_response *FwResp = QK::Boot::Limine::GetFirmwareTypeResponse(g_Req.firmwareType);
            if (FwResp)
            {
                g_Log("Firmware: ");
                g_Log(firmwareTypeToString(FwResp->firmware_type));
                g_Log("\r\n");
            }
            else
            {
                g_Log("Firmware: unknown (no response)\r\n");
            }

            const limine_rsdp_response *RsdpResp = QK::Boot::Limine::GetRsdpResponse(g_Req.rsdp);
            if (RsdpResp && RsdpResp->address)
            {
                const QC::PhysAddr rsdpPhys = static_cast<QC::PhysAddr>(reinterpret_cast<QC::uptr>(RsdpResp->address));
                QK::Boot::Acpi::EnumerateTables(rsdpPhys, g_Log, &QK::Boot::Tpm::TryTpm2CrbStartup);
            }
            else
            {
                g_Log("ACPI: no RSDP response\r\n");
            }
        }
        else
        {
            const limine_rsdp_response *RsdpResp = QK::Boot::Limine::GetRsdpResponse(g_Req.rsdp);
            if (RsdpResp && RsdpResp->address)
            {
                const QC::PhysAddr rsdpPhys = static_cast<QC::PhysAddr>(reinterpret_cast<QC::uptr>(RsdpResp->address));
                QK::Boot::Acpi::EnumerateTables(rsdpPhys, nullptr, &QK::Boot::Tpm::TryTpm2CrbStartup);
            }
        }

        QK::Boot::Arch::InitCpuGdtIdtAndInterrupts(g_Log);
    }

    void initializeGraphics()
    {
        const auto BootHeap = QK::Boot::Memory::GetEarlyHeap();
        QK::Boot::Desktop::EarlyHeap Heap{};
        Heap.Buffer = BootHeap.Buffer;
        Heap.Size = BootHeap.Size;

        g_DesktopPrepared = QK::Boot::Desktop::PrepareFromLimineRequests(g_Req.framebuffer, g_Req.modules, Heap, g_Log);
        if (g_Log)
        {
            if (g_DesktopPrepared)
                g_Log("Graphics: framebuffer present\r\n");
            else
                g_Log("Graphics: no framebuffer response\r\n");
        }
    }

    void initializeInput()
    {
        if (!g_DesktopPrepared)
        {
            if (g_Log)
                g_Log("Input: no framebuffer; skipping\r\n");
            return;
        }

        QK::Boot::Desktop::InitializeInput();
    }

    void initializeWindowSystem()
    {
        if (!g_DesktopPrepared)
        {
            if (g_Log)
                g_Log("WindowSystem: no framebuffer; skipping\r\n");
            return;
        }

        QK::Boot::Desktop::InitializeWindowSystem();
    }

    [[noreturn]] void initializeDesktop()
    {
        if (g_Log)
            g_Log("Desktop: starting session\r\n");

        if (!g_DesktopPrepared)
        {
            if (g_Log)
                g_Log("Desktop: no framebuffer response; halting\r\n");
            for (;;)
                asm volatile("hlt");
        }

        QK::Boot::Desktop::InitializeDesktopAndRunLoop();

        // If the desktop session ever returns, halt.
        for (;;)
        {
            asm volatile("hlt");
        }
    }
}
