SUMMARY = "systemd-networkd configuration for Yoctocoupler"
DESCRIPTION = "Enables systemd-networkd with static wired networking and systemd-resolved DNS."

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit systemd

SRC_URI = " \
    file://10-yoctocoupler-wired.network \
    file://yoctocoupler-networkd.service \
"

S = "${WORKDIR}"

RDEPENDS:${PN} = "systemd"
SYSTEMD_SERVICE:${PN} = "yoctocoupler-networkd.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    install -D -m 0644 ${WORKDIR}/10-yoctocoupler-wired.network \
        ${D}${sysconfdir}/systemd/network/10-yoctocoupler-wired.network
    install -D -m 0644 ${WORKDIR}/yoctocoupler-networkd.service \
        ${D}${systemd_system_unitdir}/yoctocoupler-networkd.service
}

FILES:${PN} += " \
    ${sysconfdir}/systemd/network/10-yoctocoupler-wired.network \
    ${systemd_system_unitdir}/yoctocoupler-networkd.service \
"
