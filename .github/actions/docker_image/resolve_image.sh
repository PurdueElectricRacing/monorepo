#!/usr/bin/env bash
set -euo pipefail

REPO="ghcr.io/purdueelectricracing/monorepo/per-firmware"
HASH="$(sha256sum Dockerfile requirements.txt | sha256sum | cut -c1-12)"
TAG="deps-${HASH}"
IMAGE="${REPO}:${TAG}"

CAN_PUSH="false"
if [ "${EVENT_NAME}" != "pull_request" ] || [ "${HEAD_REPO:-}" = "${BASE_REPO}" ]; then
  CAN_PUSH="true"
fi

IS_MASTER_PUSH="false"
TAGS="${IMAGE}"
if [ "${EVENT_NAME}" = "push" ] && [ "${GITHUB_REF}" = "refs/heads/master" ]; then
  IS_MASTER_PUSH="true"
  TAGS="${IMAGE},${REPO}:latest"
fi

write_outputs() {
  local should_build="$1"
  local resolved_image="$2"
  {
    echo "should_build=${should_build}"
    echo "image=${resolved_image}"
    echo "tags=${TAGS}"
  } >> "${GITHUB_OUTPUT}"
}

if DIGEST="$(docker buildx imagetools inspect "${IMAGE}" --format '{{.Manifest.Digest}}' 2>/dev/null)" && [ -n "${DIGEST}" ]; then
  echo "Found existing CI image: ${IMAGE} (${DIGEST})"
  if [ "${IS_MASTER_PUSH}" = "true" ] && [ "${CAN_PUSH}" = "true" ]; then
    LATEST_DIGEST="$(docker buildx imagetools inspect "${REPO}:latest" --format '{{.Manifest.Digest}}' 2>/dev/null || true)"
    if [ "${DIGEST}" != "${LATEST_DIGEST}" ]; then
      echo "Updating ${REPO}:latest -> ${IMAGE} (${DIGEST})"
      docker buildx imagetools create -t "${REPO}:latest" "${IMAGE}"
    else
      echo "${REPO}:latest already points to ${DIGEST}; skipping push."
    fi
  fi
  write_outputs "false" "${IMAGE}"
  exit 0
fi

echo "CI image ${IMAGE} not found in registry."
if [ "${CAN_PUSH}" != "true" ]; then
  echo "::warning::Fork PR modified Docker dependencies; falling back to ${REPO}:latest"
  write_outputs "false" "${REPO}:latest"
  exit 0
fi

write_outputs "true" "${IMAGE}"
