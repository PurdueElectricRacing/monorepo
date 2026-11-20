SUMMARY = "Shared minimal real-time image for Yoctocoupler"
DESCRIPTION = "A minimal headless systemd image for Raspberry Pi CM5 with PREEMPT_RT, GPIO PPS, static networking, and OpenSSH."

LICENSE = "MIT"

require recipes-core/images/core-image-minimal.bb

WKS_FILE = "yoctocoupler.wks"

IMAGE_FEATURES += "ssh-server-openssh"
IMAGE_INSTALL:append = " kernel-module-pps-gpio kernel-module-can-raw yoctocoupler-networkd"
# Variant packages are selected by the independent production and QEMU KAS profiles.

# Inject a key only when explicitly supplied; never create a default credential.
YOCTOCOUPLER_SSH_PUBLIC_KEY ?= ""

ROOTFS_POSTPROCESS_COMMAND:append = " yoctocoupler_install_ssh_key;"
# Changes to the supplied key must regenerate the root filesystem.
do_rootfs[vardeps] += "YOCTOCOUPLER_SSH_PUBLIC_KEY"

yoctocoupler_install_ssh_key() {
    if [ -z "${YOCTOCOUPLER_SSH_PUBLIC_KEY}" ]; then
        return
    fi

    install -d -m 0700 "${IMAGE_ROOTFS}/root/.ssh"
    printf '%s\n' "${YOCTOCOUPLER_SSH_PUBLIC_KEY}" > "${IMAGE_ROOTFS}/root/.ssh/authorized_keys"
    chmod 0600 "${IMAGE_ROOTFS}/root/.ssh/authorized_keys"
}
