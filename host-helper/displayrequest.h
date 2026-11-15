#pragma once
// Injectable API boundary so validation/apply failure paths can be tested
// without changing a machine's actual display configuration.
namespace DisplayRequest {
constexpr unsigned Clone = 0x2, Extend = 0x4, Validate = 0x40, Apply = 0x80;
template<class SetConfig>
long apply(unsigned topology, SetConfig setConfig) {
    if (topology != Clone && topology != Extend) return 87; // ERROR_INVALID_PARAMETER
    long result = setConfig(topology | Validate);
    return result == 0 ? setConfig(topology | Apply) : result;
}
}
