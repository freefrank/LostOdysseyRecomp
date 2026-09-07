"""Regenerate the embedded SMAA source from the pinned upstream shader."""
from pathlib import Path

root = Path(__file__).resolve().parent
source = (root / "SMAA.hlsl").read_bytes().decode("latin1")
for name, register, filtering in (("Linear", 0, "MIN_MAG_LINEAR_MIP_POINT"), ("Point", 1, "MIN_MAG_MIP_POINT")):
    old = f"SamplerState {name}Sampler {{ Filter = {filtering}; AddressU = Clamp; AddressV = Clamp; }};"
    assert source.count(old) == 1
    source = source.replace(old, f"SamplerState {name}Sampler : register(s{register});")
(root / "SMAA_source.h").write_text('#pragma once\nstatic const char smaaSource[] = R"SMAA(\n' + source + '\n)SMAA";\n', encoding="utf8")
