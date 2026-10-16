#pragma once
// A file's or folder's picture from Windows (decision 063): its thumbnail
// when Windows has one (a photo, a PDF's first page), else its type's icon,
// as the same picture Explorer shows. Blocking: call it off the UI thread,
// on a thread with COM started.

#include "core/content.h"

#include <memory>
#include <string>

namespace cup::platform {

// At most `pixels` on its longer side; nothing if Windows has no picture for it.
std::shared_ptr<const ImageData> shellImage(const std::wstring& path, int pixels);

} // namespace cup::platform
