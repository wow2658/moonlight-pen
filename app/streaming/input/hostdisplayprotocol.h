#pragma once
namespace HostDisplayProtocol {
// Use ordinary letter keys: Sunshine reserves Ctrl+Alt+Shift+F1 through F13
// for its own display switching, and Explorer's handling of high function keys
// varies across input backends. D/E/H can also be tested on a normal keyboard.
constexpr unsigned CloneKey = 0x44;  // D
constexpr unsigned ExtendKey = 0x45; // E
constexpr unsigned ProbeKey = 0x48;  // H
constexpr unsigned CloneId = 1, ExtendId = 2, ProbeId = 3;
}
