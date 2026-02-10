// QKDrvXHCI - USB 3.0 xHCI Controller implementation
// Namespace: QKDrv::XHCI

#include "QKDrvXHCI.h"
#include "QKMemTranslator.h"
#include "QCLogger.h"
#include "QCString.h"

// Early page allocator (defined in QKMain.cpp, identity-mapped)
extern "C" QC::PhysAddr earlyAllocatePage();

namespace QKDrv
{
    namespace XHCI
    {

        // Helper: allocate a DMA page
        static QC::PhysAddr allocateDMAPage()
        {
            return earlyAllocatePage();
        }

        // xHCI command bits
        constexpr QC::u32 CMD_RUN = 1 << 0;
        constexpr QC::u32 CMD_HCRST = 1 << 1;
        constexpr QC::u32 CMD_INTE = 1 << 2;

        // xHCI status bits
        constexpr QC::u32 STS_HCH = 1 << 0;
        constexpr QC::u32 STS_CNR = 1 << 11;

        // Port status bits
        constexpr QC::u32 PORTSC_CCS = 1 << 0;
        constexpr QC::u32 PORTSC_PED = 1 << 1;
        constexpr QC::u32 PORTSC_PR = 1 << 4;

        Controller *Controller::probe(QArch::PCIDevice *pciDevice)
        {
            // Check for xHCI: class 0x0C, subclass 0x03, progIF 0x30
            if (pciDevice->classCode == QArch::PCIClass::SerialBus &&
                pciDevice->subclass == 0x03 &&
                pciDevice->progIF == 0x30)
            {
                return new Controller(pciDevice);
            }
            return nullptr;
        }

        Controller::Controller(QArch::PCIDevice *pciDevice)
            : m_pciDevice(pciDevice), m_mmioBase(0), m_capRegs(nullptr), m_opRegs(nullptr), m_doorbells(nullptr), m_portRegs(nullptr), m_portCount(0), m_maxSlots(0), m_maxIntrs(0), m_dcbaa(nullptr), m_commandRing(nullptr), m_eventRing(nullptr), m_commandEnqueue(0), m_eventDequeue(0), m_mouseCallback(nullptr), m_screenWidth(1024), m_screenHeight(768), m_isTablet(false)
        {
        }

        Controller::~Controller()
        {
            shutdown();
        }

        QC::Status Controller::initialize()
        {
            QC_LOG_INFO("xHCI", "Initializing xHCI controller");

            // Enable bus mastering and memory space
            QArch::PCI::instance().enableBusMastering(m_pciDevice->address);
            QArch::PCI::instance().enableMemorySpace(m_pciDevice->address);

            // Map MMIO
            QC::PhysAddr barAddr = m_pciDevice->bar[0];
            m_mmioBase = QK::Memory::Translator::instance().mapMMIO(barAddr, 0x10000);

            // Set up register pointers
            m_capRegs = reinterpret_cast<volatile CapRegs *>(m_mmioBase);
            m_opRegs = reinterpret_cast<volatile OpRegs *>(m_mmioBase + m_capRegs->capLength);
            m_doorbells = reinterpret_cast<volatile QC::u32 *>(m_mmioBase + m_capRegs->dbOffset);
            m_portRegs = reinterpret_cast<volatile PortRegs *>(
                m_mmioBase + m_capRegs->capLength + 0x400);

            // Get controller parameters
            m_maxSlots = m_capRegs->hcsParams1 & 0xFF;
            m_portCount = (m_capRegs->hcsParams1 >> 24) & 0xFF;
            m_maxIntrs = (m_capRegs->hcsParams1 >> 8) & 0x7FF;

            QC_LOG_INFO("xHCI", "xHCI: %u ports, %u slots, version %x.%x",
                        m_portCount, m_maxSlots,
                        (m_capRegs->hciVersion >> 8) & 0xFF,
                        m_capRegs->hciVersion & 0xFF);

            // Reset controller
            reset();

            // Initialize data structures
            initializeRings();
            initializeScratchpad();

            // Configure and start
            m_opRegs->config = m_maxSlots;
            m_opRegs->usbcmd |= CMD_RUN | CMD_INTE;

            // Wait for controller to start
            while (m_opRegs->usbsts & STS_HCH)
                ;

            QC_LOG_INFO("xHCI", "xHCI controller initialized and running");

            // Probe for devices
            probeDevices();

            return QC::Status::Success;
        }

