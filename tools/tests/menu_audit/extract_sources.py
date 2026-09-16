"""Build host-only fixtures from the current production function bodies.

No game assets, SDL, fonts, or graphics SDKs are required. These are logic tests,
not a runtime build or real-device/driver tests. Services/backends are test doubles.
"""
from pathlib import Path
import argparse
import re


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    # Mask comments and string/character literals before counting braces.
    masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda m: ' ' * len(m.group()), source, flags=re.S)
    opening = masked.index('{', start)
    depth = 0
    for i in range(opening, len(masked)):
        depth += (masked[i] == '{') - (masked[i] == '}')
        if depth == 0:
            return source[start:i + 1] + '\n'
    raise ValueError(f'Unclosed function: {signature}')


def extract(root: Path, out: Path) -> None:
    runtime = root / 'LostOdysseyRecomp'
    hid = (runtime / 'hid/hid.cpp').read_text(encoding='utf-8')
    overlay = (runtime / 'debug/menu_overlay.cpp').read_text(encoding='utf-8')
    video = (runtime / 'gpu/video.cpp').read_text(encoding='utf-8')
    out.mkdir(parents=True, exist_ok=True)
    # Keep the dispatcher's real state declaration and the complete function.
    dispatch = hid[hid.index('static hid::ButtonQuarantine s_buttonQuarantine;'):
                   hid.index('void hid::PumpHostInput()')]
    logic = overlay[overlay.index('namespace debug_menu'):
                    overlay.index('    // Render debug overlay onto')]
    state = video[video.index('        struct PresentationDisplayState {'):
                  video.index('        constexpr plume::RenderFormat kSwapChainFormat')]
    prepare = state + function(video, '    static bool PreparePresentation(')
    frame = function(video, '    static bool StoreCpuFrame(')
    for name, text in [('input_dispatch.inc', dispatch), ('overlay_logic.inc', logic + '}\n'),
                       ('video_prepare.inc', prepare), ('cpu_frame.inc', frame)]:
        (out / name).write_text('// Generated from production source; do not edit.\n' + text, encoding='utf-8')

    # Check the wiring outside the extracted functions as well. In particular,
    # a tested helper must not be bypassed by one of the production entry points.
    for signature in ('    void PresentFrontbuffer(', '    void PresentHostOverlay('):
        body = function(video, signature)
        assert body.count('PreparePresentation(displayTicket)') == 1, signature
        assert body.index('PreparePresentation(displayTicket)') < body.index('g_swapChain->getWidth()'), signature
        assert 'StoreCpuFrame(menuPresentBuffer, menuWidth, menuHeight)' in body, signature
        assert 'g_displayChanges.PresentationTicket()' not in body, signature
        assert 'g_swapChain->resize()' not in body, signature
    upload = function(video, '    static bool UploadAndPresentPixels(')
    assert 'g_swapChain->resize()' not in upload
    assert 'pixels.size() != size_t(width) * height' in upload
    sample = function(hid, 'uint32_t hid::GetState(')
    assert sample.index('s_buttonQuarantine.Capture()') < sample.index('SDL_GameControllerGetAttached')
    assert 's_buttonQuarantine.Filter(gp.wButtons, quarantineBeforeSample)' in sample
    assert 's_buttonQuarantine.ObserveRelease' not in sample
    assert 's_buttonQuarantine.Consume' not in sample
    assert 'g_keys' not in function(hid, 'static uint16_t ReadRawButtonsLocked(')
    print('PASS source wiring: unique keyboard route, read-only guest quarantine, shared prepare and frame publication')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    extract(args.root, args.out)
