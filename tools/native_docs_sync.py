"""One-shot source-specific documentation sync; removed in its publish commit."""
from pathlib import Path
import hashlib
import json

EXPECTED = {
    'CHANGELOG.md':'50654334d66ef454c26a3e2753d1d976037ce0a2c804b5dd6b4b2b2b227cb7de',
    'docs/STATUS.md':'d4540d8351897ca0a407215d7bff6341c14c5729e415f3ee8666a128df9ed474',
    'docs/ROADMAP.md':'f393c4aa7376a36e4b3e06c349d0ac8077ce470be41858d59b369ef41d677277',
    'docs/ROADMAP.zh-CN.md':'94d0e9a6c34c00e51e6bc54883561ac3cea0ec9176d7d08fc87b5f913fd98de6',
    'tools/perf/README.md':'299e229a248ee778743ee00a6ccb66125a0e08569a949e7605e022b3d3487402',
}
for name,expected in EXPECTED.items():
    if hashlib.sha256(Path(name).read_bytes()).hexdigest()!=expected:
        raise SystemExit('Refuse to overwrite changed documentation: '+name)

def write(name,text):
    Path(name).write_text(text,encoding='utf-8',newline='\n')

def one(text,old,new):
    if text.count(old)!=1: raise SystemExit('Ambiguous documentation anchor: '+old)
    return text.replace(old,new,1)

en='- **Native renderer front-end (development, opt-in)**: Added a shared explicit `DrawState` backend and complete ordinary indexed/non-indexed mesh hooks before SDK state flush. Accepted calls emit owned ordered state deltas, preserve mixed legacy state/predication, and use shared native stream coherency. Windows renderer/CP/producer compilation and the 128-case canonical SDK CPU oracle passed, including stream-wait ordering with zero native flush calls. Added independent `LO_NATIVE_FRONTEND=mesh` / `--native-frontend` selection and corrected diagnostic receipt/scene parsing. Shader/derived preparation, UP/special/recorded paths and legacy presentation scheduling remain; no full application link, new-path game/GPU run, Uhra performance win, user acceptance or release is claimed. See [implementation and remaining coverage](docs/notes/native-renderer-front-end.md).\n'
zh='- **原生图形前端（开发中、按需开启）**：新增共用的显式`DrawState`后端，在普通indexed/non-indexed mesh的SDK状态flush之前接管完整调用。受支持调用提交自有值的有序状态delta，保留native/legacy状态一致性与predication，并通过共用执行器处理原生stream coherency。Windows renderer/CP/producer编译及128例原SDK CPU对照通过，包含stream等待顺序，native路径SDK flush调用为零。新增独立`LO_NATIVE_FRONTEND=mesh` / `--native-frontend`开关，并修复诊断回执和场景标记解析。Shader/派生状态准备、UP/特殊/录制路径及legacy呈现调度仍待迁移；未宣称完整应用链接、新路径游戏/GPU运行、Uhra性能收益、用户验收或发布。详见[实现与剩余覆盖](docs/notes/native-renderer-front-end.md)。\n'
s=Path('CHANGELOG.md').read_text(encoding='utf-8');i=s.index('## Unreleased');head=s[:i];tail=s[i:]
assert tail.index('### English\n')<tail.index('\n## ',1)
tail=tail.replace('### English\n','### English\n\n'+en,1)
pos=tail.index('### 简体中文\n');tail=tail[:pos]+tail[pos:].replace('### 简体中文\n','### 简体中文\n\n'+zh,1)
write('CHANGELOG.md',head+tail)
status='- **Native renderer front-end P1/P2 and stream subset (development)**: [Explicit shared backend and pre-flush ordinary mesh](notes/native-renderer-front-end.md) are pushed on `feature/native-renderer-front-end` (`2c85a3a` runtime checkpoint). Windows production renderer/CP/producer compilation, portable CPU contracts and 128 canonical-SDK cases pass; the extended oracle preserves predicated stream-wait order with zero SDK flush calls on accepted native calls. `LO_NATIVE_FRONTEND=mesh` is separate from the historical off/all experiment. Shader/derived preparation, UP/special/recorded paths and PM4 presentation/scheduling remain. Real-game coverage, full application link, GPU/platform acceptance, new Uhra 120 FPS A/B and full PM4 removal are open; default off, not released.\n'
write('docs/STATUS.md',one(Path('docs/STATUS.md').read_text(encoding='utf-8'),'Status as of 2026-09-29:\n','Status as of 2026-09-29:\n'+status))
for name,anchor,text in [
('docs/ROADMAP.md','8. [ ] **Remove the PM4 translator:** ','2026-09-29 update: the [new native front-end](notes/native-renderer-front-end.md) implements the shared DrawState backend, pre-flush ordinary indexed/non-indexed mesh and native stream coherency. Production Windows TUs and the 128-case original-SDK CPU oracle pass. This is mixed-mode development, not complete P3/P4: shader/derived preparation, UP/special/recorded commands, generic events and presentation still depend on legacy; game coverage and new-path performance are unmeasured. **Historical prototype evidence:** '),
('docs/ROADMAP.zh-CN.md','8. [ ] **移除 PM4 转换器：**','2026-09-29更新：[新原生前端](notes/native-renderer-front-end.md)已实现共用DrawState后端、普通indexed/non-indexed mesh的flush前接管，以及原生stream coherency；Windows生产代码编译和128例原SDK CPU对照通过。当前属于混合路径开发，P3/P4尚未完成：shader/派生状态准备、UP/特殊/录制命令、通用事件和呈现仍依赖legacy；真实游戏覆盖和新路径性能未测。**历史原型证据：**')]:
    write(name,one(Path(name).read_text(encoding='utf-8'),anchor,anchor+text))
