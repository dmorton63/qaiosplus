#pragma once

#include "QCTypes.h"

namespace QFS
{
    class VFS;
}

namespace QK::Boot::Tpm
{
    using FLogFn = void (*)(const char *);

    void TryTpm2CrbStartup(QC::u32 StartMethod, QC::PhysAddr ControlAreaPhys, FLogFn Log);

    bool IsReady();

    void RunSecureStoreSelfTests(QFS::VFS *Vfs, FLogFn Log);
}
