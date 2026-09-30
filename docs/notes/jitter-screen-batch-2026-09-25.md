# Screen-sampling candidate batch — 2026-09-25

This note records the separately authorized manual trial for the remaining screen-sampling candidates. It does not change the accepted e810+fe31 guarded trial or the accepted 2078 slot-8 scene result.

## Implemented scope

The runtime adds exactly 40 VS entries to `PositionVPSlot`: 35 slot-7 mappings and 5 slot-8 mappings, covering 101 observed PS pairings. The source and decision record is [`screen_mapping_batch_20260925.json`](../../../tools/shader_analysis/reviews/screen_mapping_batch_20260925.json), based on the historical `feedback_mapping` review. Mixed-camera cases `bda41a11626a545c` and `eb5f611c4321708e` remain excluded and separately held. The baseline `main` reference is `1fd4b4b`.

The accepted e810+fe31 conditional slot-7 gate and accepted 2078 slot-8 mapping remain separate. This batch does not broaden either accepted scope.

## Evidence boundary

The 40 candidates have no corresponding reviewed-corpus capture evidence. The selector passed 78,316 CPU checks for 40 VS, 101 PS pairings, 32 phases and 1440p/4K; old separation was 0.487798 px and maximum projection error was 0.000279 px. These checks verify projection arithmetic and guard behavior; they do not verify texture-producer alignment, pixel sampling phase, image coverage or visual correctness.

The manual package is available at `out/validation/issue64/manual-screen-batch1/Test.cmd`. The EXE SHA-256 is `63D98F60BDDB293117B6D320CBF572781C701A3670ABD3084650E378F20DD0CC` from `1fd4b4b+dirty` with the batch. The build passed and the user accepted the manual run. No new capture evidence is claimed here. The trial does not establish whole-game coverage or cross-hardware behavior; v0.6.20 was published on 2026-09-25T21:15:07Z from `be842b91d7367fd198074b1b8d3c1bc3ef4372a6` after successful [Release CI](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36188596414). Public Windows/Linux packages and their checksum files are available; no additional local artifact validation was performed, as requested.