readme=r'''

### Ordinary-mesh native front-end

The separate `--native-frontend off|mesh` option selects the pre-flush ordinary
mesh implementation. `--mode` still selects the older sparse/quad/fan hooks;
use `--mode off` in both cases to isolate the new work. The driver records both
options and the actual candidate executable SHA-256. A mesh run that never
acknowledges the new mode, or has no executed native mesh in the sample, fails
rather than silently benchmarking an old executable.

The following is the new ordinary Uhra ABBA procedure, **not a completed
measurement**. Point `$nativeBuild` at a build of this development branch, not
the historical `out/endian-constants/runtime` executable. Keep the baseline and
game paths read-only. Every output directory must be new.

```powershell
$nativeBuild = 'out/native-renderer/runtime' # replace with the actual candidate build
$nativeBaseline = 'C:/Users/freefrank/ownCloud/Git/LostOdysseyRecomp/out/issue70-runtime/baseline'
$nativeGame = 'D:/Mihoyo/LostOdysseyRecomp-windows-x64/game/disc1'
$nativeRun = Get-Date -Format 'yyyyMMdd-HHmmss'
$nativeCases = @(
    @{Name='off-1'; Frontend='off'}, @{Name='mesh-1'; Frontend='mesh'},
    @{Name='mesh-2'; Frontend='mesh'}, @{Name='off-2'; Frontend='off'}
)
foreach ($case in $nativeCases) {
    $output = "out/native-pm4/frontend-$nativeRun-$($case.Name)"
    python tools/perf/run_native_title.py --build $nativeBuild --baseline $nativeBaseline --game $nativeGame --output $output --mode off --native-frontend $case.Frontend --scene uhra --fps 120 --backend d3d12 --warmup-frame 3300 --sample-seconds 20 --startup-timeout 180
    if ($LASTEXITCODE -ne 0) { throw "Native front-end probe failed: $output" }
}
```

Ordinary comparisons must omit `--render-timing`, `--scene-stats` and
`--frontend-stats`. Each enables an explicit diagnostic and labels the result
accordingly. `--frontend-stats` adds producer fallback reasons and remaining
PM4 opcode/type counts; the other two can turn on per-draw renderer timing.
Both `frame timing completed=` and `present timing completed=` are accepted.
The scene marker is latched during incremental reads, so large later logs do
not evict startup evidence. Inherited `LO_*` variables are cleared and these
options are explicitly injected into the child environment.

Low-cost native execution receipts are recorded separately from diagnostic
counters. Their coverage window is expressed in their own swap receipts, not
silently normalized to a different CPU sample window or to host-expanded GPU
draws. Inspect CmdProc, Guest Main, whole-process CPU and throughput together.
Log reset/truncation or missing completed-frame progress invalidates the sample.
Rollback is `LO_NATIVE_FRONTEND=off` (or unset), followed by a process restart;
`LO_NATIVE_COMMANDS=off` additionally disables the earlier prototype.

Portable parser/environment regression checks are
`python tools/tests/native_probe_log_test.py`; they do not require Windows or
`psutil`. Actual game runs require both. See the
[implementation, SDK contracts and remaining dependencies](../../docs/notes/native-renderer-front-end.md)
and [bounded validation record](../../docs/notes/native-renderer-front-end-evidence.json).
'''
write('tools/perf/README.md',Path('tools/perf/README.md').read_text(encoding='utf-8').rstrip()+readme)
RESULT={
'CHANGELOG.md':'eec260451507e26020ba24457186de1db6c98a47e31fc365de2bb1100d73d354',
'docs/STATUS.md':'3b2828a9708c8b06ff7167ff82e5028635f2929c3ea4b407318d45aa3f31fc1e',
'docs/ROADMAP.md':'c0d454862f728901ae7411ea196e1e8dbdf52c256b2ef77690970150e1214c31',
'docs/ROADMAP.zh-CN.md':'4a4ef61767f70b022a5b7f2c4a771655d438c00896f8638cd836bb64339bf168',
 'tools/perf/README.md':'242df3e9dc9e14052aa29b76c5b409ef546784673e68f6b579c526290e1f26e5'}
for name,expected in RESULT.items():
    if hashlib.sha256(Path(name).read_bytes()).hexdigest()!=expected:
        raise SystemExit('Unexpected documentation result: '+name)
print('Documentation synchronized; historical evidence and unrelated status preserved.')
