project('unused', 'c',
  version: '0.1',
  default_options: ['warning_level=3', 'c_std=c89'])

deps = [
  meson.get_compiler('c').find_library('bfd'),
  meson.get_compiler('c').find_library('opcodes'),
]

src = files(
  'backend/libopcodes.c',
  'debug.c',
  'dict.c',
  'unused.c',
)

exe = executable('unused', src,
  dependencies: deps,
  install: true)
