#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: add-ssh-key-to-image.sh [--output IMAGE] [--force] IMAGE.wic[.bz2] PUBLIC_KEY.pub

Add one or more OpenSSH public keys to /root/.ssh/authorized_keys in an
already-built Yoctocoupler WIC image. The input image is never modified. By
default, a sibling image named '*-with-ssh-key.wic[.bz2]' is created.

Options:
  -o, --output IMAGE  Choose the output image path (.wic or .wic.bz2)
      --force         Replace the output image if it already exists
  -h, --help          Show this help

The source image must use this project's DOS/MBR layout with the ext4 rootfs
in partition 2. Requires Python 3, ssh-keygen, dd, e2fsprogs (debugfs/e2fsck),
and bzip2 for .wic.bz2. On macOS, install e2fsprogs with Homebrew.
No Yocto rebuild, mount, or root privileges are required.
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

SCRIPT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd -P)

output=''
force=0
while (($#)); do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    -o|--output)
      (($# >= 2)) || die "$1 requires a path"
      output=$2
      shift 2
      ;;
    --force)
      force=1
      shift
      ;;
    --)
      shift
      break
      ;;
    -* )
      die "unknown option: $1 (see --help)"
      ;;
    *)
      break
      ;;
  esac
done

(($# == 2)) || { usage >&2; die 'provide an input WIC image and a public-key file'; }
input=$1
key_file=$2

platform=$(uname -s)
case "$platform" in
  Linux|Darwin) ;;
  *) die "unsupported operating system: $platform" ;;
esac
command -v python3 >/dev/null 2>&1 || die 'required tool not found: python3'
if [[ $platform == Linux ]]; then
  command -v realpath >/dev/null 2>&1 || die 'required tool not found: realpath'
