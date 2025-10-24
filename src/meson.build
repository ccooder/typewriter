typewriter_sources = [
    'main.c',
    'typewriter-application.c',
    'typewriter-window.c',
    'typewriter-ui.c',
    'typewriter-input.c',
    'typewriter-load-article.c',
    'x11-util.c',
    'qq-group-item.c',
]

typewriter_deps = [
    dependency('gtk4'),
    dependency('x11'),
    dependency('xtst'),
]

typewriter_sources += gnome.compile_resources('typewriter-resources',
                                              'typewriter.gresource.xml',
                                              c_name : 'typewriter'
)

fs = import('fs')
src_dir = meson.current_source_dir()
build_dir = meson.current_build_dir()

copy = fs.copyfile(
    join_paths(src_dir, 'list_qq_win.scpt'),
    install : true,
    install_dir : join_paths(meson.current_build_dir())
)

subdir('theme')

deps = declare_dependency(
    dependencies : typewriter_deps,
    sources : typewriter_sources,
)

executable(meson.project_name(),
           dependencies : deps,
           install : true,
)
