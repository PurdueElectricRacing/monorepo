# Building the PER CI Container Image in GitHub Actions

## Goal

Replace the externally-maintained `irvingywang/per-firmware:latest` Docker image with an
image that CI builds and caches from the repo's own `Dockerfile`, and that all CI
workflows consume. This removes:

- The manual step of an individual periodically rebuilding/pushing to a personal Docker
  Hub account.
- Drift between the `Dockerfile` in the repo and the image CI actually runs on.
- A dependency on a personal account (bus factor, credentials, access).

## Current State

| Item | Detail |
|---|---|
| Dockerfile | Repo root `Dockerfile`, based on `ubuntu:26.04`. Installs apt toolchain (g++, clang-format, cmake, cppcheck, arm-none-eabi-gcc, ninja), a Python venv at `/opt/venv` from `requirements.txt`, and Rust `stable` at `/opt/cargo` + `/opt/rustup`. |
| Consumers | 4 workflows, 8 job definitions use `container: image: irvingywang/per-firmware:latest` |

| Workflow | Jobs using the image |
|---|---|
| `.github/workflows/run_tests.yml` | `fil_tests`, `unit_tests` |
| `.github/workflows/build_daq.yml` | `daqcloud_schema`, `superdbc_models`, `build`, `fmt`, `clippy` |
| `.github/workflows/build_firmware.yml` | `firmware_build`, `cppcheck` |
| `.github/workflows/release_firmware.yml` | (does not use the container; unchanged) |

## Why We Need a Registry Push (the core constraint)

`jobs.<id>.container.image` is resolved **when the job starts**, before any step runs.
Therefore the image cannot be produced by a step inside the consuming job. The
supported pattern ("passing an image between jobs") is:

1. An **`image` job** builds the Dockerfile and pushes it to a registry.
2. Consumer jobs declare `needs: [image]` and reference
   `container: image: ${{ needs.image.outputs.image }}`.

A Docker registry is mandatory. **GitHub Container Registry (GHCR)** is the natural
choice:

- No account, token, or "link to CI/CD" setup is required — every GitHub org has an
  implicit GHCR (`ghcr.io/purdueelectricracing/...`).
- Auth is the per-run `GITHUB_TOKEN`, which — once the workflow declares
  `packages: write` — is sufficient to push/pull, so there are no personal secrets or
  tokens to manage. The composite action still performs an explicit
  `docker/login-action` step because `docker/build-push-action` does not authenticate
  with any registry by itself.
- Free for public repos.
- **Cross-run, cross-workflow BuildKit layer cache (`type=gha`)** scoped to the repo:
  the first build of a given Dockerfile revision takes minutes; every later build
  (any PR, any branch, any workflow) is a near-instant cache hit. This is the "lot of
  caching" layer on top of the existing `actions/cache` cargo caches.

## Architecture

```
PR / push to master
        │
        ▼
┌─────────────┐   ┌──────────────┐   ┌────────────────────────┐
│  image job  │──▶│ GHCR         │◀──┤  image job             │   (each of the three main
│  (build +   │   │ gha BuildKit │   │  (each other main      │    workflows carries the
│   push)     │   │ layer cache  │   │  workflow)             │    same image job; same
└─────────────┘   └──────┬───────┘   └────────────────────────┘    key → cache hit,
                                                                 near-instant)
                         │ pull (sha-pinned)
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
      fil_tests      build / clippy   firmware_build
      (container:    (container:      (container:
      needs.image)   needs.image)     needs.image)
```

- Each of the three main workflows (`run_tests`, `build_daq`, `build_firmware`)
  runs an `image` job on every PR and master push; BuildKit's `gha` cache makes
  builds of an unchanged image near-instant.
- Tags: `sha-<commit sha>` on every build; `latest` **only** on pushes to `master`.
  `latest` stays fresh because the main workflows trigger on every master push.
  PRs and feature branches never touch `latest`.
- Fork PRs: `github.repository` is the *base* repository on fork PRs, and the
  `GITHUB_TOKEN` is read-only there, so the `image` job carries
  `if: github.event_name != 'pull_request' ||
  github.event.pull_request.head.repo.full_name == github.repository` and is skipped
  for fork PRs (the containerized jobs that `needs: [image]` are skipped as a result,
  keeping the workflow green). PRs from branches of the org repo and master pushes
  are unaffected.

