project('unused', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

deps = [
  meson.get_compiler('c').find_library('bfd'),
  meson.get_compiler('c').find_library('opcodes'),
]

exe = executable('unused', 'unused.c',
  dependencies: deps,
  install : true)
