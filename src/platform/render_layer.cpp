#include "platform/render_layer.h"

namespace cup::platform {
namespace {

constexpr wchar_t kClassName[] = L"CupIslandRenderWindow";

wuc::Desktop::DesktopWindowTarget createWindowTarget(wuc::Compositor const& compositor, HWND window)
{
    namespace abi = ABI::Windows::UI::Composition::Desktop;

    auto interop = compositor.as<abi::ICompositorDesktopInterop>();
    wuc::Desktop::DesktopWindowTarget target{ nullptr };
    winrt::check_hresult(interop->CreateDesktopWindowTarget(
        window,
        /* isTopmost */ true,
        reinterpret_cast<abi::IDesktopWindowTarget**>(winrt::put_abi(target))));
    return target;
}

} // namespace

winrt::Windows::System::DispatcherQueueController createDispatcherQueueForCurrentThread()
{
    DispatcherQueueOptions options{
        sizeof(DispatcherQueueOptions),
        DQTYPE_THREAD_CURRENT,
        DQTAT_COM_STA
    };

    winrt::Windows::System::DispatcherQueueController controller{ nullptr };
    winrt::check_hresult(CreateDispatcherQueueController(
        options,
        reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(winrt::put_abi(controller))));
    return controller;
}

RenderLayer createRenderLayer(HINSTANCE instance, Vec2 canvas)
{
    WNDCLASSEXW windowClass{ sizeof(windowClass) };
    windowClass.lpfnWndProc = DefWindowProcW; // input never reaches it (WS_EX_TRANSPARENT)
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kClassName;
    RegisterClassExW(&windowClass);

    RenderLayer layer;
    int width = static_cast<int>(canvas.x);
    int height = static_cast<int>(canvas.y);
    layer.left = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;

    // NOREDIRECTIONBITMAP: DirectComposition owns every pixel, unpainted areas
    // are simply transparent. LAYERED | TRANSPARENT: excluded from hit-testing,
    // the only way to pass clicks through to other processes' windows
    // (HTTRANSPARENT only forwards within one thread). NOACTIVATE: never takes focus.
    layer.hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, L"Cup Island Render",
        WS_POPUP,
        layer.left, 0, width, height,
        nullptr, nullptr, instance, nullptr);
    if (!layer.hwnd) return layer;

    layer.compositor = wuc::Compositor();
    layer.target = createWindowTarget(layer.compositor, layer.hwnd);
    layer.root = layer.compositor.CreateContainerVisual();
    layer.root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    layer.target.Root(layer.root);
    return layer;
}

} // namespace cup::platform
