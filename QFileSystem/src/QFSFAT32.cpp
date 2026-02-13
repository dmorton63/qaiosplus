// QFileSystem FAT32 - Implementation
// Namespace: QFS

#include "QFSFAT32.h"
#include "QFSFile.h"
#include "QFSDirectory.h"
#include "QFSPath.h"
#include "QKMemHeap.h"
#include "QCLogger.h"
#include "QCString.h"

namespace QFS
{

    // FAT32 special values
    constexpr QC::u32 FAT32_EOC = 0x0FFFFFF8;
    constexpr QC::u32 FAT32_BAD = 0x0FFFFFF7;
    constexpr QC::u32 FAT32_FREE = 0x00000000;

    FAT32::FAT32(BlockDevice *device)
        : m_device(device), m_fatStart(0), m_dataStart(0), m_clusterSize(0), m_totalClusters(0), m_clusterBuffer(nullptr)
    {
        QC::String::memset(&m_bootSector, 0, sizeof(m_bootSector));
    }

    FAT32::~FAT32()
    {
        unmount();
    }

    QC::Status FAT32::mount()
    {
        QC_LOG_INFO("QFSFAT32", "Mounting FAT32 filesystem");

        // Read boot sector into a temporary buffer, then copy the BPB portion
        QC::u8 sectorBuffer[512];
        QC::Status status = m_device->readSector(0, sectorBuffer);
        if (status != QC::Status::Success)
        {
            QC_LOG_ERROR("QFSFAT32", "Failed to read boot sector");
            return status;
        }

        QC::String::memcpy(&m_bootSector, sectorBuffer, sizeof(FAT32BootSector));

        // Validate FAT32
        if (m_bootSector.bytesPerSector != 512)
        {
            QC_LOG_ERROR("QFSFAT32", "Unsupported sector size: %u", m_bootSector.bytesPerSector);
            return QC::Status::NotSupported;
        }

        // Calculate layout
        m_fatStart = m_bootSector.reservedSectors;
        m_dataStart = m_fatStart + (m_bootSector.fatCount * m_bootSector.sectorsPerFat32);
        m_clusterSize = m_bootSector.bytesPerSector * m_bootSector.sectorsPerCluster;

        const QC::u32 totalSectors = (m_bootSector.totalSectors32 != 0) ? m_bootSector.totalSectors32 : m_bootSector.totalSectors16;
        if (totalSectors > m_dataStart)
        {
            const QC::u32 dataSectors = totalSectors - m_dataStart;
            m_totalClusters = dataSectors / m_bootSector.sectorsPerCluster;
        }

        // Allocate cluster buffer
        m_clusterBuffer = static_cast<QC::u8 *>(
            QK::Memory::Heap::instance().allocate(m_clusterSize));
        if (!m_clusterBuffer)
        {
            QC_LOG_ERROR("QFSFAT32", "Failed to allocate cluster buffer (%u bytes)", m_clusterSize);
            return QC::Status::OutOfMemory;
        }

        QC_LOG_INFO("QFSFAT32", "FAT32 mounted: %u bytes/cluster, root at cluster %u",
                    m_clusterSize, m_bootSector.rootCluster);

        return QC::Status::Success;
    }

    QC::Status FAT32::unmount()
    {
        if (m_clusterBuffer)
        {
            QK::Memory::Heap::instance().free(m_clusterBuffer);
            m_clusterBuffer = nullptr;
        }
        return QC::Status::Success;
    }

    QC::u32 FAT32::clusterToSector(QC::u32 cluster)
    {
        return m_dataStart + ((cluster - 2) * m_bootSector.sectorsPerCluster);
    }

    bool FAT32::loadCluster(QC::u32 cluster)
    {
        if (cluster < 2)
            return false;

        QC::u32 sector = clusterToSector(cluster);
        QC::usize count = m_bootSector.sectorsPerCluster;
        QC::Status status = m_device->readSectors(sector, count, m_clusterBuffer);
        return status == QC::Status::Success;
    }

    bool FAT32::storeCluster(QC::u32 cluster)
    {
        if (cluster < 2)
            return false;
        QC::u32 sector = clusterToSector(cluster);
        QC::usize count = m_bootSector.sectorsPerCluster;
        QC::Status status = m_device->writeSectors(sector, count, m_clusterBuffer);
        return status == QC::Status::Success;
    }

