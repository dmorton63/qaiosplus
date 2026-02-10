// QNetwork Ethernet - Ethernet layer implementation
// Namespace: QNet

#include "QNetEthernet.h"
#include "QNetStack.h"
#include "QNetIP.h"
#include "QCMemUtil.h"
#include "QKMemHeap.h"

namespace QNet
{

    // MACAddress implementation
    bool MACAddress::operator==(const MACAddress &other) const
    {
        for (int i = 0; i < 6; i++)
        {
            if (bytes[i] != other.bytes[i])
                return false;
        }
        return true;
    }

    bool MACAddress::isBroadcast() const
    {
        for (int i = 0; i < 6; i++)
        {
            if (bytes[i] != 0xFF)
                return false;
        }
        return true;
    }

    bool MACAddress::isMulticast() const
    {
        return (bytes[0] & 0x01) != 0;
    }

    // ARP packet structure
    struct ARPPacket
    {
        QC::u16 hardwareType;
        QC::u16 protocolType;
        QC::u8 hardwareAddrLen;
        QC::u8 protocolAddrLen;
        QC::u16 operation;
        MACAddress senderMAC;
        QC::u32 senderIP;
        MACAddress targetMAC;
        QC::u32 targetIP;
    } __attribute__((packed));

    namespace ARPOperation
    {
        constexpr QC::u16 Request = 1;
        constexpr QC::u16 Reply = 2;
    }

    // Byte swap utilities for network byte order
    static inline QC::u16 htons(QC::u16 val)
    {
        return (val >> 8) | (val << 8);
    }

    static inline QC::u16 ntohs(QC::u16 val)
    {
        return htons(val);
    }

    static inline QC::u32 htonl(QC::u32 val)
    {
        return ((val >> 24) & 0x000000FF) |
               ((val >> 8) & 0x0000FF00) |
               ((val << 8) & 0x00FF0000) |
               ((val << 24) & 0xFF000000);
    }

    static inline QC::u32 ntohl(QC::u32 val)
    {
        return htonl(val);
    }

    Ethernet::Ethernet()
    {
        memset(&m_mac, 0, sizeof(m_mac));
        memset(m_arpCache, 0, sizeof(m_arpCache));
    }

    Ethernet::~Ethernet()
    {
    }

    void Ethernet::initialize()
    {
        // Clear ARP cache
        for (QC::usize i = 0; i < ARP_CACHE_SIZE; i++)
        {
            m_arpCache[i].valid = false;
        }
    }

    void Ethernet::receiveFrame(const void *data, QC::usize length)
    {
        if (length < sizeof(EthernetHeader))
            return;

        const auto *header = static_cast<const EthernetHeader *>(data);

        // Check if frame is for us (unicast, broadcast, or multicast)
        if (!header->destination.isBroadcast() &&
            !header->destination.isMulticast() &&
            !(header->destination == m_mac))
        {
            return; // Not for us
        }

        QC::u16 etherType = ntohs(header->etherType);
        const void *payload = static_cast<const QC::u8 *>(data) + sizeof(EthernetHeader);
        QC::usize payloadLen = length - sizeof(EthernetHeader);

        switch (etherType)
        {
        case EtherType::IPv4:
            Stack::instance().ip()->receivePacket(payload, payloadLen);
            break;

        case EtherType::ARP:
            handleARP(payload, payloadLen);
            break;

        case EtherType::IPv6:
            // IPv6 not yet supported
            break;
        }
    }

    void Ethernet::sendFrame(const MACAddress &dest, QC::u16 etherType,
                             const void *payload, QC::usize length)
    {
        // Allocate buffer for full frame
        QC::usize frameSize = sizeof(EthernetHeader) + length;
        QC::u8 *frame = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(frameSize));
        if (!frame)
            return;

        // Build header
        auto *header = reinterpret_cast<EthernetHeader *>(frame);
        header->destination = dest;
        header->source = m_mac;
        header->etherType = htons(etherType);

