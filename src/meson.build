typewriter_sources = [
  'main.c',
  'typewriter-application.c',
  'typewriter-window.c',
]

typewriter_deps = [
  dependency('gtk4'),
]

typewriter_sources += gnome.compile_resources('typewriter-resources',
  'typewriter.gresource.xml',
  c_name: 'typewriter'
)



subdir('theme')

deps = declare_dependency(
  dependencies: typewriter_deps,
  sources: typewriter_sources,
)

executable(meson.project_name(),
  dependencies: deps,
       install: true,
)
