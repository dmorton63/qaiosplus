// QKMemory Virtual Memory Manager - Implementation
// Namespace: QK::Memory

#include "QKMemVMM.h"
#include "QKMemPMM.h"
#include "QKMemPaging.h"
#include "QKMemTranslator.h"
#include "QCLogger.h"
#include "QCString.h"

namespace QK::Memory
{

    VMM &VMM::instance()
    {
        static VMM instance;
        return instance;
    }

    VMM::VMM()
        : m_kernelPML4(0), m_nextVirtualAddress(0xFFFF900000000000ULL)
    {
    }

    VMM::~VMM()
    {
    }

    void VMM::initialize()
    {
        QC_LOG_INFO("QKMemVMM", "Initializing virtual memory manager");

        // Create kernel page tables
        m_kernelPML4 = PMM::instance().allocatePage();
        if (m_kernelPML4 == 0)
        {
            QC_LOG_FATAL("QKMemVMM", "Failed to allocate kernel PML4");
            return;
        }

        QC::String::memset(phys_to_virt<void>(m_kernelPML4), 0, PAGE_SIZE);

        QC_LOG_INFO("QKMemVMM", "VMM initialized, kernel PML4 at 0x%lx", m_kernelPML4);
    }

    QC::PhysAddr VMM::createAddressSpace()
    {
        QC::PhysAddr pml4 = PMM::instance().allocatePage();
        if (pml4 == 0)
            return 0;

        QC::String::memset(phys_to_virt<void>(pml4), 0, PAGE_SIZE);

        // Copy kernel mappings (upper half)
        QC::u64 *kernelTable = phys_to_virt<QC::u64>(m_kernelPML4);
        QC::u64 *newTable = phys_to_virt<QC::u64>(pml4);

        for (int i = 256; i < 512; ++i)
        {
            newTable[i] = kernelTable[i];
        }

        return pml4;
    }

    void VMM::destroyAddressSpace(QC::PhysAddr pml4)
    {
        if (pml4 == 0 || pml4 == m_kernelPML4)
            return;

        // TODO: Recursively free page tables (only user-space)
        PMM::instance().freePage(pml4);
    }

    void VMM::switchAddressSpace(QC::PhysAddr pml4)
    {
        asm volatile("mov %0, %%cr3" : : "r"(pml4) : "memory");
    }

