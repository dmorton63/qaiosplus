// QFileSystem Directory - Implementation
// Namespace: QFS

#include "QFSDirectory.h"
#include "QFSFAT32.h"

namespace QFS
{

    Directory::Directory()
        : m_open(false), m_fs(nullptr), m_fsHandle(nullptr)
    {
    }

    Directory::~Directory()
    {
        if (m_open && m_fs)
        {
            m_fs->closeDir(this);
        }
    }

    bool Directory::read(DirEntry *entry)
    {
        if (!m_open || !m_fs || !entry)
        {
            return false;
        }

        return m_fs->readDir(this, entry);
    }

    void Directory::rewind()
    {
        // TODO: Implement rewind - reset directory iteration
    }

} // namespace QFS
