#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/run-image-in-qemu.sh [OPTIONS] [IMAGE.wic[.bz2]] [-- QEMU-OPTION...]

Boot a Yoctocoupler WIC image on the project's Raspberry Pi 5 QEMU model.
With no IMAGE, the newest unmodified Yoctocoupler WIC image is selected.
When IMAGE is explicit, also supply its matching kernel with --kernel (or
KERNEL_IMAGE). The input is copied/decompressed to a temporary SD-card image
and remains unmodified. The guest uses the serial console on ttyAMA10.
Requires Bash, cp, stat, dd, and mktemp; .wic.bz2 images also need bzip2.
The runner supports Linux and macOS with a host-compatible raspi5b QEMU binary.

Options:
  --qemu PATH          QEMU binary (default: tools/rpi5-machine/build/..., then
                       build/rpi5-machine-eval/build/...; also honors $QEMU)
  --kernel PATH        Matching kernel Image for an explicitly selected WIC
                       (default-image mode uses build/qemu/.../raspberrypi5/Image;
                       also honors $KERNEL_IMAGE)
  --ram SIZE           Guest RAM: 1G, 2G, 4G, 8G, or 16G (default: $QEMU_RAM or 2G)
  --cpus N             Guest CPUs, 1-4 (default: $QEMU_CPUS or 4)
  --ssh-forward PORT   Add an IGB NIC and forward localhost:PORT to guest SSH
                       at 192.168.50.2:22. The image needs kernel-module-igb
                       and an installed root SSH key for this to work.
  -h, --help           Show this help
  --                   Pass remaining arguments to QEMU

The QEMU model is a Pi 5 model, not a CM5 carrier/RP1 model. It does not
validate carrier Ethernet or physical-board timing.
EOF
}

die() {
    printf 'run-image-in-qemu: %s\n' "$*" >&2
    exit 2
}

absolute_path() {
    case $1 in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$PWD" "$1" ;;
    esac
}

file_size_bytes() {
    case $(uname -s) in
        Linux) stat -c %s "$1" ;;
        Darwin) stat -f %z "$1" ;;
        *) die 'only Linux and macOS are supported hosts' ;;
    esac
}

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
deploy_dir="${KAS_BUILD_DIR:-$repo_root/build/qemu}/tmp/deploy/images/raspberrypi5"
kernel_path=${KERNEL_IMAGE:-"$deploy_dir/Image"}
kernel_explicit=0
[[ -n ${KERNEL_IMAGE:-} ]] && kernel_explicit=1
qemu_override=${QEMU:-}
ram=${QEMU_RAM:-2G}
cpus=${QEMU_CPUS:-4}
ssh_forward=
image=
explicit_image=0
extra_qemu_args=()

