#pragma once

namespace updater::shader_pack
{
// Installs shaders/pipelines_corpus.bin, the shipped pipeline recipe corpus the
// renderer reads at startup, from the shader-packs index when it is missing, and
// replaces it when the index lists another file (at most one check a day, only
// with automatic updates on). Runs on its own thread and returns at once; a file
// installed while the game runs is used from the next start. Logs one line.
void StartCorpusDownload(bool automaticUpdates);
}
