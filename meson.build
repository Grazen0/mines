project(
  'mines',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : [
    'warning_level=3',
    'c_std=c23',
    'b_sanitize=address,undefined',
    'b_lundef=false',
  ],
)

dependencies = [
]

inc_dirs = include_directories('src')

sources = [
  'src/ansi.c',
  'src/extra.c',
  'src/game.c',
  'src/input.c',
  'src/main.c',
  'src/tui.c',
]

exe = executable(
  'mines',
  sources,
  dependencies : dependencies,
  include_directories : inc_dirs,
  install : true,
  c_args: ['-D_POSIX_C_SOURCE']
)

