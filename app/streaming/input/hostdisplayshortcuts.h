#pragma once
#include <array>

namespace HostDisplayShortcuts {
struct Key { short code; bool down; char modifiers; };
// Windows VK codes and GameStream modifier bits. Every sequence releases all
// synthetic keys. The caller releases physical keys before sending a sequence.
inline std::array<Key, 8> monitor(int number)
{
    const short fn = static_cast<short>(number == 2 ? 0x71 : 0x70);
    return {{{0xA2, true, 2}, {0xA4, true, 6}, {0xA0, true, 7}, {fn, true, 7},
             {fn, false, 7}, {0xA0, false, 6}, {0xA4, false, 2}, {0xA2, false, 0}}};
}
inline std::array<Key, 8> helper(short fn)
{
    // Dedicated host actions: Ctrl+Alt+Shift+D/E/H; see hostdisplayprotocol.h.
    return {{{0xA2, true, 2}, {0xA4, true, 6}, {0xA0, true, 7}, {fn, true, 7},
             {fn, false, 7}, {0xA0, false, 6}, {0xA4, false, 2}, {0xA2, false, 0}}};
}
}
