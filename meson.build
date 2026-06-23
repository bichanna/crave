project(
  'crave',
  'c',
  version: '0.1.0',
  license: 'Apache',
  default_options: ['warning_level=3', 'werror=true', 'c_std=c17', 'default_library=static'],
)

root = meson.project_source_root()
cc = meson.get_compiler('c')

if host_machine.system() == 'darwin'
  raylib = cc.find_library('raylib',
    dirs: root / 'thirdparty' / 'raylib' / 'mac',
    static: true
  )
  system_libs = [dependency('appleframeworks',
    modules: ['Cocoa', 'IOKit', 'CoreVideo', 'CoreGraphics', 'OpenGL']
  )]

elif host_machine.system() == 'linux'
  raylib = cc.find_library('raylib',
    dirs: root / 'thirdparty' / 'raylib' / 'linux_x64',
    static: true
  )
  system_libs = [dependency('gl'), dependency('x11'), dependency('threads'),
    cc.find_library('m'), cc.find_library('dl'), cc.find_library('rt')]

# TODO: Windows support later

else
  error('unsupported OS')
endif

thirdparty = static_library('thirdparty',
  'thirdparty/sqlite3/sqlite3.c',
  'thirdparty/tinyfiledialogs/tinyfiledialogs.c',
  c_args: ['-w']
)

includes = [
  include_directories(
    'src',
    'thirdparty/sqlite3',
    'thirdparty/tinyfiledialogs',
    'thirdparty/raylib'
  ),
  include_directories('thirdparty/clay', is_system: true),
]

src = [
  'src/main.c'
]

executable('crave',
  src,
  include_directories: includes,
  link_with: thirdparty,
  dependencies: [raylib] + system_libs,
  install: true
)