    QC::u32 FAT32::readFAT(QC::u32 cluster)
    {
        QC::u32 fatOffset = cluster * 4;
        QC::u32 fatSector = m_fatStart + (fatOffset / m_bootSector.bytesPerSector);
        QC::u32 entryOffset = fatOffset % m_bootSector.bytesPerSector;

        QC::u8 buffer[512];
        m_device->readSector(fatSector, buffer);

        return *reinterpret_cast<QC::u32 *>(&buffer[entryOffset]) & 0x0FFFFFFF;
    }

    void FAT32::writeFAT(QC::u32 cluster, QC::u32 value)
    {
        QC::u32 fatOffset = cluster * 4;
        QC::u32 fatSector = m_fatStart + (fatOffset / m_bootSector.bytesPerSector);
        QC::u32 entryOffset = fatOffset % m_bootSector.bytesPerSector;

        QC::u8 buffer[512];
        m_device->readSector(fatSector, buffer);

        *reinterpret_cast<QC::u32 *>(&buffer[entryOffset]) =
            (*reinterpret_cast<QC::u32 *>(&buffer[entryOffset]) & 0xF0000000) | (value & 0x0FFFFFFF);

        m_device->writeSector(fatSector, buffer);
    }

    QC::u32 FAT32::traverseToCluster(QC::u32 startCluster, QC::u32 index)
    {
        QC::u32 cluster = startCluster;
        for (QC::u32 i = 0; i < index; ++i)
        {
            cluster = readFAT(cluster);
            if (cluster >= FAT32_EOC)
                break;
        }
        return cluster;
    }

    QC::u32 FAT32::entryCluster(const FAT32DirEntry &entry) const
    {
        return (static_cast<QC::u32>(entry.clusterHigh) << 16) | entry.clusterLow;
    }

    bool FAT32::iterateDirectory(QC::u32 startCluster, const char *fatName, FAT32DirEntry *outEntry, QC::u32 *entryIndex)
    {
        if (!outEntry)
            return false;

        QC::u32 cluster = startCluster;
        QC::u32 index = 0;

        while (cluster >= 2 && cluster < FAT32_BAD)
        {
            if (!loadCluster(cluster))
                return false;

            QC::u32 entriesPerCluster = m_clusterSize / sizeof(FAT32DirEntry);
            FAT32DirEntry *entries = reinterpret_cast<FAT32DirEntry *>(m_clusterBuffer);

            for (QC::u32 i = 0; i < entriesPerCluster; ++i, ++index)
            {
                FAT32DirEntry &entry = entries[i];
                if (entry.name[0] == 0x00)
                    return false; // End of directory
                if (entry.name[0] == 0xE5)
                    continue; // Deleted
                if ((entry.attributes & FAT32Attr::LongName) == FAT32Attr::LongName)
                    continue;

                if (!fatName || QC::String::memcmp(entry.name, fatName, 11) == 0)
                {
                    QC::String::memcpy(outEntry, &entry, sizeof(FAT32DirEntry));
                    if (entryIndex)
                        *entryIndex = index;
                    return true;
                }
            }

            QC::u32 next = readFAT(cluster);
            if (next >= FAT32_EOC)
                break;
            cluster = next;
        }

        return false;
    }

