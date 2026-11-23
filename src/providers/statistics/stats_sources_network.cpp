// Network adapters and their byte counters (GetIfTable2, Windows Vista+;
// decision 173). One table read per look: nothing is subscribed to.
#include "providers/statistics/stats_sources.h"

#include "platform/win.h"

#include <winsock2.h>

#include <ws2ipdef.h>

#include <iphlpapi.h>
#include <netioapi.h>

namespace cup::stats {

std::vector<AdapterSample> networkAdapters()
{
    std::vector<AdapterSample> found;
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table) return found;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2& row = table->Table[i];
        // Windows lists every adapter several times over: once itself, and
        // once per filter and virtual layer on it (a firewall's, a VPN's),
        // each counting the same bytes. Only the hardware is counted.
        if (!row.InterfaceAndOperStatusFlags.HardwareInterface || row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        found.push_back(AdapterSample{
            .id = row.InterfaceLuid.Value,
            .name = utf8(row.Alias),
            .received = row.InOctets,
            .sent = row.OutOctets,
            .linkSpeed = row.ReceiveLinkSpeed,
            .wired = row.Type == IF_TYPE_ETHERNET_CSMACD,
            .up = row.OperStatus == IfOperStatusUp,
        });
    }
    FreeMibTable(table);
    return found;
}

} // namespace cup::stats
