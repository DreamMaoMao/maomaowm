wayland_scanner = find_program('wayland-scanner')
wayland_protos_dep = dependency('wayland-protocols')
wl_protocol_dir = wayland_protos_dep.get_pkgconfig_variable('pkgdatadir')
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
	wl_protocol_dir + '/staging/cursor-shape/cursor-shape-v1.xml',
	wl_protocol_dir + '/stable/tablet/tablet-v2.xml',
	'wlr-foreign-toplevel-management-unstable-v1.xml',
	'dwl-ipc-unstable-v2.xml',
	'wlr-layer-shell-unstable-v1.xml',
	'handwrite-unstable-v1.xml',
]
wayland_sources = [
	wayland_scanner_code.process(wayland_xmls),
	wayland_scanner_client.process(wayland_xmls),
]