    File *FAT32::open(const char *path, OpenMode mode)
    {
        if (!path || path[0] == '\0')
            return nullptr;

        if (!(mode & OpenMode::Read) && !(mode & OpenMode::Write))
            return nullptr;

        QC::u32 parentCluster = 0;
        QC::u32 entryIndex = 0;
        FAT32DirEntry *entry = findEntry(path, &parentCluster, &entryIndex);

        FAT32DirEntry createdEntry;
        if (!entry)
        {
            if (!(mode & OpenMode::Create))
                return nullptr;

            char parentPath[256];
            char baseName[256];
            Path::dirname(path, parentPath, sizeof(parentPath));
            Path::basename(path, baseName, sizeof(baseName));

            if (baseName[0] == '\0')
                return nullptr;

            QC::u32 dirCluster = m_bootSector.rootCluster;
            if (!(parentPath[0] == '/' && parentPath[1] == '\0'))
            {
                FAT32DirEntry *dirEntry = findEntry(parentPath);
                if (!dirEntry || !(dirEntry->attributes & FAT32Attr::Directory))
                    return nullptr;
                dirCluster = entryCluster(*dirEntry);
                if (dirCluster < 2)
                    return nullptr;
            }

            QC::u32 freeIndex = 0;
            if (!findFreeDirectoryEntry(dirCluster, &freeIndex))
                return nullptr;

            char fatName[11];
            formatName(baseName, fatName);
            QC::String::memset(&createdEntry, 0, sizeof(createdEntry));
            QC::String::memcpy(createdEntry.name, fatName, 11);
            createdEntry.attributes = FAT32Attr::Archive;
            createdEntry.clusterHigh = 0;
            createdEntry.clusterLow = 0;
            createdEntry.size = 0;

            if (!updateDirectoryEntry(dirCluster, freeIndex, createdEntry))
                return nullptr;

            parentCluster = dirCluster;
            entryIndex = freeIndex;
            m_entryCache = createdEntry;
            entry = &m_entryCache;
        }

        if (entry->attributes & FAT32Attr::Directory)
            return nullptr;

        QC::u32 firstCluster = entryCluster(*entry);

        auto *handle = new FATFileHandle();
        handle->startCluster = firstCluster;
        handle->size = entry->size;
        handle->dirCluster = parentCluster;
        handle->dirEntryIndex = entryIndex;
        handle->dirty = false;

        File *file = new File();
        file->setFileSystem(this);
        file->setHandle(handle);
        file->setMode(mode);
        file->setSize(entry->size);
        file->setPosition(0);
        file->setOpen(true);

        if ((mode & OpenMode::Truncate) && (mode & OpenMode::Write))
        {
            if (handle->startCluster >= 2)
                freeClusterChain(handle->startCluster);
            handle->startCluster = 0;
            handle->size = 0;
            handle->dirty = true;
            file->setSize(0);
            file->setPosition(0);
        }

        if (mode & OpenMode::Append)
        {
            file->setPosition(file->size());
        }

        return file;
    }

    QC::Status FAT32::close(File *file)
    {
        if (!file)
            return QC::Status::InvalidParam;
        auto *handle = static_cast<FATFileHandle *>(file->handle());
        if (handle)
        {
            if (handle->dirty)
            {
                FAT32DirEntry entryOnDisk;
                QC::String::memset(&entryOnDisk, 0, sizeof(entryOnDisk));
                if (loadCluster(traverseToCluster(handle->dirCluster, handle->dirEntryIndex / (m_clusterSize / sizeof(FAT32DirEntry)))))
                {
                    // Read the entry, update fields, then write it back.
                    const QC::u32 entriesPerCluster = m_clusterSize / sizeof(FAT32DirEntry);
                    const QC::u32 indexWithinCluster = handle->dirEntryIndex % entriesPerCluster;
                    auto *entries = reinterpret_cast<FAT32DirEntry *>(m_clusterBuffer);
                    entryOnDisk = entries[indexWithinCluster];
                    entryOnDisk.size = static_cast<QC::u32>(handle->size);
                    entryOnDisk.clusterHigh = static_cast<QC::u16>((handle->startCluster >> 16) & 0xFFFF);
                    entryOnDisk.clusterLow = static_cast<QC::u16>(handle->startCluster & 0xFFFF);
                    updateDirectoryEntry(handle->dirCluster, handle->dirEntryIndex, entryOnDisk);
                }
            }
            delete handle;
            file->setHandle(nullptr);
        }
        file->setOpen(false);
        file->setFileSystem(nullptr);
        return QC::Status::Success;
    }

    QC::isize FAT32::read(File *file, void *buffer, QC::usize size)
    {
        if (!file || !buffer || size == 0)
            return 0;

        auto *handle = static_cast<FATFileHandle *>(file->handle());
        if (!handle)
            return -1;

        QC::u64 position = file->tell();
        if (position >= handle->size)
            return 0;

        QC::usize remaining = static_cast<QC::usize>(handle->size - position);
        if (size > remaining)
            size = remaining;

        QC::u8 *dst = static_cast<QC::u8 *>(buffer);
        QC::usize totalRead = 0;

        while (size > 0)
        {
            QC::u32 clusterSize = m_clusterSize;
            QC::u32 clusterIndex = static_cast<QC::u32>(position / clusterSize);
            QC::u32 offset = static_cast<QC::u32>(position % clusterSize);

            QC::u32 cluster = traverseToCluster(handle->startCluster, clusterIndex);
            if (cluster < 2 || cluster >= FAT32_EOC)
                break;

            if (!loadCluster(cluster))
                break;

            QC::usize chunk = clusterSize - offset;
            if (chunk > size)
                chunk = size;

            QC::String::memcpy(dst + totalRead, m_clusterBuffer + offset, chunk);

            size -= chunk;
            totalRead += chunk;
            position += chunk;
        }

        return static_cast<QC::isize>(totalRead);
    }

