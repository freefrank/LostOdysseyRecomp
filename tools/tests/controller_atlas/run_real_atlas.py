"""Check both real English controller atlases without hardcoding disc offsets."""
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[3]
inventory = json.loads((root / 'out/issue40/ui-export/inventory.json').read_text(encoding='utf-8'))
decoder = root / 'out/issue40/controller-atlas-fixture/LoControllerAtlasFixture.exe'
for package, export in (('rpmenurescommon_int.xxx', 'Icon_Page_0'),
                        ('rpfontscommon_int.xxx', 'Texture2D_1')):
    entry = next(x for x in inventory['selected_entries'] if x['disc'] == 'disc1' and
                 x['path'].endswith(package))
    subprocess.run([str(decoder), str(root / 'LostOdysseyRecompLib/private' / entry['disc'] / entry['archive']),
                    str(entry['offset']), str(entry['length']), export], check=True)
