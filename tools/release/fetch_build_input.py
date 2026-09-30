"""CI-only private XEX provisioning; never prints the URL or file contents."""
import os
from pathlib import Path
import urllib.request
local = os.environ.get('LO_BUILD_XEX_PATH', '')
url = os.environ.get('LO_BUILD_XEX_URL', '')
if not local and not url.startswith('https://'):
    raise SystemExit('Configure the LO_BUILD_XEX_URL Actions secret with a private HTTPS Disc 1 XEX URL.')
try:
    with (Path(local).open('rb') if local else urllib.request.urlopen(url, timeout=120)) as response:
        data = response.read(32 * 1024**2 + 1)
except Exception:
    raise SystemExit('Could not retrieve the private build input. Check the Actions secret.') from None
if not data or len(data) > 32 * 1024**2:
    raise SystemExit('Private build input is empty or exceeds 32 MiB.')
target = Path('LostOdysseyRecompLib/private/disc1/default.xex')
target.parent.mkdir(parents=True, exist_ok=True)
target.write_bytes(data)
print(f'Private build input staged ({len(data)} bytes).')