## Files to Create

### 1. `.github/actions/per-image/action.yml` (new — composite action, single source of build logic)

```yaml
name: Build PER CI image
description: Builds the repo Dockerfile and pushes it to GHCR with gha layer caching.
outputs:
  image:
    description: Fully qualified image reference (name:tag).
    value: ghcr.io/${{ github.repository }}/per-firmware:${{ steps.meta.outputs.version }}
runs:
  using: composite
  steps:
    - uses: docker/setup-buildx-action@v4

    - id: meta
      uses: docker/metadata-action@v5
      with:
        images: ghcr.io/${{ github.repository }}/per-firmware
        tags: |
          type=sha,prefix=sha-
          type=raw,value=latest,enable=${{ github.event_name == 'push' && github.ref == 'refs/heads/master' }}

    - uses: docker/login-action@v3
      with:
        registry: ghcr.io
        username: ${{ github.actor }}
        password: ${{ github.token }}

    - uses: docker/build-push-action@v7
      with:
        context: .
        file: Dockerfile
        push: true
        tags: ${{ steps.meta.outputs.tags }}
        labels: ${{ steps.meta.outputs.labels }}
        cache-from: type=gha,scope=per-firmware
        cache-to: type=gha,mode=max,scope=per-firmware
```

Notes:

- `type=gha` cache is the big win: apt, pip, and rustup layers persist across runs and
  workflows. `mode=max` caches every layer including the final one.
- Cache key is tied to the `Dockerfile` content implicitly by BuildKit layer hashing —
  only changed instructions invalidate downstream layers.
- `actions/checkout@v7` must run **before** this action in the calling job (the
  composite action does not check out).

## Files to Modify

### 2. `.github/workflows/run_tests.yml`

- Add `packages: write` to the top-level `permissions:`.
- Add the `image` job:

```yaml
jobs:
  image:
    name: CI image
    runs-on: ubuntu-latest
    timeout-minutes: 30
    outputs:
      image: ${{ steps.built.outputs.image }}
    steps:
      - uses: actions/checkout@v7
      - id: built
        uses: ./.github/actions/per-image
```

- For **both** `fil_tests` and `unit_tests`:
  - Add `needs: [image]`
  - Change the container line to:

```yaml
    container:
      image: ${{ needs.image.outputs.image }}
```

- **`fil_tests` only:** the step

```yaml
      - name: Install FIL PGO tools
        run: |
          apt-get update
          apt-get install -y --no-install-recommends make clang llvm libclang-rt-dev
          ...
```

  re-installs packages on every run. Move `make clang llvm libclang-rt-dev` into the
  `Dockerfile` (they are stable CI dependencies) and delete that step; leave the
  `--version` sanity echoes as plain `run` lines if desired.

### 3. `.github/workflows/build_daq.yml`

- Add `packages: write` to `permissions:`.
- Add the same `image` job as above.
- For all five jobs (`daqcloud_schema`, `superdbc_models`, `build`, `fmt`, `clippy`):
  add `needs: [image]` and point `container: image:` at
  `${{ needs.image.outputs.image }}`.
- No other changes; the existing `actions/cache` cargo steps complement the image
  cache and should stay.

### 4. `.github/workflows/build_firmware.yml`

- Add `packages: write` to `permissions:`.
- Add the same `image` job.
- For `firmware_build` and `cppcheck`: add `needs: [image]`, swap the container line.

### 5. `Dockerfile` (small hardening while we own it)

- Add near the top:

```dockerfile
ENV DEBIAN_FRONTEND=noninteractive
```

- (Recommended) Move the FIL PGO tools in:

```dockerfile
RUN apt -y install --no-install-recommends make clang llvm libclang-rt-dev
```

- (Optional) Pin the base image by digest (`ubuntu:26.04@sha256:...`) for
  fully-deterministic toolchains across time. If done, update the digest on a
  deliberate cadence. The `type=sha` tagging already makes every CI run reproducible
  within a commit even without this.
- (Optional) Document the policy in a header comment: "The Rust `stable` toolchain and
  apt set are only refreshed when this Dockerfile changes. That is intentional — CI
  toolchain stability."

## What NOT to Change

- `release_firmware.yml`, `deploy_coverage.yml`, `deploy_doxygen.yml`, `label_prs.yml`
  — they do not use the container image.
