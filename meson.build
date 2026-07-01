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

elif host_machine.system() == 'windows'
  raylib = cc.find_library('raylib',
    dirs: root / 'thirdparty' / 'raylib' / 'windows_x64',
    static: true
  )
  # libs pulled in by static raylib (GLFW backend) and tinyfiledialogs
  system_libs = [
    cc.find_library('opengl32'), cc.find_library('gdi32'),
    cc.find_library('winmm'), cc.find_library('user32'),
    cc.find_library('shell32'), cc.find_library('kernel32'),
    cc.find_library('comdlg32'), cc.find_library('ole32'),
  ]

else
  error('unsupported OS')
endif

includes = [
  include_directories(
    'src',
    'thirdparty/sqlite3',
    'thirdparty/tinyfiledialogs',
    'thirdparty/raylib',
    'thirdparty/stb',
  ),
  include_directories('thirdparty/clay', is_system: true),
]

# portable for GCC/Clang (-w) and MSVC (/w)
nowarn = cc.get_argument_syntax() == 'msvc' ? ['/w'] : ['-w']

thirdparty = static_library('thirdparty',
  'thirdparty/sqlite3/sqlite3.c',
  'thirdparty/tinyfiledialogs/tinyfiledialogs.c',
  'src/clay_raylib.c',
  'src/image.c',
  include_directories: includes,
  c_args: nowarn
)

font_files = files([
  'fonts/GeistMono-Bold.ttf',
  'fonts/GeistMono-Italic.ttf',
  'fonts/GeistMono-Regular.ttf',
])

fonts_h = custom_target('fonts.h',
  output: 'fonts.h',
  input: font_files,
  command: [find_program('python3'), root / 'scripts' / 'embed_fonts.py', '@OUTPUT@', '@INPUT@']
)

src = [
  'src/main.c',
  'src/db.c',
  'src/app.c',
  'src/ui.c',
  fonts_h
]

executable('crave',
  src,
  include_directories: includes,
  link_with: thirdparty,
  dependencies: [raylib] + system_libs,
  win_subsystem: 'windows',  # GUI app: no console window on Windows
  install: true
)