fi
resolve_existing() {
  local path=$1 parent base
  case "$path" in
    /*) ;;
    *) path="$PWD/$path" ;;
  esac
  if [[ $platform == Linux ]]; then
    realpath -e -- "$path"
  else
    [[ -e $path ]] || return 1
    parent=$(dirname "$path")
    base=$(basename "$path")
    parent=$(cd "$parent" && pwd -P) || return 1
    printf '%s/%s\n' "$parent" "$base"
  fi
}
if ! input=$(resolve_existing "$input"); then
  die "input image not found: $1"
fi
if ! key_file=$(resolve_existing "$key_file"); then
  die "public-key file not found: $2"
fi
[[ -f $input && -r $input ]] || die "input image is not a readable regular file: $input"
[[ -f $key_file && -r $key_file ]] || die "public-key file is not a readable regular file: $key_file"

case "$input" in
  *.wic.bz2) input_compressed=1 ;;
  *.wic) input_compressed=0 ;;
  *) die 'input image must end in .wic or .wic.bz2' ;;
esac

if [[ -z $output ]]; then
  case "$input" in
    *.wic.bz2) output=${input%.wic.bz2}-with-ssh-key.wic.bz2 ;;
    *.wic) output=${input%.wic}-with-ssh-key.wic ;;
  esac
fi
case "$output" in
  /*) ;;
  *) output="$PWD/$output" ;;
esac
output_dir=$(dirname "$output")
mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd -P)
output="$output_dir/$(basename "$output")"
case "$output" in
  *.wic.bz2) output_compressed=1 ;;
  *.wic) output_compressed=0 ;;
  *) die 'output image must end in .wic or .wic.bz2' ;;
esac
[[ ! -L $output ]] || die "output path must not be a symlink: $output"
if [[ -e $output && $input -ef $output ]]; then
  die 'input and output refer to the same file; choose a separate output path'
fi
if [[ -e $output && $force -eq 0 ]]; then
  die "output already exists (use --force to replace it): $output"
fi

for tool in dd ssh-keygen; do
  command -v "$tool" >/dev/null 2>&1 || die "required tool not found: $tool"
done
DEBUGFS=$(command -v debugfs 2>/dev/null || true)
E2FSCK=$(command -v e2fsck 2>/dev/null || true)
if [[ -z $DEBUGFS || -z $E2FSCK ]]; then
  if [[ $platform == Darwin ]] && command -v brew >/dev/null 2>&1; then
    e2fsprogs_prefix=$(brew --prefix e2fsprogs 2>/dev/null || true)
    for bin_dir in "$e2fsprogs_prefix/bin" "$e2fsprogs_prefix/sbin"; do
      [[ -x $bin_dir/debugfs ]] && DEBUGFS=$bin_dir/debugfs
      [[ -x $bin_dir/e2fsck ]] && E2FSCK=$bin_dir/e2fsck
    done
  fi
fi
[[ -n $DEBUGFS ]] || die 'required tool not found: debugfs (install e2fsprogs; on macOS: brew install e2fsprogs)'
[[ -n $E2FSCK ]] || die 'required tool not found: e2fsck (install e2fsprogs; on macOS: brew install e2fsprogs)'
if ((input_compressed || output_compressed)); then
  command -v bzip2 >/dev/null 2>&1 || die 'required tool not found: bzip2'
fi

workdir=$(mktemp -d "${TMPDIR:-/tmp}/yoctocoupler-key-image.XXXXXX")
output_tmp=''
cleanup() {
  local status=$?
  trap - EXIT INT TERM
  if [[ -n ${output_tmp:-} ]]; then
    rm -f "$output_tmp"
  fi
  if [[ -n ${workdir:-} ]]; then
    rm -rf "$workdir"
  fi
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Validate every nonblank public-key line independently; ssh-keygen can return
# success for a file containing one valid key plus a malformed later line.
key_validation_file="$workdir/key_to_validate.pub"
key_count=0
line_number=0
while IFS= read -r line || [[ -n $line ]]; do
  line_number=$((line_number + 1))
  [[ $line =~ ^[[:space:]]*$ || $line =~ ^[[:space:]]*# ]] && continue
  read -r key_type key_blob _ <<< "$line"
  case "$key_type" in
    ssh-*|ecdsa-sha2-*|sk-*) ;;
    *) die "line $line_number is not an OpenSSH public-key line" ;;
  esac
  [[ -n $key_blob ]] || die "line $line_number is missing its public-key data"
  printf '%s\n' "$line" > "$key_validation_file"
  ssh-keygen -lf "$key_validation_file" >/dev/null 2>&1 || \
    die "line $line_number is not a valid OpenSSH public key"
  key_count=$((key_count + 1))
done < "$key_file"
((key_count > 0)) || die 'key file contains no OpenSSH public keys'

raw_image="$workdir/image.wic"
rootfs_image="$workdir/rootfs.ext4"
existing_keys="$workdir/existing_authorized_keys"
merged_keys="$workdir/authorized_keys"
verified_keys="$workdir/verified_authorized_keys"

if ((input_compressed)); then
  bzip2 -dc "$input" > "$raw_image"
else
  cp "$input" "$raw_image"
fi

# Read the project's rootfs partition (DOS/MBR partition 2) without attaching
# a loop device or mounting the image.
partition_info=$(python3 "$SCRIPT_DIR/wic-image-tools.py" rootfs-partition "$raw_image") || \
  die 'could not locate the rootfs partition in the WIC image'
read -r rootfs_start rootfs_sectors <<< "$partition_info"

dd if="$raw_image" of="$rootfs_image" bs=512 skip="$rootfs_start" count="$rootfs_sectors"

ssh_dir_stat=$("$DEBUGFS" -R 'stat /root/.ssh' "$rootfs_image" 2>&1 || true)
if grep -q 'Type: directory' <<< "$ssh_dir_stat"; then
  ssh_dir_exists=1
elif grep -q 'File not found by ext2_lookup' <<< "$ssh_dir_stat"; then
  ssh_dir_exists=0
else
  die 'could not inspect /root/.ssh in the root filesystem'
fi

authorized_stat=$("$DEBUGFS" -R 'stat /root/.ssh/authorized_keys' "$rootfs_image" 2>&1 || true)
if grep -q 'Type: regular' <<< "$authorized_stat"; then
  authorized_exists=1
  "$DEBUGFS" -R 'cat /root/.ssh/authorized_keys' "$rootfs_image" 2>/dev/null > "$existing_keys" || \
    die 'could not read existing authorized_keys'
elif grep -q 'File not found by ext2_lookup' <<< "$authorized_stat"; then
  authorized_exists=0
  : > "$existing_keys"
else
  die '/root/.ssh/authorized_keys exists but is not a regular file'
fi

# Preserve existing keys, append only new key lines, and ensure the file ends
# with a newline. Existing authorized_keys entries are not printed.
added_keys=$(python3 "$SCRIPT_DIR/wic-image-tools.py" merge-keys \
  "$existing_keys" "$key_file" "$merged_keys") || die 'could not merge the supplied public key(s)'

if ((ssh_dir_exists == 0)); then
  (cd "$workdir" && "$DEBUGFS" -w -R 'mkdir /root/.ssh' rootfs.ext4 >/dev/null 2>&1) || \
    die 'could not create /root/.ssh in the root filesystem'
fi
if ((authorized_exists)); then
  (cd "$workdir" && "$DEBUGFS" -w -R 'rm /root/.ssh/authorized_keys' rootfs.ext4 >/dev/null 2>&1) || \
    die 'could not replace existing authorized_keys'
fi
(cd "$workdir" && "$DEBUGFS" -w -R 'write authorized_keys /root/.ssh/authorized_keys' rootfs.ext4 >/dev/null 2>&1) || \
  die 'could not write authorized_keys into the root filesystem'

for command in \
  'set_inode_field /root/.ssh mode 040700' \
  'set_inode_field /root/.ssh uid 0' \
  'set_inode_field /root/.ssh gid 0' \
  'set_inode_field /root/.ssh/authorized_keys mode 0100600' \
  'set_inode_field /root/.ssh/authorized_keys uid 0' \
  'set_inode_field /root/.ssh/authorized_keys gid 0'; do
  (cd "$workdir" && "$DEBUGFS" -w -R "$command" rootfs.ext4 >/dev/null 2>&1) || \
    die "could not set secure ownership or permissions ($command)"
done

dir_check=$("$DEBUGFS" -R 'stat /root/.ssh' "$rootfs_image" 2>/dev/null)
file_check=$("$DEBUGFS" -R 'stat /root/.ssh/authorized_keys' "$rootfs_image" 2>/dev/null)
grep -q 'Type: directory' <<< "$dir_check" && grep -Eq 'Mode:[[:space:]]*0700' <<< "$dir_check" || \
  die '/root/.ssh does not have the expected directory permissions'
grep -q 'Type: regular' <<< "$file_check" && grep -Eq 'Mode:[[:space:]]*0600' <<< "$file_check" || \
  die 'authorized_keys does not have the expected file permissions'
grep -Eq 'User:[[:space:]]+0' <<< "$file_check" && grep -Eq 'Group:[[:space:]]+0' <<< "$file_check" || \
  die 'authorized_keys is not owned by root'
(cd "$workdir" && "$DEBUGFS" -R 'cat /root/.ssh/authorized_keys' rootfs.ext4 2>/dev/null) > "$verified_keys"
cmp -s "$merged_keys" "$verified_keys" || die 'authorized_keys verification failed'

if ! "$E2FSCK" -fn "$rootfs_image" > "$workdir/e2fsck.log" 2>&1; then
  cat "$workdir/e2fsck.log" >&2
  die 'offline ext4 consistency check failed; the original image is unchanged'
fi

dd if="$rootfs_image" of="$raw_image" bs=512 seek="$rootfs_start" conv=notrunc

output_tmp=$(mktemp "${output}.tmp.XXXXXX")
if ((output_compressed)); then
  bzip2 -c "$raw_image" > "$output_tmp"
else
  cp "$raw_image" "$output_tmp"
fi
chmod 0644 "$output_tmp"
if ((force)); then
  mv -f "$output_tmp" "$output"
else
  # Hard-link creation is atomic and refuses to overwrite a file that appeared
  # after the initial existence check.
  ln "$output_tmp" "$output" || die "output already exists: $output"
  rm -f "$output_tmp"
fi
output_tmp=''

printf 'Added %s new public key(s).\n' "$added_keys"
printf 'Image copy: %s\n' "$output"
printf 'The original image is unchanged. Its existing .bmap does not match this copy; use bmaptool copy --nobmap or generate a new bmap.\n'
