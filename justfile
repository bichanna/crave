BUILD_DIR := "build"

default: build

debug_config:
    meson configure --buildtype=debug {{BUILD_DIR}} -Dc_args='-DDEBUG'

debug: debug_config
    meson compile -C {{BUILD_DIR}}

config:
    meson setup {{BUILD_DIR}}

build: config
    meson compile -C {{BUILD_DIR}}

install: build
    meson install -C {{BUILD_DIR}}

release:
    meson setup --buildtype=release {{BUILD_DIR}}

format:
    clang-format -i **/*.{h,c} 

clean:
    rm -rf {{BUILD_DIR}}

todo:
    grep -r -n -E "TODO|FIXME" src/*.c
