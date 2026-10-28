# lsfg-vk (vendored subset)

The lsfg-vk frame generation backend (https://lsfg-vk.dev, GPL-3.0-or-later,
see LICENSE.md) as vendored by NetherSX2_nx (https://github.com/NaGaa95/NetherSX2_nx,
third_party/lsfg-vk at f084dc1): borrowed-device support for running on an
application's own Vulkan device, and NVK replacements for the beta1 and mipmap
shaders. Used on the Switch by LostOdysseyRecomp/gpu/lsfg_frame_gen.cpp.

The shaders themselves are not included: they are read at run time from the
user's own Lossless.dll (the Steam version of Lossless Scaling), placed at
/switch/LostOdysseyRecomp/lsfg/Lossless.dll.