    QC::PhysAddr VMM::currentAddressSpace() const
    {
        QC::PhysAddr cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));
        return cr3;
    }

    QC::Status VMM::map(QC::VirtAddr virt, QC::PhysAddr phys, PageFlags flags)
    {
        QC::PhysAddr pml4Addr = currentAddressSpace();
        QC::u64 *pml4 = phys_to_virt<QC::u64>(pml4Addr);

        // Get or create PDPT
        QC::u64 *pdpt = getOrCreateTable(pml4, pml4Index(virt));
        if (!pdpt)
            return QC::Status::OutOfMemory;

        // Get or create PD
        QC::u64 *pd = getOrCreateTable(pdpt, pdptIndex(virt));
        if (!pd)
            return QC::Status::OutOfMemory;

        // Get or create PT
        QC::u64 *pt = getOrCreateTable(pd, pdIndex(virt));
        if (!pt)
            return QC::Status::OutOfMemory;

        // Set the page table entry
        QC::u64 entry = phys | static_cast<QC::u64>(flags);
        pt[ptIndex(virt)] = entry;

        invalidatePage(virt);

        return QC::Status::Success;
    }

    QC::Status VMM::mapRange(QC::VirtAddr virt, QC::PhysAddr phys, QC::usize size, PageFlags flags)
    {
        QC::usize pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

        for (QC::usize i = 0; i < pages; ++i)
        {
            QC::Status status = map(virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, flags);
            if (status != QC::Status::Success)
            {
                // Rollback
                for (QC::usize j = 0; j < i; ++j)
                {
                    unmap(virt + j * PAGE_SIZE);
                }
                return status;
            }
        }

        return QC::Status::Success;
    }

    QC::Status VMM::unmap(QC::VirtAddr virt)
    {
        QC::PhysAddr pml4Addr = currentAddressSpace();
        QC::u64 *pml4 = phys_to_virt<QC::u64>(pml4Addr);

        QC::u64 pml4Entry = pml4[pml4Index(virt)];
        if (!(pml4Entry & 1))
            return QC::Status::NotFound;

        QC::u64 *pdpt = phys_to_virt<QC::u64>(pml4Entry & ~0xFFFULL);
        QC::u64 pdptEntry = pdpt[pdptIndex(virt)];
        if (!(pdptEntry & 1))
            return QC::Status::NotFound;

        QC::u64 *pd = phys_to_virt<QC::u64>(pdptEntry & ~0xFFFULL);
        QC::u64 pdEntry = pd[pdIndex(virt)];
        if (!(pdEntry & 1))
            return QC::Status::NotFound;

        QC::u64 *pt = phys_to_virt<QC::u64>(pdEntry & ~0xFFFULL);
        pt[ptIndex(virt)] = 0;

        invalidatePage(virt);

        return QC::Status::Success;
    }

    QC::Status VMM::unmapRange(QC::VirtAddr virt, QC::usize size)
    {
        QC::usize pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

        for (QC::usize i = 0; i < pages; ++i)
        {
            unmap(virt + i * PAGE_SIZE);
        }

        return QC::Status::Success;
    }

    QC::PhysAddr VMM::translate(QC::VirtAddr virt) const
    {
        QC::PhysAddr pml4Addr = currentAddressSpace();
        QC::u64 *pml4 = phys_to_virt<QC::u64>(pml4Addr);

        QC::u64 pml4Entry = pml4[pml4Index(virt)];
        if (!(pml4Entry & 1))
            return 0;

        QC::u64 *pdpt = phys_to_virt<QC::u64>(pml4Entry & ~0xFFFULL);
        QC::u64 pdptEntry = pdpt[pdptIndex(virt)];
        if (!(pdptEntry & 1))
            return 0;

        QC::u64 *pd = phys_to_virt<QC::u64>(pdptEntry & ~0xFFFULL);
        QC::u64 pdEntry = pd[pdIndex(virt)];
        if (!(pdEntry & 1))
            return 0;

        QC::u64 *pt = phys_to_virt<QC::u64>(pdEntry & ~0xFFFULL);
        QC::u64 ptEntry = pt[ptIndex(virt)];
        if (!(ptEntry & 1))
            return 0;

        return (ptEntry & ~0xFFFULL) | pageOffset(virt);
    }

    PageFlags VMM::getFlags(QC::VirtAddr virt) const
    {
        // TODO: Implement flag retrieval
        return PageFlags::None;
    }

    bool VMM::isMapped(QC::VirtAddr virt) const
    {
        return translate(virt) != 0;
    }

    QC::VirtAddr VMM::allocate(QC::usize size, PageFlags flags)
    {
        QC::usize pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        QC::VirtAddr addr = m_nextVirtualAddress;
        m_nextVirtualAddress += pages * PAGE_SIZE;

        for (QC::usize i = 0; i < pages; ++i)
        {
            QC::PhysAddr phys = PMM::instance().allocatePage();
            if (phys == 0)
            {
                // Rollback
                for (QC::usize j = 0; j < i; ++j)
                {
                    QC::PhysAddr p = translate(addr + j * PAGE_SIZE);
                    unmap(addr + j * PAGE_SIZE);
                    PMM::instance().freePage(p);
                }
                return 0;
            }
            map(addr + i * PAGE_SIZE, phys, flags);
        }

        return addr;
    }

    void VMM::free(QC::VirtAddr addr, QC::usize size)
    {
        QC::usize pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

        for (QC::usize i = 0; i < pages; ++i)
        {
            QC::PhysAddr phys = translate(addr + i * PAGE_SIZE);
            if (phys)
            {
                unmap(addr + i * PAGE_SIZE);
                PMM::instance().freePage(phys);
            }
        }
    }

    QC::u64 *VMM::getOrCreateTable(QC::u64 *parent, QC::usize index)
    {
        if (parent[index] & 1)
        {
            return phys_to_virt<QC::u64>(parent[index] & ~0xFFFULL);
        }

        QC::PhysAddr newTable = PMM::instance().allocatePage();
        if (newTable == 0)
            return nullptr;

        QC::String::memset(phys_to_virt<void>(newTable), 0, PAGE_SIZE);
        parent[index] = newTable | 0x03; // Present + Writable

        return phys_to_virt<QC::u64>(newTable);
    }

    void VMM::invalidatePage(QC::VirtAddr addr)
    {
        asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
    }

} // namespace QK::Memory
