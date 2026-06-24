# JUCE 8 — Pinned Commit

This file pins the exact JUCE commit that the project builds against.
The full `third_party/JUCE/` directory is gitignored, so this file is
the only way to know (or restore) the exact version that produced a
given build.

## Pinned commit

```
3ba67d4585e9d1fbcdb26a877c7978608b1f802e
```

- **Short SHA:** `3ba67d4`
- **Date:** 2026-05-21
- **Subject:** `CI: Add juce9 branch`
- **Branch:** `master`

## How to restore this exact version

On a fresh clone, after `git clone` of this repo:

```powershell
if (-not (Test-Path -LiteralPath "third_party\JUCE")) {
    git clone https://github.com/juce-framework/JUCE.git third_party\JUCE
}
cd third_party\JUCE
git fetch --unshallow
git checkout 3ba67d4585e9d1fbcdb26a877c7978608b1f802e
cd ..\..
```

`git fetch --unshallow` is needed because the repo was originally
cloned with `--depth 1`; without it, `git checkout <sha>` will fail
with "couldn't find remote ref" if the SHA isn't in the shallow
history.

## How to upgrade

1. `cd third_party\JUCE && git checkout master && git pull`
2. Build and smoke-test (`.\build-dev.bat Debug` + run the exe).
3. If everything works, update `third_party/JUCE.commit` to the new
   `git rev-parse HEAD` and commit that change in the **main** repo
   (one-line diff, easy to review).

If a build breaks, `git checkout` the previous SHA in `third_party\JUCE`
and the project will be back to a known-good state.