        void Controller::shutdown()
        {
            if (m_opRegs)
            {
                m_opRegs->usbcmd &= ~CMD_RUN;
                while (!(m_opRegs->usbsts & STS_HCH))
                    ;
            }
            QC_LOG_INFO("xHCI", "xHCI controller shutdown");
        }

        void Controller::reset()
        {
            m_opRegs->usbcmd |= CMD_HCRST;
            while (m_opRegs->usbcmd & CMD_HCRST)
                ;
            while (m_opRegs->usbsts & STS_CNR)
                ;
        }

        void Controller::initializeRings()
        {
            // Allocate DCBAA (identity-mapped, so phys == virt)
            QC::PhysAddr dcbaaPhys = allocateDMAPage();
            m_dcbaa = reinterpret_cast<QC::u64 *>(dcbaaPhys);
            QC::String::memset(m_dcbaa, 0, 4096);
            m_opRegs->dcbaap = dcbaaPhys;

            // Allocate command ring
            QC::PhysAddr cmdRingPhys = allocateDMAPage();
            m_commandRing = reinterpret_cast<TRB *>(cmdRingPhys);
            QC::String::memset(m_commandRing, 0, 4096);
            m_opRegs->crcr = cmdRingPhys | 1; // Ring cycle state = 1

            // TODO: Allocate event ring and interrupter
        }

        void Controller::initializeScratchpad()
        {
            // TODO: Allocate scratchpad buffers if required
        }

        void Controller::poll()
        {
            // Check for port status changes
            for (QC::u8 port = 0; port < m_portCount; ++port)
            {
                // TODO: Handle port status change events
            }

            // TODO: Process event ring
        }

        void Controller::probeDevices()
        {
            // Check each port for connected devices
            for (QC::u8 port = 0; port < m_portCount; ++port)
            {
                if (isPortConnected(port))
                {
                    Speed speed = getPortSpeed(port);
                    QC_LOG_INFO("xHCI", "Device on port %u, speed %u", port, static_cast<QC::u8>(speed));
                    // TODO: Enumerate device and check if it's a tablet
                }
            }
        }

        void Controller::setScreenSize(QC::u32 width, QC::u32 height)
        {
            m_screenWidth = width;
            m_screenHeight = height;
        }

        bool Controller::isPortConnected(QC::u8 port) const
        {
            if (port >= m_portCount)
                return false;
            return (m_portRegs[port].portsc & PORTSC_CCS) != 0;
        }

        Speed Controller::getPortSpeed(QC::u8 port) const
        {
            if (port >= m_portCount)
                return Speed::Full;

            QC::u32 speed = (m_portRegs[port].portsc >> 10) & 0xF;
            switch (speed)
            {
            case 1:
                return Speed::Full;
            case 2:
                return Speed::Low;
            case 3:
                return Speed::High;
            case 4:
                return Speed::Super;
            case 5:
                return Speed::SuperPlus;
            default:
                return Speed::Full;
            }
        }

        void Controller::resetPort(QC::u8 port)
        {
            if (port >= m_portCount)
                return;

            m_portRegs[port].portsc |= PORTSC_PR;

            // Wait for reset to complete
            while (m_portRegs[port].portsc & PORTSC_PR)
                ;
        }

        void Controller::ringDoorbell(QC::u8 slot, QC::u8 endpoint)
        {
            m_doorbells[slot] = endpoint;
        }

        TRB *Controller::enqueueTRB(QC::u8 slot, QC::u8 endpoint, const TRB &trb)
        {
            // TODO: Implement TRB enqueueing
            (void)slot;
            (void)endpoint;
            (void)trb;
            return nullptr;
        }

        TRB *Controller::dequeueEvent()
        {
            // TODO: Implement event dequeue
            return nullptr;
        }

    } // namespace XHCI
} // namespace QKDrv
