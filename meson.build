project('nfl_typewriter', 'c',
        version : '1.0.0',
        default_options : ['warning_level=3', 'c_std=gnu2x'])

dep = dependency('gtk4', required : true)

srcs = run_command('grabber.sh', 'application', check: true).stdout().strip().split('\n')
message('srcs: @0@'.format(srcs))


nfl_typewriter = executable('nfl_typewriter', 'main.c',
                            srcs,
                            include_directories: include_directories('application'),
                            dependencies : dep,
                            install : true)

test('test', nfl_typewriter)