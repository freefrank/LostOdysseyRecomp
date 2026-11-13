"""Compile the actual presentation strings with the runtime's DXC arguments.

Optional --baseline compares fixed-SDR shaders byte for byte. Compiles do not
claim that DXIL was executed on D3D12 or that Vulkan negotiated an HDR surface.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess


def strings(path):
    return dict(re.findall(r'const char \*(\w+) = R"\((.*?)\)";', path.read_text(), re.S))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dxc', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[3] / 'LostOdysseyRecomp/gpu/presentation.cpp')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = strings(args.source)
    variants = [
        ('main', source['source'], 'vertex', 'vs'),
        ('main', source['source'], 'pixel', 'ps'),
        ('main', source['source'], 'gainPixel', 'ps'),
        ('compute', source['source']+'\n#define PRESENT_GROUP_X 16\n#define PRESENT_GROUP_Y 16\n'+source['computeSource'], 'presentCompute', 'cs'),
        ('overlay', source['overlaySource'], 'vertexOverlay', 'vs'),
        ('overlay', source['overlaySource'], 'pixelOverlay', 'ps'),
        ('ui', source['uiSource'], 'vertexUi', 'vs'),
        ('ui', source['uiSource'], 'pixelUi', 'ps'),
    ]
    count = 0

    def compile_text(text, label, entry, profile, spirv):
        nonlocal count
        path = args.output / (label + '.hlsl')
        binary = args.output / (label + ('.spv' if spirv else '.dxil'))
        path.write_text(text)
        command = [str(args.dxc.resolve()), '-E', entry, '-T', profile+'_6_0', '-HV', '2021',
                   '-Wno-parentheses-equality', '-Wno-unused-value', '-all-resources-bound',
                   '-O3', '-Qstrip_debug', '-Fo', str(binary)]
        if spirv:
            command += ['-spirv', '-fspv-target-env=vulkan1.2', '-fvk-use-dx-layout']
            if profile == 'vs':
                command += ['-fvk-invert-y']
        else:
            command += ['-Qstrip_reflect']
        try:
            subprocess.run(command + [str(path)], check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as error:
            raise RuntimeError(f'{label}: DXC failed\n{error.stdout}\n{error.stderr}') from error
        count += 1
        return binary.read_bytes()

    for label, text, entry, profile in variants:
        for spirv in (False, True):
            compile_text(text, label+'-'+entry+('-vk' if spirv else '-dx'), entry, profile, spirv)
    if args.baseline:
        previous = strings(args.baseline)['source']
        # Freeze just the output flags at SDR. Other shader code (sampling,
        # display LUT, user curve and range controls) remains production code.
        def sdr(text, flags):
            text = text.replace('#define outputFlags parameters.outputFlags', f'#define outputFlags {flags}u')
            return text.replace('uint outputFlags;', 'uint unusedOutputFlags;')
        for flags, spirv in [(flags, spirv) for flags in (0, 64|256) for spirv in (False, True)]:
            suffix = '-vk' if spirv else '-dx'
            for entry, profile, tail in [('pixel', 'ps', ''), ('presentCompute', 'cs', '\n#define PRESENT_GROUP_X 16\n#define PRESENT_GROUP_Y 16\n'+source['computeSource'])]:
                def prepared(text):
                    text = sdr(text + tail, flags)
                    if not spirv:
                        text += '\n'  # Define after cbuffer, before any uses.
                        text = text.replace('// Display gamma ramp,', f'#define outputFlags {flags}u\n// Display gamma ramp,', 1)
                    return text
                label = entry+'-flags'+str(flags)+suffix
                a = compile_text(prepared(previous), 'sdr-before-'+label, entry, profile, spirv)
                b = compile_text(prepared(source['source']), 'sdr-after-'+label, entry, profile, spirv)
                if a != b:
                    raise AssertionError('SDR binary changed: '+entry+suffix)
                print('SDR byte-identical:', label, hashlib.sha256(a).hexdigest())
    print(count, 'production shader compiles PASS (DXIL and SPIR-V)')


if __name__ == '__main__':
    main()
