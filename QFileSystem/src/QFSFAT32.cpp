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

        // Read boot sector
        QC::Status status = m_device->readSector(0, &m_bootSector);
        if (status != QC::Status::Success)
        {
            QC_LOG_ERROR("QFSFAT32", "Failed to read boot sector");
            return status;
        }

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

        if (entry->attributes & FAT32Attr::Directory)
        {
            return nullptr; // Can't open directory as file
        }

        File *file = new File();
        // TODO: Set up file handle with cluster info
        return file;
    }

    QC::Status FAT32::close(File *file)
    {
        if (!file)
            return QC::Status::InvalidParam;
        delete file;
        return QC::Status::Success;
    }

    QC::isize FAT32::read(File *file, void *buffer, QC::usize size)
    {
        // TODO: Implement FAT32 read
        return -1;
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
        // TODO: Implement path traversal
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
