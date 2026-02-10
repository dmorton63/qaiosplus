// QKMemory Address Translator - Implementation
// Namespace: QK::Memory

#include "QKMemTranslator.h"
#include "QKMemPMM.h"
#include "QKMemVMM.h"
#include "QCLogger.h"

namespace QK::Memory
{

    Translator &Translator::instance()
    {
        static Translator instance;
        return instance;
    }

    Translator::Translator()
        : m_physicalBase(0), m_mmioBase(0xFFFFE00000000000ULL), m_useIdentityMapping(true)
    {
    }

    Translator::~Translator()
    {
    }

    void Translator::initialize(QC::VirtAddr physicalBase)
    {
        m_physicalBase = physicalBase;
        m_useIdentityMapping = false; // VMM is ready
        QC_LOG_INFO("QKMemTrans", "Address translator initialized, phys base: 0x%lx", physicalBase);
    }

    QC::PhysAddr Translator::virtToPhys(QC::VirtAddr virt) const
    {
        if (m_useIdentityMapping)
        {
            // With identity mapping, virt == phys for lower addresses
            return static_cast<QC::PhysAddr>(virt);
        }

        if (isHigherHalf(virt))
        {
            // Direct mapping in higher half
            return virt - m_physicalBase;
        }

        // Use page tables for other addresses
        return VMM::instance().translate(virt);
    }

    bool Translator::isIdentityMapped(QC::VirtAddr addr) const
    {
        return m_useIdentityMapping || addr < 0x100000000ULL; // First 4GB
    }

    bool Translator::isHigherHalf(QC::VirtAddr addr) const
    {
        return addr >= KERNEL_OFFSET;
    }

    QC::VirtAddr Translator::mapMMIO(QC::PhysAddr phys, QC::usize size)
    {
        if (m_useIdentityMapping)
        {
            // Limine provides identity mapping - just return physical address
            QC_LOG_DEBUG("QKMemTrans", "MMIO identity map: phys=0x%lx, size=%lu", phys, size);
            return static_cast<QC::VirtAddr>(phys);
        }

        // Use VMM for proper MMIO mapping
        QC::VirtAddr virt = m_mmioBase;
        m_mmioBase += (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        VMM::instance().mapRange(virt, phys, size,
                                 PageFlags::Present | PageFlags::Writable | PageFlags::NoCache);

        QC_LOG_DEBUG("QKMemTrans", "Mapped MMIO: phys=0x%lx -> virt=0x%lx, size=%lu",
                     phys, virt, size);

        return virt;
    }

    void Translator::unmapMMIO(QC::VirtAddr virt, QC::usize size)
    {
        if (m_useIdentityMapping)
        {
            // Nothing to do for identity-mapped MMIO
            return;
        }
        VMM::instance().unmapRange(virt, size);
    }

} // namespace QK::Memory
