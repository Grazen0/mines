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

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

inc_dirs = include_directories('src')

core_sources = [
  'src/ansi.c',
  'src/extra.c',
  'src/game.c',
  'src/input.c',
  'src/solver.c',
  'src/tui.c',
]

core_lib = library(
  'mines_core',
  'src/csp.c',
  'src/game.c',
  'src/solver.c',
  include_directories : inc_dirs,
  install : false,

)

core_dep = declare_dependency(
  link_with : core_lib,
  include_directories : inc_dirs,
)

exe_deps = [
  core_dep,
]


exe = executable(
  'mines',
  'src/ansi.c',
  'src/extra.c',
  'src/input.c',
  'src/main.c',
  'src/tui.c',
  dependencies : exe_deps,
  include_directories : inc_dirs,
  install : true,
)

tests = [
  'csp_test',
]

unity_proj = subproject('unity')
unity_dep = unity_proj.get_variable('unity_dep')
runner_gen = unity_proj.get_variable('gen_test_runner')

test_deps = [
  unity_dep,
  core_dep,
]

foreach test_name : tests
  test_runner = meson.project_source_root() / 'test' / (test_name + '.c')
  test_exe = executable(
    test_name,
    test_runner,
    runner_gen.process(test_runner),
    dependencies : test_deps,
    include_directories : inc_dirs,
  )
  test(test_name + '_test', test_exe)
endforeach
