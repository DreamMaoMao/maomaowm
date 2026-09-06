project('mango', ['c'],
  version : '0.16.3',
)

subdir('protocols')

is_nixos = false
os_release = run_command('cat', '/etc/os-release', check: false)
if os_release.returncode() == 0
    if os_release.stdout().contains('ID=nixos')
        is_nixos = true
    endif
endif

prefix = get_option('prefix')
sysconfdir = get_option('sysconfdir')

# If sysconfdir starts with prefix, strip the prefix.
if sysconfdir.startswith(prefix) and not is_nixos
  sysconfdir = sysconfdir.substring(prefix.length())
  # Ensure sysconfdir is an absolute path.
  if not sysconfdir.startswith('/')
    sysconfdir = '/' + sysconfdir
  endif
endif

cc = meson.get_compiler('c')
libm = cc.find_library('m')
libdrm = dependency('libdrm')
xcb = dependency('xcb', required : get_option('xwayland'))
xlibs = dependency('xcb-icccm', required : get_option('xwayland'))
wayland_server_dep = dependency('wayland-server',version: '>=1.23.1')
wlroots_dep = dependency('wlroots-0.20',version: '>=0.20.0')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput',version: '>=1.27.1')
libwayland_client_dep = dependency('wayland-client')
pcre2_dep = dependency('libpcre2-8')
libscenefx_dep = dependency('scenefx-0.5',version: '>=0.5.0')
pixman_dep = dependency('pixman-1')
cjson_dep = dependency('libcjson')
pangocairo_dep = dependency('pangocairo')

# version info
git = find_program('git', required : false)
is_git_repo = false

# check if current directory is a git repo
if git.found()
  git_status = run_command(git, 'rev-parse', '--is-inside-work-tree', check : false)
  if git_status.returncode() == 0 and git_status.stdout().strip() == 'true'
    is_git_repo = true
  endif
endif

if is_git_repo
  # if current directory is a git repo, get commit hash and latest tag
  commit_hash = run_command(git, 'rev-parse', '--short', 'HEAD', check : false).stdout().strip()
  latest_tag = meson.project_version()
  version_with_hash = '@0@(@1@)'.format(latest_tag, commit_hash)
else
  # if not a git repo, use project version and "release" string
  commit_hash = 'release'
  latest_tag = meson.project_version()
  version_with_hash = '@0@(@1@)'.format(latest_tag, commit_hash)
endif

# define compilation args
c_args = [
  '-g',
  '-O2',
  '-U_FORTIFY_SOURCE',
  '-Wno-unused-function',
  '-Wno-unused-result',
  '-DWLR_USE_UNSTABLE',
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="@0@"'.format(version_with_hash),
  '-DSYSCONFDIR="@0@"'.format('/etc'),
]

# add debug args only when debug option is enabled
if get_option('asan')
  c_args += [
    '-fsanitize=address',
    '-fno-omit-frame-pointer',
    '-fno-optimize-sibling-calls'
  ]
endif

if xcb.found() and xlibs.found()
  c_args += '-DXWAYLAND'
endif

# define link args
link_args = []
if get_option('asan')
  link_args += '-fsanitize=address'
endif

# Auto-include every .c file under src/ (recursively) so new source files
# are picked up at `meson setup` time without editing this file.
# NOTE: mmsg/mmsg.c lives outside src/ on purpose (separate binary, own main()).
find = find_program('find')
mango_sources = files(run_command(find, 'src', '-name', '*.c', check: true).stdout().split())

mango_inc = include_directories('.', 'include')

executable('mango',
  mango_sources,

  wayland_sources,
  include_directories : mango_inc,
  dependencies : [
    libm,
    libdrm,
    xcb,
    xlibs,
    libscenefx_dep,
    wayland_server_dep,
    wlroots_dep,
    xkbcommon_dep,
    libinput_dep,
    libwayland_client_dep,
    pcre2_dep,
    pixman_dep,
    cjson_dep,
    pangocairo_dep,
  ],
  install : true,
  c_args : c_args,
  link_args : link_args,
)

# build mmsg

wayland_scanner_client_header = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.h',
  arguments: ['client-header', '@INPUT@', '@OUTPUT@']
)

wayland_scanner_private_code = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.c',
  arguments: ['private-code', '@INPUT@', '@OUTPUT@']
)

# use generator in mmsg target
executable('mmsg',
  'mmsg/mmsg.c',
  'src/common/log.c',
  'src/common/util.c',

  wayland_sources,

  include_directories : mango_inc,

  dependencies: [
    libwayland_client_dep,
    wlroots_dep,
    pcre2_dep,
  ],

  install: true,

  c_args: [
    '-g',
    '-O2',
    '-U_FORTIFY_SOURCE',
    '-Wno-unused-function',
    '-DWLR_USE_UNSTABLE',
  ],
)

mandir = get_option('mandir')
desktop_install_dir = join_paths(prefix, 'share/wayland-sessions')
portal_install_dir = join_paths(prefix, 'share/xdg-desktop-portal')
install_data('assets/mango.desktop', install_dir : desktop_install_dir)
install_data('assets/mango-portals.conf', install_dir : portal_install_dir)
install_data('assets/config.conf', install_dir : join_paths(sysconfdir, 'mango'))

# generate the mmsg man page at build time so the version always
# matches the project version
mmsg_man_conf = configuration_data()
mmsg_man_conf.set('VERSION', meson.project_version())
install_data(
  configure_file(
    input : 'mmsg/mmsg.1.in',
    output : 'mmsg.1',
    configuration : mmsg_man_conf,
  ),
  install_dir : join_paths(mandir, 'man1'),
)
