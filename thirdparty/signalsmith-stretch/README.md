# Signalsmith Stretch

Vendored unchanged from Signalsmith Stretch **1.4.0**, commit
`a670068d9aeb64913331d5cc29337b19a457a7df`:
https://github.com/Signalsmith-Audio/signalsmith-stretch/tree/a670068d9aeb64913331d5cc29337b19a457a7df

The portable FFT/STFT headers come from Signalsmith Linear **0.6.5**, commit
`146b26f91b48835d665fa0498d64d9617818d27a`:
https://github.com/Signalsmith-Audio/linear/tree/146b26f91b48835d665fa0498d64d9617818d27a

Only the three headers required by the portable backend are included; platform
FFT integrations are not enabled. Both projects use the MIT license; the
respective `LICENSE.txt` files are included here and in `signalsmith-linear/`.

`apu/fast_forward_audio.h` uses one six-channel instance with a 2048-sample
window and a 512-sample hop, on every platform including Android. This bounds
the algorithmic start latency to 32 ms at 2x (less at higher tempos), and avoids
an external FFT dependency. No FFT or stretch processing runs at normal speed.
