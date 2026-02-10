#pragma once

// QKDrvXHCI - USB 3.0 xHCI Controller driver
// Namespace: QKDrv::XHCI

#include "../QKDrvBase.h"
#include "QArchPCI.h"

namespace QKDrv
{
    namespace XHCI
    {

        // USB device speed
        enum class Speed : QC::u8
        {
            Full = 1,
            Low = 2,
            High = 3,
            Super = 4,
            SuperPlus = 5
        };

        // xHCI capability registers
        struct CapRegs
        {
            QC::u8 capLength;
            QC::u8 reserved;
            QC::u16 hciVersion;
            QC::u32 hcsParams1;
            QC::u32 hcsParams2;
            QC::u32 hcsParams3;
            QC::u32 hccParams1;
            QC::u32 dbOffset;
            QC::u32 rtsOffset;
            QC::u32 hccParams2;
        } __attribute__((packed));

        // xHCI operational registers
        struct OpRegs
        {
            QC::u32 usbcmd;
            QC::u32 usbsts;
            QC::u32 pageSize;
            QC::u32 reserved1[2];
            QC::u32 dnctrl;
            QC::u64 crcr;
            QC::u32 reserved2[4];
            QC::u64 dcbaap;
            QC::u32 config;
        } __attribute__((packed));

        // Port register set
        struct PortRegs
        {
            QC::u32 portsc;
            QC::u32 portpmsc;
            QC::u32 portli;
            QC::u32 porthlpmc;
        } __attribute__((packed));

        // TRB (Transfer Request Block)
        struct TRB
        {
            QC::u64 parameter;
            QC::u32 status;
            QC::u32 control;
        } __attribute__((packed));

        // HID Tablet report (absolute coordinates)
        struct HIDTabletReport
        {
            QC::u8 buttons;
            QC::u16 x;
            QC::u16 y;
            QC::i8 wheel;
        } __attribute__((packed));

        class Controller : public DriverBase
        {
        public:
            static Controller *probe(QArch::PCIDevice *pciDevice);

            Controller(QArch::PCIDevice *pciDevice);
            ~Controller();

            // DriverBase interface
            QC::Status initialize() override;
            void shutdown() override;
            void poll() override;
            const char *name() const override { return "xHCI"; }
            ControllerType controllerType() const override { return ControllerType::XHCI; }

            // USB operations
            QC::u8 portCount() const { return m_portCount; }
            bool isPortConnected(QC::u8 port) const;
            Speed getPortSpeed(QC::u8 port) const;
            void resetPort(QC::u8 port);

            // Mouse/tablet support
            void setMouseCallback(MouseCallback callback) { m_mouseCallback = callback; }
            void setScreenSize(QC::u32 width, QC::u32 height);

            bool hasTablet() const { return m_isTablet; }

        private:
            void reset();
            void initializeRings();
            void initializeScratchpad();
            void ringDoorbell(QC::u8 slot, QC::u8 endpoint);
            TRB *enqueueTRB(QC::u8 slot, QC::u8 endpoint, const TRB &trb);
            TRB *dequeueEvent();

            void probeDevices();

            QArch::PCIDevice *m_pciDevice;
            QC::VirtAddr m_mmioBase;

            volatile CapRegs *m_capRegs;
            volatile OpRegs *m_opRegs;
            volatile QC::u32 *m_doorbells;
            volatile PortRegs *m_portRegs;

            QC::u8 m_portCount;
            QC::u8 m_maxSlots;
            QC::u8 m_maxIntrs;

            QC::u64 *m_dcbaa; // Device Context Base Address Array
            TRB *m_commandRing;
            TRB *m_eventRing;
            QC::usize m_commandEnqueue;
            QC::usize m_eventDequeue;

            MouseCallback m_mouseCallback;
            QC::u32 m_screenWidth;
            QC::u32 m_screenHeight;
            bool m_isTablet;
        };

    } // namespace XHCI
} // namespace QKDrv
