wayland_scanner = find_program('wayland-scanner')
wayland_protos_dep = dependency('wayland-protocols')
wl_protocol_dir = wayland_protos_dep.get_variable(pkgconfig:'pkgdatadir')
wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'])
wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.h',
	arguments: ['server-header', '@INPUT@', '@OUTPUT@'])

wayland_xmls = [
wl_protocol_dir + '/stable/xdg-shell/xdg-shell.xml',
wl_protocol_dir + '/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml',
wl_protocol_dir + '/unstable/xdg-output/xdg-output-unstable-v1.xml',
wl_protocol_dir + '/staging/cursor-shape/cursor-shape-v1.xml',
	wl_protocol_dir + '/stable/tablet/tablet-v2.xml',
	wl_protocol_dir + '/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml',
	wl_protocol_dir + '/staging/ext-image-capture-source/ext-image-capture-source-v1.xml',
	wl_protocol_dir + '/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml',
	wl_protocol_dir + '/staging/ext-workspace/ext-workspace-v1.xml',
	wl_protocol_dir + '/staging/tearing-control/tearing-control-v1.xml',
	'wlr-foreign-toplevel-management-unstable-v1.xml',
	'wlr-layer-shell-unstable-v1.xml',
	'wlr-output-power-management-unstable-v1.xml',
	'kde-appmenu.xml',
	'gtk-shell.xml',
]
wayland_sources = [
	wayland_scanner_code.process(wayland_xmls),
	wayland_scanner_client.process(wayland_xmls),
]