    QC::isize FAT32::write(File *file, const void *buffer, QC::usize size)
    {
        if (!file || !buffer || size == 0)
            return 0;

        auto *handle = static_cast<FATFileHandle *>(file->handle());
        if (!handle)
            return -1;

        QC::u64 position = file->tell();
        if (position > handle->size)
        {
            // Minimal implementation: do not support sparse writes yet.
            return -1;
        }

        const QC::u8 *src = static_cast<const QC::u8 *>(buffer);
        QC::usize totalWritten = 0;

        auto getOrAllocateClusterAt = [&](QC::u32 clusterIndex) -> QC::u32
        {
            if (handle->startCluster < 2)
            {
                QC::u32 first = allocateCluster();
                if (first < 2)
                    return 0;
                handle->startCluster = first;
                handle->dirty = true;
            }

            QC::u32 cluster = handle->startCluster;
            for (QC::u32 i = 0; i < clusterIndex; ++i)
            {
                QC::u32 next = readFAT(cluster);
                if (next >= FAT32_EOC)
                {
                    QC::u32 newCluster = allocateCluster();
                    if (newCluster < 2)
                        return 0;
                    writeFAT(cluster, newCluster);
                    cluster = newCluster;
                    handle->dirty = true;
                    continue;
                }
                cluster = next;
            }
            return cluster;
        };

        while (size > 0)
        {
            const QC::u32 clusterSize = m_clusterSize;
            const QC::u32 clusterIndex = static_cast<QC::u32>(position / clusterSize);
            const QC::u32 offset = static_cast<QC::u32>(position % clusterSize);

            QC::u32 cluster = getOrAllocateClusterAt(clusterIndex);
            if (cluster < 2)
                break;

            if (!loadCluster(cluster))
                break;

            QC::usize chunk = clusterSize - offset;
            if (chunk > size)
                chunk = size;

            QC::String::memcpy(m_clusterBuffer + offset, src + totalWritten, chunk);
            if (!storeCluster(cluster))
                break;

            size -= chunk;
            totalWritten += chunk;
            position += chunk;
        }

        if (totalWritten > 0)
        {
            if (position > handle->size)
            {
                handle->size = position;
                handle->dirty = true;
            }
        }

        return static_cast<QC::isize>(totalWritten);
    }

    Directory *FAT32::openDir(const char *path)
    {
        if (!path)
            return nullptr;

        QC::u32 startCluster = 0;
        if (path[0] == '/' && path[1] == '\0')
        {
            startCluster = m_bootSector.rootCluster;
        }
        else
        {
            FAT32DirEntry *entry = findEntry(path);
            if (!entry || !(entry->attributes & FAT32Attr::Directory))
                return nullptr;
            startCluster = entryCluster(*entry);
        }

        if (startCluster < 2)
            return nullptr;

        auto *handle = new FATDirHandle();
        handle->startCluster = startCluster;
        handle->currentCluster = startCluster;
        handle->entryIndex = 0;

        Directory *dir = new Directory();
        dir->setFileSystem(this);
        dir->setHandle(handle);
        dir->setOpen(true);
        return dir;
    }

    QC::Status FAT32::closeDir(Directory *dir)
    {
        if (!dir)
            return QC::Status::InvalidParam;

        auto *handle = static_cast<FATDirHandle *>(dir->handle());
        if (handle)
        {
            delete handle;
            dir->setHandle(nullptr);
        }
        dir->setOpen(false);
        dir->setFileSystem(nullptr);
        return QC::Status::Success;
    }

