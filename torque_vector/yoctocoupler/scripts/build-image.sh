#!/usr/bin/env bash
set -Eeuo pipefail

readonly KAS_COMMIT=59e55e4f4e38d659e04de0018c9e5671a2a2fedb
readonly KAS_SHA256=5309424870c8d445800d4d98d064b0f4e3f0126557179ccce686ad4afb6c97e2
readonly KAS_URL="https://raw.githubusercontent.com/siemens/kas/${KAS_COMMIT}/kas-container"

usage() {
    cat <<'EOF'
Usage: scripts/build-image.sh [--profile production|qemu]

Download (if needed) and run the checksum-pinned KAS container wrapper to build
a Yoctocoupler CM5 image. The production profile is the default. Docker is
required.

Options:
  --profile PROFILE  Image profile: production (hardware) or qemu (default:
                     production; each has a separate default build directory)
  -h, --help         Show this help

Environment:
  KAS_IMAGE_VERSION            KAS container image tag (default: 4.7)
  KAS_BUILD_DIR                Build directory (default: <repo>/build for
                               production, <repo>/build/qemu for qemu)
  KAS_CONTAINER                Optional local wrapper path; it must match the
                               pinned KAS wrapper checksum
  YOCTOCOUPLER_SSH_PUBLIC_KEY  Optional root SSH public key to add to the image
EOF
}

die() {
    printf 'build-image: %s\n' "$*" >&2
    exit 2
}

absolute_path() {
    case $1 in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$PWD" "$1" ;;
    esac
}

sha256_of() {
    local output
    if command -v sha256sum >/dev/null 2>&1; then
        output=$(sha256sum "$1") || return 1
    elif command -v shasum >/dev/null 2>&1; then
        output=$(shasum -a 256 "$1") || return 1
    else
        die 'sha256sum or shasum is required to verify the KAS wrapper'
    fi
    printf '%s\n' "${output%% *}"
}

has_expected_checksum() {
    local actual
    actual=$(sha256_of "$1") || return 1
    [[ $actual == "$KAS_SHA256" ]]
}

profile=production
while (($#)); do
    case $1 in
        --profile)
            (($# >= 2)) || die '--profile requires production or qemu'
            profile=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *) die "unknown option: $1" ;;
    esac
done

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
caller_dir=$PWD
case $profile in
    production)
        kas_config=kas/yoctocoupler-cm5.yml
        default_build_dir="$repo_root/build"
        ;;
    qemu)
        kas_config=kas/yoctocoupler-cm5-qemu.yml
        default_build_dir="$repo_root/build/qemu"
        ;;
    *) die "unknown profile '$profile' (choose production or qemu)" ;;
esac

command -v docker >/dev/null 2>&1 || die 'Docker is required; install/start Docker and retry'

kas_container=${KAS_CONTAINER:-}
if [[ -n $kas_container ]]; then
    case $kas_container in
        /*) ;;
        *) kas_container="$caller_dir/$kas_container" ;;
    esac
    [[ -f $kas_container && -x $kas_container ]] ||
        die "KAS_CONTAINER must point to an executable file: $kas_container"
    has_expected_checksum "$kas_container" ||
        die "KAS_CONTAINER does not match the pinned KAS wrapper checksum: $kas_container"
else
    command -v curl >/dev/null 2>&1 || die 'curl is required to download the pinned KAS wrapper'
    if [[ -n ${XDG_CACHE_HOME:-} ]]; then
        cache_root=$XDG_CACHE_HOME
    else
        [[ -n ${HOME:-} ]] || die 'HOME must be set to choose a KAS wrapper cache directory'
        cache_root="$HOME/.cache"
    fi
    cache_root=$(cd "$caller_dir" && absolute_path "$cache_root")
    cache_dir="$cache_root/yoctocoupler"
    mkdir -p "$cache_dir"
    kas_container="$cache_dir/kas-container-$KAS_COMMIT"

    if ! has_expected_checksum "$kas_container" 2>/dev/null; then
        temporary_wrapper=$(mktemp "$cache_dir/kas-container.XXXXXX")
        cleanup() {
            [[ -z ${temporary_wrapper:-} ]] || rm -f "$temporary_wrapper"
        }
        trap cleanup EXIT
        printf 'Downloading pinned KAS container wrapper...\n'
        curl --fail --location --silent --show-error "$KAS_URL" --output "$temporary_wrapper" ||
            die 'failed to download the pinned KAS wrapper'
        has_expected_checksum "$temporary_wrapper" ||
            die 'downloaded KAS wrapper failed SHA-256 verification'
        chmod 0755 "$temporary_wrapper"
        mv -f "$temporary_wrapper" "$kas_container"
        temporary_wrapper=
        trap - EXIT
    fi
    chmod 0755 "$kas_container"
fi

build_dir=${KAS_BUILD_DIR:-$default_build_dir}
case $build_dir in
    /*) ;;
    *) build_dir="$caller_dir/$build_dir" ;;
esac
export KAS_BUILD_DIR="$build_dir"
export KAS_IMAGE_VERSION=${KAS_IMAGE_VERSION:-4.7}

runtime_args=()
if [[ -n ${YOCTOCOUPLER_SSH_PUBLIC_KEY:-} ]]; then
    runtime_args+=(--runtime-args '-e YOCTOCOUPLER_SSH_PUBLIC_KEY')
    printf 'A public SSH key was supplied; forwarding it to the build container.\n'
else
    printf 'No SSH public key supplied; the image root account will remain locked.\n'
fi

printf 'Building the %s Yoctocoupler CM5 image with KAS %s...\n' "$profile" "$KAS_IMAGE_VERSION"
cd "$repo_root"
"$kas_container" ${runtime_args[@]+"${runtime_args[@]}"} build "$kas_config"
printf 'Build complete. Image artifacts are in %s/tmp/deploy/images/raspberrypi5/.\n' "$KAS_BUILD_DIR"
