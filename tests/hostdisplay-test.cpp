#include "../app/streaming/input/hostdisplayshortcuts.h"
#include "../app/streaming/input/hostdisplayprotocol.h"
#include <cassert>
#include <set>
#include <cstdio>

template<size_t N> void check(const std::array<HostDisplayShortcuts::Key, N>& keys)
{
    std::set<short> down;
    for (auto key : keys) {
        if (key.down) assert(down.insert(key.code).second);
        else assert(down.erase(key.code) == 1);
    }
    assert(down.empty());
    assert((keys.back().modifiers & 0x0F) == 0);
}
int main()
{
    auto first = HostDisplayShortcuts::monitor(1);
    auto second = HostDisplayShortcuts::monitor(2);
    auto clone = HostDisplayShortcuts::helper(HostDisplayProtocol::CloneKey);
    auto extend = HostDisplayShortcuts::helper(HostDisplayProtocol::ExtendKey);
    auto probe = HostDisplayShortcuts::helper(HostDisplayProtocol::ProbeKey);
    check(first); check(second); check(clone); check(extend); check(probe);
    assert(first[3].code == 0x70 && first[3].modifiers == 7);
    assert(second[3].code == 0x71 && second[3].modifiers == 7);
    assert(clone[3].code == 0x44 && extend[3].code == 0x45 && probe[3].code == 0x48);
    for (auto sequence : {clone, extend, probe}) {
        assert(sequence[3].modifiers == 7);
        assert(!(sequence[3].code >= 0x70 && sequence[3].code <= 0x7C));
        assert(sequence[3].code != 0x4E); // Sunshine's cursor visibility shortcut
    }
    puts("Host display shortcuts: PASS (balanced keys; F1/F2 monitors; D/E/H host actions; no Sunshine shortcut collisions)");
}