while (($#)); do
    case $1 in
        --qemu)
            (($# >= 2)) || die '--qemu requires a path'
            qemu_override=$2
            shift 2
            ;;
        --kernel)
            (($# >= 2)) || die '--kernel requires a path'
            kernel_path=$2
            kernel_explicit=1
            shift 2
            ;;
        --ram)
            (($# >= 2)) || die '--ram requires a size'
            ram=$2
            shift 2
            ;;
        --cpus)
            (($# >= 2)) || die '--cpus requires a number'
            cpus=$2
            shift 2
            ;;
        --ssh-forward)
            (($# >= 2)) || die '--ssh-forward requires a host port'
            ssh_forward=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            extra_qemu_args=("$@")
            break
            ;;
        -* )
            die "unknown option: $1 (pass QEMU options after --)"
            ;;
        *)
            [[ -z $image ]] || die "unexpected extra argument: $1"
            image=$1
            explicit_image=1
            shift
            ;;
    esac
done

case $ram in
    1G|2G|4G|8G|16G) ;;
    *) die "unsupported --ram value '$ram' (use 1G, 2G, 4G, 8G, or 16G)" ;;
esac
[[ $cpus =~ ^[1-4]$ ]] || die '--cpus must be an integer from 1 to 4'
if [[ -n $ssh_forward ]]; then
    [[ $ssh_forward =~ ^[0-9]{1,5}$ ]] || die '--ssh-forward must be a TCP port number'
    ((10#$ssh_forward >= 1 && 10#$ssh_forward <= 65535)) ||
        die '--ssh-forward must be between 1 and 65535'
fi

if [[ -z $image ]]; then
    for candidate in \
        "$deploy_dir"/yoctocoupler-image-raspberrypi5.rootfs-*.wic \
        "$deploy_dir"/yoctocoupler-image-raspberrypi5.rootfs-*.wic.bz2; do
        [[ -f $candidate ]] || continue
        [[ $candidate == *-with-ssh-key.wic* ]] && continue
        if [[ -z $image || $candidate -nt $image ]]; then
            image=$candidate
        fi
    done
fi
[[ -n $image ]] || die "no Yoctocoupler WIC image found in $deploy_dir; build the image or pass its path"
if ((explicit_image && !kernel_explicit)); then
    die 'an explicit WIC image must be paired with its matching kernel via --kernel PATH (or KERNEL_IMAGE); otherwise the latest kernel could mismatch the image'
fi
[[ -f $image ]] || die "image not found: $image"
[[ -f $kernel_path ]] || die "kernel Image not found: $kernel_path"
image=$(absolute_path "$image")
kernel_path=$(absolute_path "$kernel_path")

if [[ -n $qemu_override ]]; then
    if [[ $qemu_override == */* ]]; then
        qemu=$qemu_override
    else
        qemu=$(command -v "$qemu_override") || die "QEMU executable not found: $qemu_override"
    fi
else
    qemu=
    for candidate in \
        "$repo_root/tools/rpi5-machine/build/qemu-system-aarch64" \
        "$repo_root/build/rpi5-machine-eval/build/qemu-system-aarch64"; do
        if [[ -x $candidate ]]; then
            qemu=$candidate
            break
        fi
    done
    [[ -n $qemu ]] || die "Pi 5 QEMU was not found. Run: scripts/build-rpi5-qemu.sh (or: git submodule update --init --recursive torque_vector/yoctocoupler/tools/rpi5-machine && make -C torque_vector/yoctocoupler/tools/rpi5-machine setup build)"
fi
[[ -x $qemu ]] || die "QEMU is not executable: $qemu"
qemu=$(absolute_path "$qemu")

machine_list=$("$qemu" -machine help 2>&1) || die "could not query QEMU machines: $qemu"
[[ $machine_list == *raspi5b* ]] || die "QEMU does not provide the required raspi5b machine: $qemu"

workdir=$(mktemp -d "${TMPDIR:-/tmp}/yoctocoupler-qemu.XXXXXX")
case $workdir in
    /*) ;;
    *) workdir="$PWD/$workdir" ;;
esac
trap 'rm -rf "$workdir"' EXIT
card_image="$workdir/sdcard.wic"
case $image in
    *.wic.bz2)
        command -v bzip2 >/dev/null || die 'bzip2 is required for .wic.bz2 images'
        bzip2 -dc "$image" > "$card_image"
        ;;
    *.wic)
        cp "$image" "$card_image"
        ;;
    *)
        die 'image must end in .wic or .wic.bz2'
        ;;
esac

image_size=$(file_size_bytes "$card_image") || die 'could not read temporary WIC size'
[[ $image_size =~ ^[0-9]+$ ]] && ((image_size > 0)) || die 'empty or invalid WIC image'
if ((image_size <= 2147483648)); then
    padded_size=1
    while ((padded_size < image_size)); do
        padded_size=$((padded_size * 2))
    done
else
    unit=524288
    padded_size=$(((image_size + unit - 1) / unit * unit))
fi
if ((padded_size > image_size)); then
    dd if=/dev/zero of="$card_image" bs=1 count=1 \
        seek=$((padded_size - 1)) conv=notrunc >/dev/null 2>&1 ||
        die 'could not pad temporary WIC image for QEMU'
fi

cmdline='console=ttyAMA10,115200 earlycon=pl011,mmio32,0x107d001000 dwc_otg.lpm_enable=0 root=/dev/mmcblk0p2 rootfstype=ext4 rootwait rw net.ifnames=0'
drive_image=${card_image//,/,,}
qemu_cmd=(
    "$qemu"
    -M raspi5b
    -m "$ram"
    -smp "$cpus"
    -kernel "$kernel_path"
    -append "$cmdline"
    -drive "if=sd,format=raw,file=$drive_image"
    -nographic
    -monitor none
    -no-reboot
)
if [[ -n $ssh_forward ]]; then
    printf 'Note: SSH forwarding needs kernel-module-igb and an authorized root key in the guest image.\n' >&2
    qemu_cmd+=(
        -netdev "user,id=net0,net=192.168.50.0/24,host=192.168.50.1,dhcpstart=192.168.50.100,hostfwd=tcp:127.0.0.1:${ssh_forward}-192.168.50.2:22"
        -device igb,netdev=net0,bus=pcie1.0
    )
fi
qemu_cmd+=(${extra_qemu_args[@]+"${extra_qemu_args[@]}"})

printf 'Booting %s on raspi5b; temporary SD copy is %s bytes.\n' "$image" "$padded_size" >&2
printf 'QEMU: ' >&2
printf '%q ' "${qemu_cmd[@]}" >&2
printf '\n' >&2
"${qemu_cmd[@]}"
