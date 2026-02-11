// QFileSystem FAT32 - Implementation
// Namespace: QFS

#include "QFSFAT32.h"
#include "QFSFile.h"
#include "QFSDirectory.h"
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
        : m_device(device), m_fatStart(0), m_dataStart(0), m_clusterSize(0), m_clusterBuffer(nullptr)
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
        FAT32DirEntry *entry = findEntry(path);
        if (!entry)
        {
            if (mode & OpenMode::Create)
            {
                // TODO: Create new file
                return nullptr;
            }
            return nullptr;
        }

        if (!(mode & OpenMode::Read))
        {
            return nullptr;
        }

        if (entry->attributes & FAT32Attr::Directory)
        {
            return nullptr; // Can't open directory as file
        }

        QC::u32 firstCluster = entryCluster(*entry);
        if (firstCluster < 2)
        {
            return nullptr;
        }

        auto *handle = new FATFileHandle();
        handle->startCluster = firstCluster;
        handle->size = entry->size;

        File *file = new File();
        file->setFileSystem(this);
        file->setHandle(handle);
        file->setMode(mode);
        file->setSize(entry->size);
        file->setPosition(0);
        file->setOpen(true);
        return file;
    }

    QC::Status FAT32::close(File *file)
    {
        if (!file)
            return QC::Status::InvalidParam;
        auto *handle = static_cast<FATFileHandle *>(file->handle());
        if (handle)
        {
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
        // TODO: Implement FAT32 write
        return -1;
    }

    Directory *FAT32::openDir(const char *path)
    {
        FAT32DirEntry *entry = nullptr;

        if (path[0] == '/' && path[1] == '\0')
        {
            // Root directory
            Directory *dir = new Directory();
            // TODO: Set up directory handle
            return dir;
        }

        entry = findEntry(path);
        if (!entry || !(entry->attributes & FAT32Attr::Directory))
        {
            return nullptr;
        }

        Directory *dir = new Directory();
        // TODO: Set up directory handle
        return dir;
    }

    QC::Status FAT32::closeDir(Directory *dir)
    {
        if (!dir)
            return QC::Status::InvalidParam;
        delete dir;
        return QC::Status::Success;
    }

    bool FAT32::readDir(Directory *dir, DirEntry *entry)
    {
        // TODO: Implement directory reading
        return false;
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
        // TODO: Implement directory creation
        return QC::Status::NotSupported;
    }

    QC::Status FAT32::remove(const char *path)
    {
        // TODO: Implement file/directory removal
        return QC::Status::NotSupported;
    }

    FAT32DirEntry *FAT32::findEntry(const char *path, QC::u32 *parentCluster)
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
            if (!iterateDirectory(currentCluster, nameBuffer, &entry))
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
        // TODO: Find free cluster in FAT
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
