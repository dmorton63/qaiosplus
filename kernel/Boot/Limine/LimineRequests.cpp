#include "LimineRequests.h"

namespace
{
    template <typename T>
    const T *GetResponse(QC::u64 Request[])
    {
        if (!Request)
        {
            return nullptr;
        }

        return reinterpret_cast<const T *>(Request[5]);
    }
}

namespace QK::Boot::Limine
{
    const limine_hhdm_response *GetHhdmResponse(QC::u64 HhdmRequest[])
    {
        return GetResponse<limine_hhdm_response>(HhdmRequest);
    }

    const FKernelAddressResponse *GetKernelAddressResponse(QC::u64 KernelAddressRequest[])
    {
        return GetResponse<FKernelAddressResponse>(KernelAddressRequest);
    }

    const limine_firmware_type_response *GetFirmwareTypeResponse(QC::u64 FirmwareTypeRequest[])
    {
        return GetResponse<limine_firmware_type_response>(FirmwareTypeRequest);
    }

    const limine_rsdp_response *GetRsdpResponse(QC::u64 RsdpRequest[])
    {
        return GetResponse<limine_rsdp_response>(RsdpRequest);
    }

    bool ReadKernelMapping(QC::u64 HhdmRequest[], QC::u64 KernelAddressRequest[], FKernelMapping &OutMapping)
    {
        const limine_hhdm_response *Hhdm = GetHhdmResponse(HhdmRequest);
        if (!Hhdm)
        {
            return false;
        }

        const FKernelAddressResponse *KernelAddr = GetKernelAddressResponse(KernelAddressRequest);
        if (!KernelAddr)
        {
            return false;
        }

        OutMapping.HhdmOffset = Hhdm->offset;
        OutMapping.KernelPhysBase = KernelAddr->physical_base;
        OutMapping.KernelVirtBase = KernelAddr->virtual_base;
        return true;
    }
}