    bool FAT32::readDir(Directory *dir, DirEntry *entry)
    {
        if (!dir || !entry)
            return false;

        auto *handle = static_cast<FATDirHandle *>(dir->handle());
        if (!handle)
            return false;

        while (handle->currentCluster >= 2 && handle->currentCluster < FAT32_BAD)
        {
            if (!loadCluster(handle->currentCluster))
                return false;

            const QC::u32 entriesPerCluster = m_clusterSize / sizeof(FAT32DirEntry);
            auto *entries = reinterpret_cast<FAT32DirEntry *>(m_clusterBuffer);

            while (handle->entryIndex < entriesPerCluster)
            {
                FAT32DirEntry &e = entries[handle->entryIndex++];
                if (e.name[0] == 0x00)
                    return false;
                if (e.name[0] == 0xE5)
                    continue;
                if ((e.attributes & FAT32Attr::LongName) == FAT32Attr::LongName)
                    continue;
                if (e.attributes & FAT32Attr::VolumeId)
                    continue;
                if (e.name[0] == '.')
                    continue;

                parseName(e.name, entry->name);
                entry->type = (e.attributes & FAT32Attr::Directory) ? FileType::Directory : FileType::Regular;
                entry->size = e.size;
                return true;
            }

            QC::u32 next = readFAT(handle->currentCluster);
            if (next >= FAT32_EOC)
                break;
            handle->currentCluster = next;
            handle->entryIndex = 0;
        }

        return false;
    }

    void FAT32::rewindDir(Directory *dir)
    {
        if (!dir)
            return;
        auto *handle = static_cast<FATDirHandle *>(dir->handle());
        if (!handle)
            return;
        handle->currentCluster = handle->startCluster;
        handle->entryIndex = 0;
    }

    QC::Status FAT32::stat(const char *path, FileInfo *info)
    {
        FAT32DirEntry *entry = findEntry(path);
        if (!entry)
            return QC::Status::NotFound;

        parseName(entry->name, info->name);
        info->type = (entry->attributes & FAT32Attr::Directory) ? FileType::Directory : FileType::Regular;
        info->size = entry->size;
        // TODO: Convert FAT32 time format to Unix timestamp
        info->createdTime = 0;
        info->modifiedTime = 0;
        info->accessedTime = 0;
        info->permissions = 0644;
        info->uid = 0;
        info->gid = 0;

        return QC::Status::Success;
    }

    QC::Status FAT32::createDir(const char *path)
    {
        if (!path || path[0] != '/')
            return QC::Status::InvalidParam;

        if (findEntry(path))
            return QC::Status::Error;

        char parentPath[256];
        char baseName[256];
        Path::dirname(path, parentPath, sizeof(parentPath));
        Path::basename(path, baseName, sizeof(baseName));

        if (baseName[0] == '\0')
            return QC::Status::InvalidParam;

        QC::u32 parentCluster = m_bootSector.rootCluster;
        if (!(parentPath[0] == '/' && parentPath[1] == '\0'))
        {
            FAT32DirEntry *parentEntry = findEntry(parentPath);
            if (!parentEntry || !(parentEntry->attributes & FAT32Attr::Directory))
                return QC::Status::NotFound;
            parentCluster = entryCluster(*parentEntry);
            if (parentCluster < 2)
                return QC::Status::NotFound;
        }

        QC::u32 freeIndex = 0;
        if (!findFreeDirectoryEntry(parentCluster, &freeIndex))
            return QC::Status::OutOfMemory;

        QC::u32 newCluster = allocateCluster();
        if (newCluster < 2)
            return QC::Status::OutOfMemory;

        // Initialize directory cluster: "." and ".." entries
        QC::String::memset(m_clusterBuffer, 0, m_clusterSize);
        auto *ents = reinterpret_cast<FAT32DirEntry *>(m_clusterBuffer);

        FAT32DirEntry dot;
        QC::String::memset(&dot, 0, sizeof(dot));
        QC::String::memset(dot.name, ' ', 11);
        dot.name[0] = '.';
        dot.attributes = FAT32Attr::Directory;
        dot.clusterHigh = static_cast<QC::u16>((newCluster >> 16) & 0xFFFF);
        dot.clusterLow = static_cast<QC::u16>(newCluster & 0xFFFF);
        dot.size = 0;
        ents[0] = dot;

        FAT32DirEntry dotdot;
        QC::String::memset(&dotdot, 0, sizeof(dotdot));
        QC::String::memset(dotdot.name, ' ', 11);
        dotdot.name[0] = '.';
        dotdot.name[1] = '.';
        dotdot.attributes = FAT32Attr::Directory;
        dotdot.clusterHigh = static_cast<QC::u16>((parentCluster >> 16) & 0xFFFF);
        dotdot.clusterLow = static_cast<QC::u16>(parentCluster & 0xFFFF);
        dotdot.size = 0;
        ents[1] = dotdot;

        if (!storeCluster(newCluster))
            return QC::Status::Error;

        FAT32DirEntry newEntry;
        QC::String::memset(&newEntry, 0, sizeof(newEntry));
        char fatName[11];
        formatName(baseName, fatName);
        QC::String::memcpy(newEntry.name, fatName, 11);
        newEntry.attributes = FAT32Attr::Directory;
        newEntry.clusterHigh = static_cast<QC::u16>((newCluster >> 16) & 0xFFFF);
        newEntry.clusterLow = static_cast<QC::u16>(newCluster & 0xFFFF);
        newEntry.size = 0;

        if (!updateDirectoryEntry(parentCluster, freeIndex, newEntry))
            return QC::Status::Error;

        return QC::Status::Success;
    }

