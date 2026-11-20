#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/build-rpi5-qemu.sh [--jobs N] [--install-deps] [-h|--help]

Build the project's Raspberry Pi 5 QEMU fork and verify that it provides the
raspi5b machine. On macOS, the script sets up Homebrew Bash/GNU coreutils,
Homebrew libfdt compiler/linker paths, and disables the unused Apple PVG device
(which no longer compiles against the macOS 27 SDK).

Options:
  --jobs N         Ninja workers (default: $QEMU_JOBS or 4)
  --install-deps   On macOS, install the required Homebrew packages first
  -h, --help       Show this help

The script does not install dependencies unless --install-deps is supplied.
EOF
}

die() {
    printf 'build-rpi5-qemu: %s\n' "$*" >&2
    exit 2
}

host_os=$(uname -s)
original_args=("$@")
jobs=${QEMU_JOBS:-4}
install_deps=0
while (($#)); do
    case $1 in
        --jobs|-j)
            (($# >= 2)) || die "$1 requires a number"
            jobs=$2
            shift 2
            ;;
        --install-deps)
            install_deps=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done
[[ $jobs =~ ^[1-9][0-9]*$ ]] && ((jobs <= 64)) ||
    die "invalid job count '$jobs' (use an integer from 1 to 64)"

# macOS ships Bash 3.2. The setup scripts use Bash 4 features, so re-exec this
# script under Homebrew Bash before doing any build work. Argument parsing above
# is Bash 3-compatible, so --help and invalid options never install packages.
if [[ $host_os == Darwin ]] && (( BASH_VERSINFO[0] < 4 )); then
    command -v brew >/dev/null 2>&1 || die 'Homebrew is required on macOS; install it from https://brew.sh/'
    if ((install_deps)) && [[ ${YOCTOCOUPLER_QEMU_DEPS_INSTALLED:-0} != 1 ]]; then
        brew install bash coreutils make meson ninja pkgconf glib pixman dtc python
        export YOCTOCOUPLER_QEMU_DEPS_INSTALLED=1
    fi
    brew_bash_prefix=$(brew --prefix bash 2>/dev/null || true)
    brew_bash=${brew_bash_prefix:+$brew_bash_prefix/bin/bash}
    [[ -n $brew_bash && -x $brew_bash ]] ||
        die 'Bash 4+ is required; run brew install bash, then rerun this script'
    exec "$brew_bash" "$0" "${original_args[@]}"
fi
(( BASH_VERSINFO[0] >= 4 )) || die 'Bash 4 or newer is required'
[[ $host_os == Darwin || $install_deps -eq 0 ]] ||
    die '--install-deps is only supported on macOS'

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
qemu_dir="$repo_root/tools/rpi5-machine"

if [[ $host_os == Darwin ]]; then
    command -v brew >/dev/null 2>&1 || die 'Homebrew is required on macOS; install it from https://brew.sh/'
    if ((install_deps)) && [[ ${YOCTOCOUPLER_QEMU_DEPS_INSTALLED:-0} != 1 ]]; then
        brew install bash coreutils make meson ninja pkgconf glib pixman dtc python
        export YOCTOCOUPLER_QEMU_DEPS_INSTALLED=1
    elif ((install_deps)); then
        # Bash 3 already installed the packages before re-execing us.
        :
    fi

    brew_prefix_for() {
        local formula=$1
        local prefix
        prefix=$(brew --prefix "$formula" 2>/dev/null) || return 1
        [[ -d $prefix ]] || return 1
        printf '%s\n' "$prefix"
    }

    bash_prefix=$(brew_prefix_for bash) ||
        die 'Homebrew Bash is missing; install it with brew install bash'
    coreutils_prefix=$(brew_prefix_for coreutils) ||
        die 'GNU coreutils are missing; install them with brew install coreutils'
    dtc_prefix=$(brew_prefix_for dtc) ||
        die 'Homebrew dtc/libfdt is missing; install it with brew install dtc'

    [[ -x $bash_prefix/bin/bash ]] || die "Homebrew Bash not found under $bash_prefix"
    [[ -x $coreutils_prefix/libexec/gnubin/realpath ]] ||
        die "GNU realpath not found under $coreutils_prefix; reinstall coreutils"
    export PATH="$bash_prefix/bin:$coreutils_prefix/libexec/gnubin:$PATH"

    # The standalone dtmerge target links directly with -lfdt and does not use
    # pkg-config, so expose Homebrew's libfdt headers and library explicitly.
    export CPATH="$dtc_prefix/include${CPATH:+:$CPATH}"
    export LIBRARY_PATH="$dtc_prefix/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
    export PKG_CONFIG_PATH="$dtc_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

for command_name in git python3 ninja pkg-config realpath; do
    require_command "$command_name"
done
require_command cc
realpath --relative-to=. . >/dev/null 2>&1 ||
    die 'GNU realpath with --relative-to support is required (macOS: brew install coreutils)'
python3 -c 'import venv' >/dev/null 2>&1 ||
    die 'Python 3 with the venv module is required'
pkg-config --exists glib-2.0 pixman-1 libfdt ||
    die 'QEMU build libraries are missing; install GLib, pixman, and libfdt development files'

if command -v gmake >/dev/null 2>&1; then
    make_command=$(command -v gmake)
else
    make_command=$(command -v make) || die 'GNU make is required (macOS: brew install make)'
fi
"$make_command" --version 2>/dev/null | grep -q 'GNU Make' ||
    die 'GNU make is required (macOS: brew install make)'

if [[ $host_os == Darwin && $install_deps -eq 1 ]]; then
    printf 'Homebrew dependencies are ready.\n' >&2
elif [[ $host_os != Darwin && $install_deps -eq 1 ]]; then
    die '--install-deps is only supported on macOS'
fi

printf 'Initializing the Pi 5 QEMU submodule...\n' >&2
repo_toplevel=$(git -C "$repo_root" rev-parse --show-toplevel) ||
    die "not inside a git repository: $repo_root"
repo_rel_prefix=$(git -C "$repo_root" rev-parse --show-prefix) ||
    die "could not determine repository-relative path of $repo_root"
# --show-prefix emits a trailing slash; strip it for path joins.
repo_rel_prefix=${repo_rel_prefix%/}
if [[ -n $repo_rel_prefix ]]; then
    submodule_path="$repo_rel_prefix/tools/rpi5-machine"
else
    submodule_path='tools/rpi5-machine'
fi
git -C "$repo_toplevel" submodule update --init --recursive "$submodule_path"

make_args=("NINJA=ninja -j$jobs")
configure_flags=${EXTRA_CONFIGURE_FLAGS:-}
if [[ $host_os == Darwin && " $configure_flags " != *' --disable-pvg '* ]]; then
    configure_flags="${configure_flags:+$configure_flags }--disable-pvg"
fi
if [[ -n $configure_flags ]]; then
    make_args+=("EXTRA_CONFIGURE_FLAGS=$configure_flags")
fi

if [[ $host_os == Darwin ]]; then
    printf 'Building for macOS with --disable-pvg and %s Ninja workers.\n' "$jobs" >&2
else
    printf 'Building with %s Ninja workers.\n' "$jobs" >&2
fi
"$make_command" -C "$qemu_dir" setup build "${make_args[@]}"

qemu_binary="$qemu_dir/build/qemu-system-aarch64"
[[ -x $qemu_binary ]] || die "build completed without producing $qemu_binary"
"$qemu_binary" -machine help 2>&1 | grep -q 'raspi5b' ||
    die "built QEMU does not advertise the raspi5b machine: $qemu_binary"
printf 'Ready: %s (raspi5b supported)\n' "$qemu_binary"
