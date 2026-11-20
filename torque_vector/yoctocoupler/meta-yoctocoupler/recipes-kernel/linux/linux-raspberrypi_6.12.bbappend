FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# Shared kernel policy for both profiles.
# The production profile selects the hardware CAN fragment; dev/QEMU selects
# the IGB + VCAN fragment.
SRC_URI += "file://yoctocoupler-rt.cfg file://yoctocoupler-can.cfg"
SRC_URI:append = "${@bb.utils.contains('YOCTOCOUPLER_QEMU', '1', ' file://yoctocoupler-qemu-igb.cfg', ' file://yoctocoupler-hardware-can.cfg', d)}"

LINUX_VERSION_EXTENSION:append = "-yoctocoupler-rt"