    QC::Status FAT32::remove(const char *path)
    {
        if (!path || path[0] != '/')
            return QC::Status::InvalidParam;

        QC::u32 parentCluster = 0;
        QC::u32 entryIndex = 0;
        FAT32DirEntry *entry = findEntry(path, &parentCluster, &entryIndex);
        if (!entry)
            return QC::Status::NotFound;

        const bool isDir = (entry->attributes & FAT32Attr::Directory) != 0;
        const QC::u32 startCluster = entryCluster(*entry);

        if (isDir)
        {
            // Only allow removing empty directories for now.
            auto *dir = openDir(path);
            if (!dir)
                return QC::Status::NotSupported;
            DirEntry tmp;
            bool hasEntries = false;
            while (readDir(dir, &tmp))
            {
                hasEntries = true;
                break;
            }
            closeDir(dir);
            delete dir;
            if (hasEntries)
                return QC::Status::NotSupported;
        }

        FAT32DirEntry deleted = *entry;
        deleted.name[0] = static_cast<char>(0xE5);
        if (!updateDirectoryEntry(parentCluster, entryIndex, deleted))
            return QC::Status::Error;

        if (startCluster >= 2)
            freeClusterChain(startCluster);

        return QC::Status::Success;
    }

    FAT32DirEntry *FAT32::findEntry(const char *path, QC::u32 *parentCluster, QC::u32 *entryIndex)
    {
        if (!path || path[0] == '\0')
            return nullptr;

        if (path[0] != '/')
            return nullptr;

        QC::u32 currentCluster = m_bootSector.rootCluster;
        if (parentCluster)
            *parentCluster = currentCluster;

        // Skip leading '/'
        const char *segment = path;
        while (*segment == '/')
            ++segment;

        if (*segment == '\0')
        {
            return nullptr; // Root directory has no direct entry
        }

        char nameBuffer[12];

        while (*segment)
        {
            const char *start = segment;
            while (*segment && *segment != '/')
                ++segment;

            QC::usize len = static_cast<QC::usize>(segment - start);
            if (len == 0)
                break;

            char element[64];
            if (len >= sizeof(element))
                len = sizeof(element) - 1;
            for (QC::usize i = 0; i < len; ++i)
            {
                char c = start[i];
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char>(c - 32);
                element[i] = c;
            }
            element[len] = '\0';

            formatName(element, nameBuffer);

            FAT32DirEntry entry;
            QC::u32 foundIndex = 0;
            if (!iterateDirectory(currentCluster, nameBuffer, &entry, &foundIndex))
            {
                return nullptr;
            }

            // Skip consecutive separators
            while (*segment == '/')
                ++segment;

            if (*segment == '\0')
            {
                if (parentCluster)
                    *parentCluster = currentCluster;
                if (entryIndex)
                    *entryIndex = foundIndex;
                m_entryCache = entry;
                return &m_entryCache;
            }

            if (!(entry.attributes & FAT32Attr::Directory))
            {
                return nullptr;
            }

            QC::u32 nextCluster = entryCluster(entry);
            if (nextCluster < 2)
            {
                return nullptr;
            }
            currentCluster = nextCluster;
        }

        return nullptr;
    }

