#pragma once

// QNetwork Ethernet - Ethernet layer
// Namespace: QNet

#include "QCTypes.h"

namespace QNet
{

    // MAC address
    struct MACAddress
    {
        QC::u8 bytes[6];

        bool operator==(const MACAddress &other) const;
        bool isBroadcast() const;
        bool isMulticast() const;
    };

    // Ethernet header
    struct EthernetHeader
    {
        MACAddress destination;
        MACAddress source;
        QC::u16 etherType;
    } __attribute__((packed));

    // EtherTypes
    namespace EtherType
    {
        constexpr QC::u16 IPv4 = 0x0800;
        constexpr QC::u16 ARP = 0x0806;
        constexpr QC::u16 IPv6 = 0x86DD;
    }

    class Ethernet
    {
    public:
        Ethernet();
        ~Ethernet();

        void initialize();

        // MAC address
        void setMACAddress(const MACAddress &mac) { m_mac = mac; }
        const MACAddress &macAddress() const { return m_mac; }

        // Packet handling
        void receiveFrame(const void *data, QC::usize length);
        void sendFrame(const MACAddress &dest, QC::u16 etherType,
                       const void *payload, QC::usize length);

        // ARP
        bool resolveMAC(QC::u32 ipAddress, MACAddress *mac);
        void updateARPCache(QC::u32 ipAddress, const MACAddress &mac);

    private:
        MACAddress m_mac;

        // ARP cache
        static constexpr QC::usize ARP_CACHE_SIZE = 64;
        struct ARPEntry
        {
            QC::u32 ip;
            MACAddress mac;
            QC::u64 timestamp;
            bool valid;
        };
        ARPEntry m_arpCache[ARP_CACHE_SIZE];
    };

} // namespace QNet
