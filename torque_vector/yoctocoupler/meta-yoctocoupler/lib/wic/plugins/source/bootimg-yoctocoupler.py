"""Raspberry Pi boot partition with a Wic-selected root device."""

import importlib.util
from pathlib import Path

import wic
from wic import WicError
from wic.misc import get_bitbake_var

# Upstream's hyphenated plugin filename cannot be imported as a Python module.
_spec = importlib.util.spec_from_file_location(
    "yoctocoupler_bootimg_partition",
    Path(wic.__file__).parent / "plugins/source/bootimg-partition.py")
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)
BootimgPartitionPlugin = _module.BootimgPartitionPlugin


class BootimgYoctocouplerPlugin(BootimgPartitionPlugin):
    name = "bootimg-yoctocoupler"

    @classmethod
    def do_configure_partition(cls, part, source_params, cr, cr_workdir,
                               oe_builddir, bootimg_dir, kernel_dir,
                               native_sysroot):
        super().do_configure_partition(
            part, source_params, cr, cr_workdir, oe_builddir, bootimg_dir,
            kernel_dir, native_sysroot)
        rootdev = cr.rootdev
        if not rootdev.startswith("PARTUUID="):
            raise WicError("Yoctocoupler root partition must use --use-uuid")

        kernel_dir = kernel_dir or get_bitbake_var("DEPLOY_DIR_IMAGE")
        for index, (source, destination) in enumerate(cls.install_task):
            if destination != "cmdline.txt":
                continue
            cmdline = Path(kernel_dir, source).read_text().split()
            if sum(arg.startswith("root=") for arg in cmdline) != 1:
                raise WicError("Expected exactly one root= in cmdline.txt")
            cmdline = ["root=" + rootdev if arg.startswith("root=") else arg
                       for arg in cmdline]
            output = Path(cr_workdir, "yoctocoupler-cmdline.txt")
            output.write_text(" ".join(cmdline) + "\n")
            cls.install_task[index] = (str(output), destination)
            return
        raise WicError("Missing cmdline.txt in boot partition files")