    bool FAT32::updateDirectoryEntry(QC::u32 dirStartCluster, QC::u32 entryIndex, const FAT32DirEntry &entry)
    {
        if (dirStartCluster < 2)
            return false;

        const QC::u32 entriesPerCluster = m_clusterSize / sizeof(FAT32DirEntry);
        const QC::u32 clusterOffset = entryIndex / entriesPerCluster;
        const QC::u32 indexWithinCluster = entryIndex % entriesPerCluster;
        const QC::u32 cluster = traverseToCluster(dirStartCluster, clusterOffset);
        if (cluster < 2 || cluster >= FAT32_BAD)
            return false;

        if (!loadCluster(cluster))
            return false;
        auto *entries = reinterpret_cast<FAT32DirEntry *>(m_clusterBuffer);
        entries[indexWithinCluster] = entry;
        return storeCluster(cluster);
    }

    bool FAT32::findFreeDirectoryEntry(QC::u32 dirStartCluster, QC::u32 *outEntryIndex)
    {
        if (!outEntryIndex || dirStartCluster < 2)
            return false;

        const QC::u32 entriesPerCluster = m_clusterSize / sizeof(FAT32DirEntry);
        QC::u32 cluster = dirStartCluster;
        QC::u32 baseIndex = 0;

        while (cluster >= 2 && cluster < FAT32_BAD)
        {
            if (!loadCluster(cluster))
                return false;

            auto *entries = reinterpret_cast<FAT32DirEntry *>(m_clusterBuffer);
            for (QC::u32 i = 0; i < entriesPerCluster; ++i)
            {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5)
                {
                    *outEntryIndex = baseIndex + i;
                    return true;
                }
            }

            QC::u32 next = readFAT(cluster);
            if (next >= FAT32_EOC)
                break;
            cluster = next;
            baseIndex += entriesPerCluster;
        }

        // No free slots; grow the directory by allocating a new cluster.
        QC::u32 newCluster = allocateCluster();
        if (newCluster < 2)
            return false;

        // Link it.
        if (cluster >= 2 && cluster < FAT32_BAD)
        {
            writeFAT(cluster, newCluster);
        }

        // New cluster is empty, first entry is free.
        *outEntryIndex = baseIndex;
        return true;
    }

    void FAT32::parseName(const char *fatName, char *outName)
    {
        int i = 0, j = 0;

        // Copy name part (first 8 chars)
        for (i = 0; i < 8 && fatName[i] != ' '; ++i)
        {
            outName[j++] = fatName[i];
        }

        // Add extension if present
        if (fatName[8] != ' ')
        {
            outName[j++] = '.';
            for (i = 8; i < 11 && fatName[i] != ' '; ++i)
            {
                outName[j++] = fatName[i];
            }
        }

        outName[j] = '\0';
    }

    void FAT32::formatName(const char *name, char *fatName)
    {
        QC::String::memset(fatName, ' ', 11);

        int i = 0, j = 0;

        // Copy name until dot or end
        while (name[i] && name[i] != '.' && j < 8)
        {
            fatName[j++] = (name[i] >= 'a' && name[i] <= 'z') ? (name[i] - 32) : name[i];
            ++i;
        }

        // Skip to extension
        while (name[i] && name[i] != '.')
            ++i;
        if (name[i] == '.')
            ++i;

        // Copy extension
        j = 8;
        while (name[i] && j < 11)
        {
            fatName[j++] = (name[i] >= 'a' && name[i] <= 'z') ? (name[i] - 32) : name[i];
            ++i;
        }
    }

    QC::u32 FAT32::allocateCluster()
    {
        if (m_totalClusters == 0)
            return 0;

        // Scan FAT for a free cluster.
        for (QC::u32 cluster = 2; cluster < m_totalClusters + 2; ++cluster)
        {
            if (readFAT(cluster) == FAT32_FREE)
            {
                writeFAT(cluster, 0x0FFFFFFF);
                // Zero out the cluster.
                QC::String::memset(m_clusterBuffer, 0, m_clusterSize);
                storeCluster(cluster);
                return cluster;
            }
        }

        return 0;
    }

    void FAT32::freeClusterChain(QC::u32 startCluster)
    {
        QC::u32 cluster = startCluster;
        while (cluster >= 2 && cluster < FAT32_BAD)
        {
            QC::u32 next = readFAT(cluster);
            writeFAT(cluster, FAT32_FREE);
            cluster = next;
        }
    }

} // namespace QFS