- `actions/checkout@v7` with `submodules: recursive` in consumer jobs — unchanged.
- The `Fix git ownership` steps — still required; container root ownership applies the
  same way.
- Existing `actions/cache` cargo steps — keep; layer cache and action cache are
  independent and complementary.

## Caching Model (what makes this fast)

| Cache | Scope | What it caches |
|---|---|---|
| BuildKit `type=gha` (`cache-from`/`cache-to`) | Per-repo, cross-run, cross-workflow | Every Docker layer: apt packages, pip venv, rustup install, toolchains. First build of a Dockerfile revision ≈ minutes; subsequent builds ≈ seconds. |
| GHCR image pull | Per-job | Pull of the `sha-<sha>` image (~1–2 GB). GitHub runners pull from GHCR fast; unavoidable with `container:` jobs. |
| `actions/cache` (existing) | Per-job, keyed on `daq/Cargo.lock` | `/opt/cargo/registry`, `/opt/cargo/git`, `daq/target`. Survives image rebuilds; keyed to dependency versions, not image layers. |
| `actions/cache` diesel-cli (existing) | Per-job | Diesel CLI install dir. |

Net effect per CI run: one near-instant image build (cache hit) + one GHCR push (cache
max) + one pull per job + existing cargo caches. No periodic manual image refreshes.

## Verification Plan (do each of these before deleting the old image)

1. **Org PR test:** push the changes to a branch of `PurdueElectricRacing/monorepo`,
   open a PR, watch all workflows. Confirm:
   - `image` job builds and pushes `ghcr.io/purdueelectricracing/monorepo/per-firmware:sha-<sha>`.
   - All consumer jobs run inside it (no missing binaries: `cargo`, `g++`,
     `arm-none-eabi-gcc`, `python3 -m pytest`, etc.).
   - Second PR/commit: BuildKit log shows cache hits for apt/pip/rustup layers.
2. **Fork PR test (the easy-to-miss case):** open a PR from a fork. Confirm the `image`
   job is SKIPPED (fork tokens are read-only on the base repo's GHCR) and the
   containerized jobs are skipped as a result — the workflow shows skipped/green, not
   failing.
3. **Master test:** merge to master. Confirm:
   - `:latest` is minted (only `push` + `refs/heads/master` enables it).
   - `release_firmware.yml` still fires off the successful Build Firmware run.
4. **Local dev test:**
   `docker pull ghcr.io/purdueelectricracing/monorepo/per-firmware:latest` and run a
   local build/test inside it, confirming it replaces the personal image for dev use.

## Rollout / Migration Sequence

1. Land the composite action and migrate `build_firmware.yml` first as a canary;
   verify the `image` job builds and pushes successfully and consumers consume it.
2. Migrate `run_tests.yml` (includes the FIL apt-install move into the Dockerfile).
   Verify.
3. Migrate `build_daq.yml` (most jobs, but mechanical). Verify.
4. Update `docs/setup.md` (or equivalent dev docs): pull
   `ghcr.io/purdueelectricracing/monorepo/per-firmware:latest` for local work.
5. After a week of green CI, deprecate `irvingywang/per-firmware` on Docker Hub
   (mark deprecated / delete) and remove any references in docs.

## Rollback

Reverting the workflow files restores the old `container: image:` references; the old
personal image must still be pulled until the first revert is green. Keep the old image
alive until step 5.

## Risks / Trade-offs

- **Per-job image pull (~1–2 GB, 20–60s)** is inherent to `container:` jobs. If it
  ever becomes a real bottleneck, options in order of effort: merge small jobs
  (`fmt` into `clippy`), or refactor jobs to `runs-on: ubuntu-latest` + `docker run`
  per step with the locally built image (larger refactor of mounts/env per step).
- **Fork PRs cost a full image build on first contact per fork** (no shared gha cache
  across repos — the cache is per-repo). Subsequent commits in the same fork PR are
  fast.
- **Fork PRs do not run the containerized CI jobs** (the image build is skipped because
  fork tokens cannot push to the base repo's GHCR); contributors using forks get no
  automated CI for those jobs. This is accepted because the team's normal workflow is
  branches of the org repo, where full CI runs.
- **GHCR `packages: write` scope:** ensure the token context is the org repo (it is, by
  default, for org workflows) and the repo remains public (free pulls are unlimited
  for public repos regardless).
