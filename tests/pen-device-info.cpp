#define WINVER 0x0602
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <cstdio>
#include <vector>

int main()
{
    UINT32 count = 0;
    if (!GetPointerDevices(&count, nullptr)) return 1;
    std::vector<POINTER_DEVICE_INFO> devices(count);
    if (count && !GetPointerDevices(&count, devices.data())) return 2;
    for (UINT32 i = 0; i < count; i++) {
        const auto& d = devices[i];
        std::printf("Pointer device: type=%u, profile=%s\n", d.pointerDeviceType,
                    d.pointerDeviceType == POINTER_DEVICE_TYPE_INTEGRATED_PEN ? "S Pen" :
                    d.pointerDeviceType == POINTER_DEVICE_TYPE_EXTERNAL_PEN ? "PTH-451" : "unmodified");
    }
}
