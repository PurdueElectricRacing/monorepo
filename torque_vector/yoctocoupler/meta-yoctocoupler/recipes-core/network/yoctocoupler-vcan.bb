SUMMARY = "Virtual CAN interface for Yoctocoupler development and QEMU"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://20-vcan0.netdev file://20-vcan0.network"
S = "${WORKDIR}"

RDEPENDS:${PN} = "yoctocoupler-networkd kernel-module-vcan"

do_install() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${WORKDIR}/20-vcan0.netdev ${WORKDIR}/20-vcan0.network \
        ${D}${sysconfdir}/systemd/network/
}

FILES:${PN} += "${sysconfdir}/systemd/network/20-vcan0.*"
