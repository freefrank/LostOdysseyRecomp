# Lost Odyssey rendering workflow for OpenCode

Maintained source for the `lo-render-flicker` skill and three agents. The project deliberately ignores `.opencode/` local configuration and state, so the portable source lives here and installs only four named Markdown files.

From the repository root, with Python 3.10+:

```powershell
python -B tools/opencode/install.py --output .opencode
```

Identical files are left alone. To update previously installed versions, add `--overwrite`; this updates only the skill and these three agents, preserving other agents, models, plugins and settings.

Start OpenCode in this repository and ask:

> 使用 lo-render-investigator，加载 lo-render-flicker skill，分析这个 F1 捕获包，完成证据支持的修正和定向验证：D:\captures\render-example.zip

`lo-render-investigator` defaults to a single-agent, script-first workflow. Coverage scripts save full JSON and print compact summaries; the agent reads only selected new or contradictory evidence. Existing shader reviews, translations and passing checks are reused. `lo-render-fixer` and `lo-render-reviewer` are optional and run only when the user requests delegation. All inherit the session's model; there is no provider dependency. The skill also works with a normal primary agent. If a running session does not discover new files, start a fresh session or reload project configuration.

The agents use the [capture tools](../capture_analysis/README.md), [shader tools](../shader_analysis/README.md), and existing focused C++ regressions. Raw captures and generated evidence belong in ignored `out/`. Candidate discovery does not automatically edit the shader whitelist or establish visual acceptance.

These definitions target the [OpenCode v2 agent format](https://opencode.ai/v2/docs/agents) and [skill discovery](https://opencode.ai/v2/docs/skills), checked against the installed CLI v2.0.16 during this task. They use `agents/` and plural `permissions`, not the old v1 agent schema. They do not migrate existing local v1 configuration.

## Validation

Use `opencode2 debug agents` from the repository to check discovery, plus the skill validator when available. Definition discovery is separate from a paid/model-backed end-to-end run. The capture tool tests and real-capture dry runs exercise the offline workflow without operating the game.
