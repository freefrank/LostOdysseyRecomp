# In-game settings driver

Windows-only scripts for checking the host settings menu (or anything else that has to be seen in gameplay) in a running build. They are an **active game driver**: `boot.py` copies the runtime into a run folder, starts it and kills it at the end; `drive.py` and `burst.py` send inputs through `LO_TEST_INPUT_FILE` and request presented screenshots. Needs Pillow for the screenshot steps.

```sh
# shell 1: boots into the field and stays up until <run>/release exists (30 minutes at most)
python tools/settings_driver/boot.py --game <disc1 folder> --stage-from <finished run folder>
# shell 2
python tools/settings_driver/drive.py p8000 w1 s:camp
python tools/settings_driver/burst.py
```

The run folder defaults to `out/settings-run`; the runtime to the `windows-clang` build under `out/build`. Both can be changed with `--run` / `--exe`, and every path option has an environment variable (`LO_SETTINGS_RUN`, `LO_SETTINGS_EXE`, `LO_SETTINGS_GAME`, `LO_SETTINGS_STAGE_FROM`). `--stage-from` is a finished run folder (save, profile, shaders, shader-cache, settings.ini, DLLs) that boots into the field with the automatic presses in `boot.py`; it is copied once, while the run folder has no `shaders/`. Run settings live in `<run>/settings.ini`; delete the run folder after a run that changed settings or the save. Screenshots go to `<run>/png`. Extra `KEY=VALUE` arguments to `boot.py` replace its game environment variables.

Menu path as of 2026-10-05: Y opens the camp menu, Up wraps to System, A, Down to Settings, A; wait about 3 seconds before RB switches to the graphics tab. Take a screenshot of a tab before changing values.
