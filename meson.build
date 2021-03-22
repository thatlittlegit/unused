project('unused', 'c',
  version: '0.1',
  default_options: ['warning_level=3', 'c_std=c89'])

deps = [
  meson.get_compiler('c').find_library('bfd'),
]

src = files(
  'debug.c',
  'dict.c',
  'unused.c',
)

summary('backend', get_option('disassembler'))

if get_option('disassembler') == 'libopcodes'
  deps += meson.get_compiler('c').find_library('opcodes')
  src += files('backend/libopcodes.c')
endif

exe = executable('unused', src,
  dependencies: deps,
  install: true)
