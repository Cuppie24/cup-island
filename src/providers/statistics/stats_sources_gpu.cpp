// Graphics adapters, and a GPU's temperature (decisions 173, 174).
//
// The temperature is what Task Manager shows: the adapter's performance
// data, which a WDDM 2.4+ driver reports to the graphics kernel, asked
// through D3DKMTQueryAdapterInfo. No admin rights, no vendor SDK. The three
// D3DKMT functions are gdi32's and older than Windows 10, but they are
// looked up by name at runtime all the same (decision 015): the kind of
// data asked for is newer (Windows 10 1803), and nothing here may stop the
// exe from starting. Windows or a driver that doesn't know it: no reading.
// The structures are the Windows SDK's own (d3dkmthk.h), never retyped;
// built with a kit that lacks the header, there is no reading either.
#include "providers/statistics/stats_sources.h"

#include "platform/win.h"

#include <dxgi.h>

#if __has_include(<d3dkmthk.h>)
#include <winternl.h> // NTSTATUS, which d3dkmthk.h expects

#include <d3dkmthk.h>
#endif

namespace cup::stats {

std::vector<GpuAdapter> gpuAdapters()
{
    std::vector<GpuAdapter> found;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) || !factory) return found;
    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) != S_OK || !adapter) break; // DXGI_ERROR_NOT_FOUND: no more
        DXGI_ADAPTER_DESC1 description{};
        if (SUCCEEDED(adapter->GetDesc1(&description))) {
            found.push_back(GpuAdapter{
                .luid = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(description.AdapterLuid.HighPart)) << 32) | description.AdapterLuid.LowPart,
                .dedicatedMemory = description.DedicatedVideoMemory,
                .software = (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0,
            });
        }
        adapter->Release();
    }
    factory->Release();
    return found;
}

#if __has_include(<d3dkmthk.h>)

std::optional<float> gpuTemperature(std::uint64_t luid)
{
    using Open = NTSTATUS(APIENTRY*)(D3DKMT_OPENADAPTERFROMLUID*);
    using Query = NTSTATUS(APIENTRY*)(D3DKMT_QUERYADAPTERINFO*);
    using Close = NTSTATUS(APIENTRY*)(D3DKMT_CLOSEADAPTER*);
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll"); // loaded with user32: every window needs it
    if (!gdi) return std::nullopt;
    auto open = reinterpret_cast<Open>(reinterpret_cast<void*>(GetProcAddress(gdi, "D3DKMTOpenAdapterFromLuid")));
    auto query = reinterpret_cast<Query>(reinterpret_cast<void*>(GetProcAddress(gdi, "D3DKMTQueryAdapterInfo")));
    auto close = reinterpret_cast<Close>(reinterpret_cast<void*>(GetProcAddress(gdi, "D3DKMTCloseAdapter")));
    if (!open || !query || !close) return std::nullopt;

    D3DKMT_OPENADAPTERFROMLUID opened{};
    opened.AdapterLuid.LowPart = static_cast<DWORD>(luid & 0xFFFFFFFFull);
    opened.AdapterLuid.HighPart = static_cast<LONG>(luid >> 32);
    if (open(&opened) < 0) return std::nullopt; // an NTSTATUS: negative is failure
    D3DKMT_ADAPTER_PERFDATA data{};             // PhysicalAdapterIndex 0: the adapter's first (and usually only) GPU
    D3DKMT_QUERYADAPTERINFO asked{};
    asked.hAdapter = opened.hAdapter;
    asked.Type = KMTQAITYPE_ADAPTERPERFDATA;
    asked.pPrivateDriverData = &data;
    asked.PrivateDriverDataSize = sizeof(data);
    NTSTATUS status = query(&asked);
    D3DKMT_CLOSEADAPTER closing{};
    closing.hAdapter = opened.hAdapter;
    close(&closing); // nothing stays open between looks
    // Tenths of a degree Celsius; 0 is a driver that doesn't report one (cleanTemperature drops it too).
    if (status < 0 || data.Temperature == 0) return std::nullopt;
    return static_cast<float>(data.Temperature) / 10.0f;
}

#else

std::optional<float> gpuTemperature(std::uint64_t)
{
    return std::nullopt;
}

#endif

} // namespace cup::stats
