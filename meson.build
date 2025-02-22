project('maomao', ['c', 'cpp'],
  version : '0.1.1'
)

subdir('protocols')

fs = import('fs')

# 获取用户的主目录
home_dir = run_command('sh', '-c', 'echo $HOME', check: true).stdout().strip()
config_dir = join_paths(home_dir, '.config', 'maomao')

# 如果目标目录不存在，则创建它
if not fs.is_dir(config_dir)
    run_command('mkdir', '-p', config_dir, check: true)
endif

# 拷贝 config.conf
config_file = join_paths(config_dir, 'config.conf')
if not fs.exists(config_file)
    run_command('cp', 'config.conf', config_file, check: true)
endif

# 拷贝 autostart.sh
autostart_file = join_paths(config_dir, 'autostart.sh')
if not fs.exists(autostart_file)
    run_command('cp', 'autostart.sh', autostart_file, check: true)
endif

cc = meson.get_compiler('c')
libm = cc.find_library('m')
xcb = dependency('xcb', required : get_option('xwayland'))
xlibs = dependency('xcb-icccm', required : get_option('xwayland'))
wayland_server_dep = dependency('wayland-server')
wlroots_dep = dependency('wlroots-0.18')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput')
libwayland_client_dep = dependency('wayland-client')

c_args = [
  '-g',
  '-Wno-unused-function',
  '-DWLR_USE_UNSTABLE',
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="@0@"'.format(meson.project_version())
]

if xcb.found() and xlibs.found()
  c_args += '-DXWAYLAND'
  c_args += '-DIM'
  c_args += '-DHANDWRITE'
 endif

executable('maomao',
  'maomao.c',
  'util.c',
  wayland_sources,
  dependencies : [
    libm,
    xcb,
    xlibs,
    wayland_server_dep,
    wlroots_dep,
    xkbcommon_dep,
    libinput_dep,
    libwayland_client_dep,
  ],
  install : true,
  c_args : c_args
)

prefix = get_option('prefix')
desktop_install_dir = join_paths(prefix, 'share/wayland-sessions')
install_data('maomao.desktop', install_dir : desktop_install_dir)
