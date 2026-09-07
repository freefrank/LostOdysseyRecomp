# SMAA

Source: https://github.com/iryoku/smaa at commit 71c806a838bdd7d517df19192a20f0c61b3ca29d. Original files unchanged. SMAA_source.h embeds the shader with explicit sampler registers s0/s1 replacing effect sampler blocks. MIT license in LICENSE.txt.
`python thirdparty/smaa/generate_embedding.py` regenerates the embedded source. The integration uses HIGH preset, color edge detection, zero subsample indices, a native-size crop to exclude padded resolve storage, and the original area/search lookup tables. All passes use UNORM views; neighborhood blending runs in gamma space as allowed by upstream integration instructions. Resources and descriptor sets are owned until the presentation fence; callers must finish the previous Draw before resizing or drawing again.
