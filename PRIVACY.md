# Privacy and diagnostic collection

Lost Odyssey Recomp's log collection is optional and off until you agree. The game asks once, in five languages, and **Settings → System → Log collection** turns it on or off at any time. It is available in Windows builds only. The game creates no player, installation or device identifier.

## What is sent

When the game starts, a background thread sends one request to `lo.dotslash.pro` with a filtered summary of the previous session's runtime log:

- build version, Windows version, Wine version under Proton, GPU name and driver, and a one-line summary of the graphics settings;
- map and battle loads;
- rendering mismatch records: shader IDs and draw state for depth writers the TAA mapping does not know, and for mapped shaders whose jitter was rejected;
- error lines, crash reports and hang reports, and whether the game shut down cleanly.

Repeated lines are sent once and the summary is limited to 60 KB. Before sending, the game replaces your user profile folder, your account name, your computer name and any folder name after `Users\` or `home/`. Other folder names in paths, such as where the game is installed, can remain. Information-level and kernel lines are never sent, even with **Debug log** on.

The request does not contain saves, profiles, screenshots, render captures, personal files, account names, email addresses, serial numbers, MAC addresses or device IDs.

## Storage

Records are stored in Cloudflare D1, keyed by the SHA-256 of the text. A record is deleted 30 days after it was last uploaded. The private GitHub repository `LostOdysseyRecomp-build-inputs`, open only to the maintainers and CI, keeps copies for research. These copies do not expire, and removing a file later does not erase Git history.

The Worker reads `CF-Connecting-IP` only for the platform's temporary rate limiter and does not store it. Workers Logs, invocation logs and traces are off. Cloudflare still processes your IP address and normal HTTPS metadata to deliver and protect the service.

## Earlier TAA collection

Until 2026-10-09 the same consent sent TAA shader data (shader hashes, original VS/PS microcode, GPU and driver, matrices and sparse depth samples). That collection has ended: the server now rejects it, and an earlier "yes" does not turn on log collection. Its D1 records expire within 30 days; the private archive keeps its copies.

## Local files

Runtime logs in `logs/` and **Capture render state** archives in `captures/` stay on your computer unless you share them. They can contain local paths, so look through them before attaching them to a report.

See the [Chinese privacy statement](PRIVACY.zh-CN.md) and the [log collector description](tools/taa-collector/README.md).
