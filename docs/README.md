# Documentation

[English README](../README.md) · [简体中文](../README.zh-CN.md) · [Research notes / 专项笔记](notes/README.md)

## Start here

| Need | Maintained entry point |
|---|---|
| Install, import or update | [Installation](INSTALLING.md) / [安装指南](INSTALLING.zh-CN.md) |
| Build or run from source | [Build guide](BUILDING.md) |
| Determine what is implemented, validated, accepted or published | [Status ledger](STATUS.md) |
| Read delivered changes | [Changelog](../CHANGELOG.md) |
| Find outstanding work | [Roadmap](ROADMAP.md) / [简体中文](ROADMAP.zh-CN.md), with the [Maintainer Project](https://github.com/users/freefrank/projects/3) |
| Find a subsystem investigation | [Complete notes index](notes/README.md), grouped by topic and document role |
| See every cutscene played on three machines | [Cutscene tour](CUTSCENE_TOUR.md) / [过场巡游](CUTSCENE_TOUR.zh-CN.md) |
| Find development, analysis or test commands | [Tool catalog](../tools/README.md) / [Test catalog](../tools/tests/README.md) |
| Understand diagnostics and consent | [Privacy](../PRIVACY.md) / [隐私说明](../PRIVACY.zh-CN.md) |
| Create a mod | [Modding guide](wiki/Modding.md) |

## Document roles and maintenance

`STATUS.md` owns the current source, validation, acceptance and publication summary. The two roadmaps own remaining work; `CHANGELOG.md` owns version history. Issue closure, merged code, a successful build, player acceptance and release publication are separate facts. Record the source and check date when reconciling them. A closed report can retain a bounded coverage gap without becoming an open defect again.

Guides describe maintained procedures. Research notes explain mechanisms and their evidence. Dated experiments, handoffs, audits and release drafts retain their original results and a visible historical label. A historical “current”, “unreleased”, failed gate, PID or next-step instruction refers to its checkpoint. Follow the linked current entry point before resuming it. Unfinished research is not automatically accepted or cancelled merely because it is old.

Keep established paths and anchors when practical. If a document is superseded, add its successor and preserve the original evidence; if moved, retain a short redirect and repair incoming relative links. Avoid duplicating changing version or task status throughout indexes. Source commits and stable identifiers are better references than moving line numbers. Local `out/` evidence is not shipped with the repository; label unavailable paths explicitly instead of presenting them as working download links. Do not index generated `out/` content.

The main README is English and links to the synchronized Chinese README. Existing language pairs remain paired; most engineering notes are Chinese. Third-party and skill-submodule documentation remains maintained upstream. Follow any workspace or session `AGENTS.md` instructions and the versioned [engineering principles](LORecomp_ENGINEERING_PRINCIPLES.zh-CN.md).

Reusable investigation methods and constraints may also be summarized in Basic Memory, with source paths, dates and applicability limits. Search for existing notes before creating another entity. Repository documents retain technical evidence and current project state; the private knowledge base is not a release ledger. The [2026-09-30 documentation review](audits/documentation-2026-09-30.md) records this normalization and its knowledge-base handoff.

## Engineering and delivery references

| Document | Scope |
|---|---|
| [Engineering principles](LORecomp_ENGINEERING_PRINCIPLES.zh-CN.md) | Correctness, performance, supported data and proportionate validation |
| [Portable shader packs](PORTABLE_SHADER_PACK.md) | Vulkan/DX12 distribution, tools and historical pack measurements |
| [Repository synchronization](PUBLISHING.md) | Remote selection and publication workflow |
| [Release packaging](../tools/release/README.md) | Package assembly and release-note extraction; the release workflow runs on Gitea Actions and publishes to GitHub |
| [Pull request checks and releases](notes/ci-gitea.md) | Gitea Actions workflows (PR checks and release packaging), runners and the `gitea/<workflow>` status reported back to GitHub |
| [AppImage compatibility](../packaging/linux/APPIMAGE_COMPATIBILITY.md) | ABI baseline, packaging evidence and hardware limits |
| [Project synchronization](project-management/README.md) | Manifest ownership, conflict handling and live state reconciliation |
| [TAA live debug](TAA_LIVE_DEBUG.md) | Local panel reference plus dated Bell/Uhra experiments |
| [Font provenance](../LostOdysseyRecomp/install/FONT-PROVENANCE.md) | Installer font source and licensing evidence |
| [Bug report template](../.github/ISSUE_TEMPLATE/bug_report.md) / [Feature request template](../.github/ISSUE_TEMPLATE/feature_request.md) | Public report inputs |

## Modding documentation

[Overview](wiki/Modding.md) · [Creating mods](wiki/Creating-Mods.md) · [API v1](wiki/Modding-API.md) · [External managers / MO2](wiki/Mod-Organizer-2.md) · [Validation](wiki/Modding-Validation.md) · [General runtime texture work](wiki/Runtime-Texture-Replacement.md)

These are maintained Wiki source pages. API support, actual game coverage and Wiki publication are separate; a proposed general texture path is not an implemented consumer.

## Tool documentation

Use the [tool catalog](../tools/README.md) to choose a tool and check its side effects. The following list includes the maintained tool guides and workflow definitions.

| Area | Guides and definitions |
|---|---|
| Build dependencies | [Dependency patches](../tools/patches/README.md), [FSR offline build](../tools/fsr/README.md) |
| Performance | [Assembly profiler](../tools/asm-profiler/README.md) / [中文](../tools/asm-profiler/README.zh-CN.md), [City capture](../tools/perf/README.md) |
| Rendering and resources | [F1 analysis](../tools/capture_analysis/README.md), [Shader analysis](../tools/shader_analysis/README.md), [UI export](../tools/ui_assets/README.md) |
| Reverse engineering | [Ghidra](../tools/ghidra/README.md), [Issue #12 diagnostics](../tools/diagnostics/issue12/README.md) |
| Automated collection | [Log collector](../tools/taa-collector/README.md), [Private feedback archive](../tools/feedback_archive/README.md) |
| Feedback workflow | [Triage skill](../tools/feedback_archive/skills/lo-feedback-triage/SKILL.md), [Review rules](../tools/feedback_archive/skills/lo-feedback-triage/references/review-rules.md), [Compact diagnostics](../tools/feedback_archive/skills/lo-feedback-triage/references/compact-diagnostics.md) |
| OpenCode workflow | [Setup](../tools/opencode/README.md), [Render skill](../tools/opencode/skills/lo-render-flicker/SKILL.md), [Investigator](../tools/opencode/agents/lo-render-investigator.md), [Fixer](../tools/opencode/agents/lo-render-fixer.md), [Reviewer](../tools/opencode/agents/lo-render-reviewer.md) |
| Focused fixtures | [Test catalog](../tools/tests/README.md), [Controller atlas](../tools/tests/controller_atlas/README.md), [Streamline probe](../tools/tests/streamline_fg/README.md) |
| Packaging | [Release tools](../tools/release/README.md), [AppImage baseline](../packaging/linux/APPIMAGE_COMPATIBILITY.md) |

## Historical records and compatibility links

| Record | Role |
|---|---|
| [Archive index](archive/README.md) | Superseded status, roadmap, release and investigation snapshots |
| [Root handoff](../HANDOFF.md) | 2026-09-22 DLSS and diagnostic checkpoint |
| [Debug menu requirements](debug-menu-requirements.md) | Original requirements and dated implementation evidence |
| [Menu boundary fixes](MENU_FIX_VALIDATION.md) | Baseline-specific repair validation |
| [Menu/shader audit](MENU_SHADER_PREBUILD_AUDIT.md) | 2026-09-16 audit |
| [0.6.0 prerelease audit](audits/0.6.0-prerelease.md) | 2026-09-18 repairs and evidence limits |
| [Initial Project import](project-management/import-audit.md) | 2026-09-08 import receipt |
| [Backlog coverage](project-management/backlog-coverage.md) / [History coverage](project-management/history-coverage.md) | Initial source inventory, not live task counts |
| [v0.1](RELEASE-v0.1.md), [v0.2](RELEASE-v0.2.md), [v0.5.0](RELEASE-v0.5.0.md), [2026-09-05 report](WORK_REPORT_2026-09-05.md) | Preserved redirects to historical records |

Every first-party research note is listed in the [notes index](notes/README.md). Historical status remains attached to the evidence it originally described.
