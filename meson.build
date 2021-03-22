project('unused', 'c',
  version : '0.1',
  default_options: ['warning_level=3', 'c_std=c89'])

deps = [
  meson.get_compiler('c').find_library('bfd'),
  meson.get_compiler('c').find_library('opcodes'),
]

exe = executable('unused', ['backend/libopcodes.c', 'debug.c', 'dict.c', 'unused.c'],
  dependencies: deps,
  install : true)