        // Copy payload
        memcpy(frame + sizeof(EthernetHeader), payload, length);

        // Transmit
        Stack::instance().transmitPacket(frame, frameSize);

        QK::Memory::Heap::instance().free(frame);
    }

    bool Ethernet::resolveMAC(QC::u32 ipAddress, MACAddress *mac)
    {
        // Check ARP cache first
        for (QC::usize i = 0; i < ARP_CACHE_SIZE; i++)
        {
            if (m_arpCache[i].valid && m_arpCache[i].ip == ipAddress)
            {
                *mac = m_arpCache[i].mac;
                return true;
            }
        }

        // Send ARP request
        sendARPRequest(ipAddress);

        // In a real implementation, we'd wait for reply
        // For now, return false to indicate resolution in progress
        return false;
    }

    void Ethernet::updateARPCache(QC::u32 ipAddress, const MACAddress &mac)
    {
        // Look for existing entry
        for (QC::usize i = 0; i < ARP_CACHE_SIZE; i++)
        {
            if (m_arpCache[i].valid && m_arpCache[i].ip == ipAddress)
            {
                m_arpCache[i].mac = mac;
                m_arpCache[i].timestamp = 0; // TODO: use actual time
                return;
            }
        }

        // Find free slot or oldest entry
        QC::usize slot = 0;
        for (QC::usize i = 0; i < ARP_CACHE_SIZE; i++)
        {
            if (!m_arpCache[i].valid)
            {
                slot = i;
                break;
            }
        }

        // Add new entry
        m_arpCache[slot].ip = ipAddress;
        m_arpCache[slot].mac = mac;
        m_arpCache[slot].timestamp = 0;
        m_arpCache[slot].valid = true;
    }

    void Ethernet::handleARP(const void *data, QC::usize length)
    {
        if (length < sizeof(ARPPacket))
            return;

        const auto *arp = static_cast<const ARPPacket *>(data);

        // Only handle Ethernet/IPv4 ARP
        if (ntohs(arp->hardwareType) != 1 || ntohs(arp->protocolType) != EtherType::IPv4)
            return;

        // Update cache with sender info
        updateARPCache(arp->senderIP, arp->senderMAC);

        QC::u16 op = ntohs(arp->operation);
        if (op == ARPOperation::Request)
        {
            // Check if they're asking for our IP
            IPv4Address ourIP = Stack::instance().ip()->address();
            if (arp->targetIP == ourIP.value)
            {
                sendARPReply(arp->senderIP, arp->senderMAC);
            }
        }
    }

    void Ethernet::sendARPRequest(QC::u32 targetIP)
    {
        ARPPacket arp;
        arp.hardwareType = htons(1); // Ethernet
        arp.protocolType = htons(EtherType::IPv4);
        arp.hardwareAddrLen = 6;
        arp.protocolAddrLen = 4;
        arp.operation = htons(ARPOperation::Request);
        arp.senderMAC = m_mac;
        arp.senderIP = Stack::instance().ip()->address().value;
        memset(&arp.targetMAC, 0, sizeof(MACAddress));
        arp.targetIP = targetIP;

        // Broadcast ARP request
        MACAddress broadcast;
        memset(&broadcast, 0xFF, sizeof(broadcast));
        sendFrame(broadcast, EtherType::ARP, &arp, sizeof(arp));
    }

    void Ethernet::sendARPReply(QC::u32 targetIP, const MACAddress &targetMAC)
    {
        ARPPacket arp;
        arp.hardwareType = htons(1);
        arp.protocolType = htons(EtherType::IPv4);
        arp.hardwareAddrLen = 6;
        arp.protocolAddrLen = 4;
        arp.operation = htons(ARPOperation::Reply);
        arp.senderMAC = m_mac;
        arp.senderIP = Stack::instance().ip()->address().value;
        arp.targetMAC = targetMAC;
        arp.targetIP = targetIP;

        sendFrame(targetMAC, EtherType::ARP, &arp, sizeof(arp));
    }

} // namespace QNet
