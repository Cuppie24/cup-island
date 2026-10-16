#include "platform/shell_images.h"

#include "platform/win.h"

#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <cstdlib>

namespace cup::platform {

std::shared_ptr<const ImageData> shellImage(const std::wstring& path, int pixels)
{
    winrt::com_ptr<IShellItemImageFactory> factory;
    if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(factory.put())))) return nullptr;
    HBITMAP bitmap = nullptr;
    SIZE size{ pixels, pixels };
    // RESIZETOFIT: the thumbnail if there is one, else the icon, kept in proportion.
    if (FAILED(factory->GetImage(size, SIIGBF_RESIZETOFIT, &bitmap)) || !bitmap) return nullptr;

    BITMAP info{};
    GetObjectW(bitmap, sizeof info, &info);
    auto image = std::make_shared<ImageData>();
    image->width = info.bmWidth;
    image->height = std::abs(info.bmHeight);
    image->bgra.resize(static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height) * 4);
    BITMAPINFO want{};
    want.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    want.bmiHeader.biWidth = image->width;
    want.bmiHeader.biHeight = -image->height; // top-down, as ImageData is
    want.bmiHeader.biPlanes = 1;
    want.bmiHeader.biBitCount = 32;
    want.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(nullptr);
    int lines = GetDIBits(screen, bitmap, 0, static_cast<UINT>(image->height), image->bgra.data(), &want, DIB_RGB_COLORS);
    ReleaseDC(nullptr, screen);
    DeleteObject(bitmap);
    if (lines != image->height || image->bgra.empty()) return nullptr;
    // Icons come with alpha, already premultiplied. A photo's thumbnail often
    // has none at all (every alpha 0): it's opaque, not invisible.
    bool alpha = false;
    for (std::size_t i = 3; i < image->bgra.size() && !alpha; i += 4) alpha = image->bgra[i] != 0;
    if (!alpha)
        for (std::size_t i = 3; i < image->bgra.size(); i += 4) image->bgra[i] = 255;
    return image;
}

} // namespace cup::platform
