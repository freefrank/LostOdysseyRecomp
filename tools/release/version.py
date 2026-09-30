"""Release tag and source version syntax shared by package tools."""

import re


VERSION_SUFFIX = r'(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?'


def normalize_release_version(version):
    match = re.fullmatch(r'v([0-9]+\.[0-9]+(?:\.[0-9]+)?)(' + VERSION_SUFFIX + r')', version)
    if not match:
        raise ValueError('Version must have the form v0.1 or v0.1.0, optionally with a suffix such as -updaterfix.')
    components = [int(x) for x in match[1].split('.')]
    components += [0] * (3 - len(components))
    return '.'.join(map(str, components)) + match[2]


def valid_source_version(version):
    return re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+' + VERSION_SUFFIX, version) is not None
