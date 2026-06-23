# Copyright 2026 - Open NVIDIA userspace driver project
# SPDX-License-Identifier: MIT

libdrm_nvidia = library(
  'drm_nvidia',
  [
    files(
      'nvidia_rm.c',
      'nvidia_device.c',
      'nvidia_bo.c',
    ),
    config_file,
  ],
  c_args : libdrm_c_args,
  include_directories : [inc_root, inc_drm],
  link_with : libdrm,
  dependencies : [dep_threads, dep_atomic_ops],
  version : '1.@0@.0'.format(patch_ver),
  install : true,
)

install_headers('nvidia.h', 'nvidia_rm.h', subdir : 'libdrm')

pkg.generate(
  libdrm_nvidia,
  name : 'libdrm_nvidia',
  subdirs : ['.', 'libdrm'],
  description : 'Userspace interface to NVIDIA RM kernel module and nvidia-drm',
)

ext_libdrm_nvidia = declare_dependency(
  link_with : [libdrm, libdrm_nvidia],
  include_directories : [inc_drm, include_directories('.')],
)

meson.override_dependency('libdrm_nvidia', ext_libdrm_nvidia)

test(
  'nvidia-symbols-check',
  symbols_check,
  args : [
    '--lib', libdrm_nvidia,
    '--symbols-file', files('nvidia-symbols.txt'),
    '--nm', prog_nm.full_path(),
  ],
)
