#pragma once

#include "Clipboard.h"

#include <Windows.h>
#include <functional>
#include <string>

namespace drek_flag_cheat {

void RegisterDiscordDrop(HWND window,
    std::function<void(ClipboardImage)> onDrop,
    std::function<void(const std::string&)> onError);

} 
