# Descriptor-buffer capability probe, not a backend

The existing Android diagnostic app now reports `VK_EXT_descriptor_buffer`
advertisement, the actual `descriptorBuffer` feature, offset alignment,
maximum binding count and sampler/image/uniform/storage descriptor sizes.
It queries the extension feature/property structs only when advertised and
when the renderer already has usable Vulkan 1.2 feature-query entry points.
Extension enumeration errors and missing feature-query support remain explicit.
It creates no device, buffer, descriptor or command and enables no extension.

Driver support must be measured on each actual Turnip/proprietary driver and
GPU combination. An Adreno model or Android version does not establish that
this extension is usable. Keep the renderer's existing immutable batch cache,
sparse pooled binding rewrites, dummy banks, bind elision and dynamic UBO path.
A descriptor-buffer backend also needs feature/dependency checks, new pipeline
layout flags, exact implementation-defined descriptor sizes/offsets, stable
addressable storage and fence lifetime rules. The extension name alone does
not justify that change, and no Android profile here proves it worthwhile.

The host `tools/tests/descriptor_buffer_probe_test.cpp` passes ASan/UBSan
(leak detection disabled because this runner cannot inspect `/proc`). It checks
absent extension, absent query entry points, false feature, property values and
both extension enumeration error stages, without calling Vulkan hardware.
`vulkan_probe.cpp` passes GCC 13 C++20 syntax checks with pinned Vulkan/SDL
headers. No Android probe APK, real driver result or speedup is claimed.

Host test command from the repository root, with Vulkan headers installed:

```sh
g++ -std=c++20 -I thirdparty/plume/contrib/Vulkan-Headers/include tools/tests/descriptor_buffer_probe_test.cpp -o /tmp/descriptor-probe-test
/tmp/descriptor-probe-test
```

Reference: https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_descriptor_buffer.html
