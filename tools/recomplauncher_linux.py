"""Build the Linux launcher profile from the shared graphics settings."""
import io
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def profile():
    result = json.loads((ROOT / 'recomplauncher.json').read_text(encoding='utf-8'))
    result['start'] = {
        'program': ['LostOdysseyRecomp.AppImage'],
        'env': {'LO_LOG_DIR': '{install}/logs'},
    }
    result['prepare']['program'] = ['LostOdysseyRecomp.AppImage']
    result['config'][0]['file'] = [{'path': '{config}/lost-odyssey-recomp/settings.ini'}]
    result['saves'] = ['{local}/lost-odyssey-recomp/save']
    result['folders'] = {
        'saves': [{'path': result['saves'][0]}],
        'mods': [{'path': '{local}/lost-odyssey-recomp/mods'}],
        'logs': [{'path': '{install}/logs'}],
    }
    # Linux releases have Vulkan DLSS/FSR SR, but no XeSS, neural rendering
    # or frame generation. Keep shared row IDs and persisted values in sync.
    excluded = {'backend', 'xess_quality', 'frame_generation', 'frame_generation_multiplier'}
    result['settings'] = [row for row in result['settings']
                          if row['id'] not in excluded and not row['id'].startswith('dlss_n')]
    aa = next(row for row in result['settings'] if row['id'] == 'antialiasing')
    aa['choices'] = [choice for choice in aa['choices'] if choice['set'].get('upscaler') != '4']
    aa['help'] = 'Temporal upscaling replaces ordinary anti-aliasing where supported. DLSS needs a compatible NVIDIA RTX GPU. Unsupported requests fall back to ordinary anti-aliasing.'
    result['looks']['reason'] = 'The game uses Vulkan and has its own anti-aliasing, upscaling and HDR settings.'
    return result


def package(image, destination):
    """The launcher reads a loose profile, not files inside an AppImage."""
    with tarfile.open(destination, 'w:gz') as archive:
        entry = archive.gettarinfo(image, arcname='LostOdysseyRecomp.AppImage')
        entry.mode = 0o755
        with image.open('rb') as source:
            archive.addfile(entry, source)
        data = (json.dumps(profile(), ensure_ascii=False, indent=2) + '\n').encode('utf-8')
        entry = tarfile.TarInfo('recomplauncher.json')
        entry.mode, entry.size = 0o644, len(data)
        archive.addfile(entry, io.BytesIO(data))
