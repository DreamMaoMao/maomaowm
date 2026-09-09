/*
 * See LICENSE file for copyright and license details.
 */
#include "mango/animation/common.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/config/parse_config.h"
#include "mango/dispatch/bind.h"
#include "mango/draw/text-node.h"
#include "mango/ext-protocol/ext-workspace.h"
#include "mango/ext-protocol/foreign-toplevel.h"
#include "mango/ext-protocol/hdr.h"
#include "mango/ext-protocol/tearing.h"
#include "mango/ext-protocol/text-input.h"
#include "mango/ext-protocol/xdg-activation.h"
#include "mango/ext-protocol/xdg-output.h"
#include "mango/input/keyboard.h"
#include "mango/input/pointer.h"
#include "mango/input/tablet.h"
#include "mango/input/trackpad.h"
#include "mango/ipc/ipc.h"
#include "mango/layout/arrange.h"
#include "mango/manage/misc.h"
#include "mango/manage/monitor.h"
#include "wlr/util/box.h"
#include <fcntl.h>
#include <getopt.h>
#include <libinput.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <pthread.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <scenefx/types/fx/blur_data.h>
#include <scenefx/types/fx/clipped_region.h>
#include <scenefx/types/wlr_scene.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_color_management_v1.h>
#include <wlr/types/wlr_color_representation_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_fixes.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wordexp.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <X11/Xlib.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif

void handle_quit_signal(int32_t signo);

void handle_signal(int32_t signo) {
	if (signo == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
	else if (signo == SIGINT || signo == SIGTERM)
		quit(NULL);
}

static void restore_child_signals(void) {
	sigset_t set;
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, NULL);

	struct sigaction sa_dfl = {.sa_flags = 0, .sa_handler = SIG_DFL};
	sigaction(SIGCHLD, &sa_dfl, NULL);
	sigaction(SIGPIPE, &sa_dfl, NULL);
}

void cleanup_listeners(void) {
	wl_list_remove(&server.ext_workspace_commit_listener.link); // 0.7
	wl_list_remove(&server.print_status_listener.link);
	wl_list_remove(&server.cursor_axis_listener.link);
	wl_list_remove(&server.cursor_button_listener.link);
	wl_list_remove(&server.cursor_frame_listener.link);
	wl_list_remove(&server.cursor_motion_listener.link);
	wl_list_remove(&server.cursor_motion_absolute_listener.link);
	wl_list_remove(&server.cursor_touch_down_listener.link);
	wl_list_remove(&server.cursor_touch_up_listener.link);
	wl_list_remove(&server.cursor_touch_cancel_listener.link);
	wl_list_remove(&server.cursor_touch_motion_listener.link);
	wl_list_remove(&server.cursor_touch_frame_listener.link);
	wl_list_remove(&server.tablet_tool_proximity_listener.link);
	wl_list_remove(&server.tablet_tool_axis_listener.link);
	wl_list_remove(&server.tablet_tool_button_listener.link);
	wl_list_remove(&server.tablet_tool_tip_listener.link);
	wl_list_remove(&server.gpu_reset_listener.link);
	wl_list_remove(&server.new_idle_inhibitor_listener.link);
	wl_list_remove(&server.layout_change_listener.link);
	wl_list_remove(&server.new_input_device_listener.link);
	wl_list_remove(&server.new_virtual_keyboard_listener.link);
	wl_list_remove(&server.new_virtual_pointer_listener.link);
	wl_list_remove(&server.new_pointer_constraint_listener.link);
	wl_list_remove(&server.new_output_listener.link);
	wl_list_remove(&server.new_xdg_toplevel_listener.link);
	wl_list_remove(&server.new_xdg_decoration_listener.link);
	wl_list_remove(&server.new_xdg_popup_listener.link);
	wl_list_remove(&server.new_layer_surface_listener.link);
	wl_list_remove(&server.output_manager_apply_listener.link);
	wl_list_remove(&server.output_manager_test_listener.link);
	wl_list_remove(&server.output_power_manager_set_mode_listener.link);
	wl_list_remove(&server.request_cursor_listener.link);
	wl_list_remove(&server.request_set_psel_listener.link);
	wl_list_remove(&server.request_set_sel_listener.link);
	wl_list_remove(&server.request_set_cursor_shape_listener.link);
	wl_list_remove(&server.request_start_drag_listener.link);
	wl_list_remove(&server.start_drag_listener.link);
	wl_list_remove(&server.new_session_lock_listener.link);
	wl_list_remove(&server.new_foreign_toplevel_capture_listener.link);
	wl_list_remove(&server.tearing_new_object_listener.link);
	wl_list_remove(
		&server.keyboard_shortcuts_inhibit_new_inhibitor_listener.link);
	wl_list_remove(
		&server.ext_image_copy_capture_manager_new_session_listener.link);
	if (server.drm_lease_manager) {
		wl_list_remove(&server.drm_lease_request_listener.link);
	}
#ifdef XWAYLAND
	wl_list_remove(&server.new_xwayland_surface_listener.link);
	wl_list_remove(&server.xwayland_ready_listener.link);
#endif
}

void cleanup(void) {
	server.allow_frame_scheduling = false;

	ipc_cleanup();
	cleanup_listeners();
#ifdef XWAYLAND
	wlr_xwayland_destroy(server.xwayland);
	server.xwayland = NULL;
#endif

	wl_display_destroy_clients(server.display);
	if (server.child_pid > 0) {
		kill(-server.child_pid, SIGTERM);
		waitpid(server.child_pid, NULL, 0);
	}
	wlr_xcursor_manager_destroy(server.cursor_manager);

	keyboard_group_destroy(&server.keyboard_group->destroy, NULL);

	mango_im_relay_finish(server.input_method_relay);

	/* If it's not destroyed manually it will cause a use-after-free of
	 * wlr_seat. Destroy it until it's fixed in the wlroots side */
	wlr_backend_destroy(server.backend);

	wl_display_destroy(server.display);
	/* Destroy after the wayland display (when the monitors are already
	   destroyed) to avoid destroying them with an invalid scene output. */
	wlr_scene_node_destroy(&server.scene->tree.node);

	mango_text_global_finish();
}

void handle_quit_signal(int32_t signo) { quit(NULL); }

/* Environment variables inherited by the startup command. */
static char *env_vars[] = {
	"DISPLAY",
	"WAYLAND_DISPLAY",
	"XDG_CURRENT_DESKTOP",
	"XDG_SESSION_TYPE",
	"XCURSOR_THEME",
	"XCURSOR_SIZE",
	"MANGO_INSTANCE_SIGNATURE",
	NULL,
};

void set_activation_env() {
	if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
		mango_error(true, WLR_INFO,
					"Not updating dbus execution environment: "
					"DBUS_SESSION_BUS_ADDRESS not set");
		return;
	}

	mango_error(true, WLR_INFO, "Updating dbus execution environment");

	char *env_keys = join_strings(env_vars, " ");

	// first command: dbus-update-activation-environment
	const char *arg1 = env_keys;
	char *cmd1 = string_printf("dbus-update-activation-environment %s", arg1);
	if (!cmd1) {
		mango_error(true, WLR_ERROR, "Failed to allocate command string");
		goto cleanup;
	}
	spawn(&(Arg){.v = cmd1});
	free(cmd1);

	// second command: systemctl --user
	const char *action = "import-environment";
	char *cmd2 = string_printf("systemctl --user %s %s", action, env_keys);
	if (!cmd2) {
		mango_error(true, WLR_ERROR, "Failed to allocate command string");
		goto cleanup;
	}
	spawn(&(Arg){.v = cmd2});
	free(cmd2);

cleanup:
	free(env_keys);
}

void // 17
run(char *startup_cmd, int readiness_fd) {
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server.display);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	set_env_display();

	/* Start the backend. This will enumerate outputs and inputs, become the
	 * DRM master, etc */
	if (!wlr_backend_start(server.backend))
		die("startup: backend_start");

	/* Now that the socket exists and the backend is started, run the
	 * startup command */

	if (startup_cmd) {
		int32_t piperw[2];
		if (pipe(piperw) < 0)
			die("startup: pipe:");
		if ((server.child_pid = fork()) < 0)
			die("startup: fork:");
		if (server.child_pid == 0) {
			setsid();
			dup2(piperw[0], STDIN_FILENO);
			close(piperw[0]);
			close(piperw[1]);
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
			die("startup: execl:");
		}
		dup2(piperw[1], STDOUT_FILENO);
		close(piperw[1]);
		close(piperw[0]);
	}

	/* Mark stdout as non-blocking to avoid people who does not close stdin
	 * nor consumes it in their startup script getting dwl frozen */
	if (fd_set_nonblock(STDOUT_FILENO) < 0)
		close(STDOUT_FILENO);

	printstatus(IPC_WATCH_ARRANGGE);

	/* At this point the outputs are initialized, choose initial
	 * selected_monitor based on cursor position, and set default cursor image
	 */
	server.selected_monitor =
		monitor_at_point(server.cursor->x, server.cursor->y);

	/* TODO hack to get cursor to display in its initial location (100, 100)
	 * instead of (0, 0) and then jumping. still may not be fully
	 * initialized, as the image/coordinates are not transformed for the
	 * monitor when displayed here */
	wlr_cursor_warp_closest(server.cursor, NULL, server.cursor->x,
							server.cursor->y);
	wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "left_ptr");
	pointer_cursor_activity();

	set_activation_env();

	run_exec();
	run_exec_once();

	/*
	 * If running inside supervision suite like s6, notify about successfull
	 * startup by writing \n to the provided file descriptor and closing it
	 */
	if (readiness_fd > 2) {
		write(readiness_fd, "\n", 1);
		close(readiness_fd);
	}

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */

	wl_display_run(server.display);
}

void setup(void) {
	setenv("XDG_CURRENT_DESKTOP", "mango", 1);
	setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 1);

	parse_config();
	if (server.cli_debug_log) {
		config.log_level = WLR_DEBUG;
	}
	init_baked_points();

	set_env_without_display();

	int32_t drm_fd, i;
	int32_t sig[] = {SIGCHLD, SIGINT,
					 SIGTERM}; // SIGPIPE is ignored because IPC send failures
							   // should not affect the main program.
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handle_signal};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	// Ignore SIGPIPE separately.
	struct sigaction sa_pipe = {.sa_flags = 0, .sa_handler = SIG_IGN};
	sigemptyset(&sa_pipe.sa_mask);
	sigaction(SIGPIPE, &sa_pipe, NULL);

	pthread_atfork(NULL, NULL, restore_child_signals);

	wlr_log_init(config.log_level, NULL);

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	server.display = wl_display_create();

	wl_display_set_default_max_buffer_size(server.display, 1024 * 1024);

	server.event_loop = wl_display_get_event_loop(server.display);

	ipc_init(server.event_loop);

	server.tablet_manager = wlr_tablet_v2_create(server.display);
	/* The backend is a wlroots feature which abstracts the underlying input
	 * and output hardware. The autocreate option will choose the most
	 * suitable backend based on the current environment, such as opening an
	 * X11 window if an X11 server is running. The NULL argument here
	 * optionally allows you to pass in a custom renderer if wlr_renderer
	 * doesn't meet your needs. The backend uses the renderer, for example,
	 * to fall back to software cursors if the backend does not support
	 * hardware cursors (some older GPUs don't). */
	if (!(server.backend =
			  wlr_backend_autocreate(server.event_loop, &server.session)))
		die("couldn't create backend");

	server.headless_backend = wlr_headless_backend_create(server.event_loop);
	if (!server.headless_backend) {
		mango_error(true, WLR_ERROR,
					"Failed to create secondary headless backend");
	} else {
		wlr_multi_backend_add(server.backend, server.headless_backend);
	}

	/* Initialize the scene graph used to lay out windows */
	server.scene = wlr_scene_create();
	server.root_bg =
		wlr_scene_rect_create(&server.scene->tree, 0, 0, config.rootcolor);
	for (i = 0; i < NUM_LAYERS; i++)
		server.layers[i] = wlr_scene_tree_create(&server.scene->tree);
	server.drag_icon = wlr_scene_tree_create(&server.scene->tree);
	wlr_scene_node_place_below(&server.drag_icon->node,
							   &server.layers[LyrBlock]->node);

	/* Create a renderer with the default implementation */
	if (!(server.renderer = fx_renderer_create(server.backend)))
		die("couldn't create renderer");

	if (server.renderer->features.input_color_transform) {
		const enum wp_color_manager_v1_render_intent render_intents[] = {
			WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL,
		};
		size_t transfer_functions_len = 0;
		enum wp_color_manager_v1_transfer_function *transfer_functions =
			wlr_color_manager_v1_transfer_function_list_from_renderer(
				server.renderer, &transfer_functions_len);

		size_t primaries_len = 0;
		enum wp_color_manager_v1_primaries *primaries =
			wlr_color_manager_v1_primaries_list_from_renderer(server.renderer,
															  &primaries_len);

		struct wlr_color_manager_v1 *cm = wlr_color_manager_v1_create(
			server.display, 2,
			&(struct wlr_color_manager_v1_options){
				.features =
					{
						.parametric = true,
						.set_mastering_display_primaries = true,
					},
				.render_intents = render_intents,
				.render_intents_len = ARRAY_SIZE(render_intents),
				.transfer_functions = transfer_functions,
				.transfer_functions_len = transfer_functions_len,
				.primaries = primaries,
				.primaries_len = primaries_len,
			});

		free(transfer_functions);
		free(primaries);

		if (cm) {
			wlr_scene_set_color_manager_v1(server.scene, cm);
		} else {
			mango_error(true, WLR_ERROR, "unable to create color manager");
		}
	}

	wlr_color_representation_manager_v1_create_with_renderer(server.display, 1,
															 server.renderer);

	wl_signal_add(&server.renderer->events.lost, &server.gpu_reset_listener);

	/* Create shm, drm and linux_dmabuf interfaces by ourselves.
	 * The simplest way is call:
	 *      wlr_renderer_init_wl_display(renderer);
	 * but we need to create manually the linux_dmabuf interface to
	 * integrate it with wlr_scene. */
	wlr_renderer_init_wl_shm(server.renderer, server.display);

	if (wlr_renderer_get_texture_formats(server.renderer,
										 WLR_BUFFER_CAP_DMABUF)) {
		wlr_drm_create(server.display, server.renderer);
		wlr_scene_set_linux_dmabuf_v1(server.scene,
									  wlr_linux_dmabuf_v1_create_with_renderer(
										  server.display, 4, server.renderer));
	}

	if (config.syncobj_enable &&
		(drm_fd = wlr_renderer_get_drm_fd(server.renderer)) >= 0 &&
		server.renderer->features.timeline && server.backend->features.timeline)
		wlr_linux_drm_syncobj_manager_v1_create(server.display, 1, drm_fd);

	/* Create a default allocator */
	if (!(server.allocator =
			  wlr_allocator_autocreate(server.backend, server.renderer)))
		die("couldn't create allocator");

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device
	 * manager handles the clipboard. Each of these wlroots interfaces has
	 * room for you to dig your fingers in and play with their behavior if
	 * you want. Note that the clients cannot set the selection directly
	 * without compositor approval, see the setsel() function. */
	server.compositor =
		wlr_compositor_create(server.display, 6, server.renderer);
	wlr_export_dmabuf_manager_v1_create(server.display);
	wlr_screencopy_manager_v1_create(server.display);
	wlr_ext_output_image_capture_source_manager_v1_create(server.display, 1);
	wlr_data_control_manager_v1_create(server.display);
	wlr_data_device_manager_create(server.display);
	wlr_primary_selection_v1_device_manager_create(server.display);
	wlr_viewporter_create(server.display);
	wlr_single_pixel_buffer_manager_v1_create(server.display);
	wlr_fractional_scale_manager_v1_create(server.display, 1);
	wlr_presentation_create(server.display, server.backend, 2);
	wlr_subcompositor_create(server.display);
	wlr_alpha_modifier_v1_create(server.display);
	wlr_ext_data_control_manager_v1_create(server.display, 1);
	wlr_fixes_create(server.display, 1);

	wl_signal_init(&server.print_status_signal);
	wl_signal_add(&server.print_status_signal, &server.print_status_listener);

	server.ext_image_copy_capture_manager =
		wlr_ext_image_copy_capture_manager_v1_create(server.display, 1);
	wl_signal_add(&server.ext_image_copy_capture_manager->events.new_session,
				  &server.ext_image_copy_capture_manager_new_session_listener);

	xdg_activation_init();

	wlr_scene_set_gamma_control_manager_v1(
		server.scene, wlr_gamma_control_manager_v1_create(server.display));

	server.power_manager = wlr_output_power_manager_v1_create(server.display);
	wl_signal_add(&server.power_manager->events.set_mode,
				  &server.output_power_manager_set_mode_listener);

	server.foreign_toplevel_list =
		wlr_ext_foreign_toplevel_list_v1_create(server.display, 1);
	server.ext_foreign_toplevel_image_capture_source_manager =
		wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(
			server.display, 1);
	server.new_foreign_toplevel_capture_listener.notify =
		handle_new_foreign_toplevel_capture_request;
	wl_signal_add(&server.ext_foreign_toplevel_image_capture_source_manager
					   ->events.new_request,
				  &server.new_foreign_toplevel_capture_listener);

	server.tearing_control =
		wlr_tearing_control_manager_v1_create(server.display, 1);
	server.tearing_new_object_listener.notify = handle_tearing_new_object;
	wl_signal_add(&server.tearing_control->events.new_object,
				  &server.tearing_new_object_listener);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	server.output_layout = wlr_output_layout_create(server.display);
	wl_signal_add(&server.output_layout->events.change,
				  &server.layout_change_listener);
	/* Custom xdg-output: handled separately for XWayland. */
	xdg_output_init();

	/* Configure a listener to be notified when new outputs are available on
	 * the backend. */
	wl_list_init(&server.monitors);
	wl_signal_add(&server.backend->events.new_output,
				  &server.new_output_listener);
	server.scene_layout =
		wlr_scene_attach_output_layout(server.scene, server.output_layout);

	/* Set up our client lists and the xdg-shell. The xdg-shell is a
	 * Wayland protocol which is used for application windows. For more
	 * detail on shells, refer to the article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&server.clients);
	wl_list_init(&server.focus_stack);
	wl_list_init(&server.fadeout_clients);
	wl_list_init(&server.fadeout_layers);

	server.idle_notifier = wlr_idle_notifier_v1_create(server.display);

	server.idle_inhibit_manager = wlr_idle_inhibit_v1_create(server.display);
	wl_signal_add(&server.idle_inhibit_manager->events.new_inhibitor,
				  &server.new_idle_inhibitor_listener);

	server.keep_idle_inhibit_source = wl_event_loop_add_timer(
		wl_display_get_event_loop(server.display), idle_keep_inhibit, NULL);

	server.layer_shell = wlr_layer_shell_v1_create(server.display, 4);
	wl_signal_add(&server.layer_shell->events.new_surface,
				  &server.new_layer_surface_listener);

	server.xdg_shell = wlr_xdg_shell_create(server.display, 6);
	wl_signal_add(&server.xdg_shell->events.new_toplevel,
				  &server.new_xdg_toplevel_listener);
	wl_signal_add(&server.xdg_shell->events.new_popup,
				  &server.new_xdg_popup_listener);

	server.session_lock_manager =
		wlr_session_lock_manager_v1_create(server.display);
	wl_signal_add(&server.session_lock_manager->events.new_lock,
				  &server.new_session_lock_listener);

	server.locked_bg = wlr_scene_rect_create(
		server.layers[LyrBlock], server.scene_geometry.width,
		server.scene_geometry.height, (float[4]){0.1, 0.1, 0.1, 1.0});
	wlr_scene_node_set_enabled(&server.locked_bg->node, false);

	/* Use decoration protocols to negotiate server-side decorations */
	wlr_server_decoration_manager_set_default_mode(
		wlr_server_decoration_manager_create(server.display),
		WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	server.decoration_manager =
		wlr_xdg_decoration_manager_v1_create(server.display);
	wl_signal_add(&server.decoration_manager->events.new_toplevel_decoration,
				  &server.new_xdg_decoration_listener);

	server.pointer_constraints =
		wlr_pointer_constraints_v1_create(server.display);
	wl_signal_add(&server.pointer_constraints->events.new_constraint,
				  &server.new_pointer_constraint_listener);

	server.relative_pointer_manager =
		wlr_relative_pointer_manager_v1_create(server.display);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that
	 * cursor images are available at all scale factors on the screen
	 * (necessary for HiDPI support). Scaled cursors will be loaded with
	 * each output. */

	set_xcursor_env();

	server.cursor_manager =
		wlr_xcursor_manager_create(config.cursor_theme, config.cursor_size);
	/*
	 * wlr_cursor *only* displays an image on screen. It does not move
	 * around when the pointer moves. However, we can attach input devices
	 * to it, and it will generate aggregate events for all of them. In
	 * these events, we can choose how we want to process them, forwarding
	 * them to clients and moving the cursor around. More detail on this
	 * process is described in my input handling blog post:
	 *
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions
	 * above.
	 */
	wl_signal_add(&server.cursor->events.motion,
				  &server.cursor_motion_listener);
	wl_signal_add(&server.cursor->events.motion_absolute,
				  &server.cursor_motion_absolute_listener);
	wl_signal_add(&server.cursor->events.button,
				  &server.cursor_button_listener);
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis_listener);
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame_listener);
	wl_signal_add(&server.cursor->events.tablet_tool_proximity,
				  &server.tablet_tool_proximity_listener);
	wl_signal_add(&server.cursor->events.tablet_tool_axis,
				  &server.tablet_tool_axis_listener);
	wl_signal_add(&server.cursor->events.tablet_tool_button,
				  &server.tablet_tool_button_listener);
	wl_signal_add(&server.cursor->events.tablet_tool_tip,
				  &server.tablet_tool_tip_listener);

	// These two lines make the mouse cursor disappear in OBS windows;
	// not sure what this comment is about.
	server.cursor_shape_manager =
		wlr_cursor_shape_manager_v1_create(server.display, 1);
	wl_signal_add(&server.cursor_shape_manager->events.request_set_shape,
				  &server.request_set_cursor_shape_listener);
	server.hide_cursor_source =
		wl_event_loop_add_timer(wl_display_get_event_loop(server.display),
								pointer_hide_cursor, server.cursor);
	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener
	 * to let us know when new input devices are available on the backend.
	 */
	wl_list_init(&server.input_devices);
	wl_list_init(&server.standalone_keyboards);
	wl_list_init(&server.virtual_keyboards);
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);
	wl_list_init(&server.keyboard_shortcut_inhibitors);
	wl_signal_add(&server.backend->events.new_input,
				  &server.new_input_device_listener);
	server.virtual_keyboard_manager =
		wlr_virtual_keyboard_manager_v1_create(server.display);
	wl_signal_add(&server.virtual_keyboard_manager->events.new_virtual_keyboard,
				  &server.new_virtual_keyboard_listener);
	server.virtual_pointer_manager =
		wlr_virtual_pointer_manager_v1_create(server.display);
	wl_signal_add(&server.virtual_pointer_manager->events.new_virtual_pointer,
				  &server.new_virtual_pointer_listener);

	server.pointer_gestures = wlr_pointer_gestures_v1_create(server.display);
	LISTEN_STATIC(&server.cursor->events.swipe_begin,
				  handle_cursor_swipe_begin);
	LISTEN_STATIC(&server.cursor->events.swipe_update,
				  handle_cursor_swipe_update);
	LISTEN_STATIC(&server.cursor->events.swipe_end, handle_cursor_swipe_end);
	LISTEN_STATIC(&server.cursor->events.pinch_begin,
				  handle_cursor_pinch_begin);
	LISTEN_STATIC(&server.cursor->events.pinch_update,
				  handle_cursor_pinch_update);
	LISTEN_STATIC(&server.cursor->events.pinch_end, handle_cursor_pinch_end);
	LISTEN_STATIC(&server.cursor->events.hold_begin, handle_cursor_hold_begin);
	LISTEN_STATIC(&server.cursor->events.hold_end, handle_cursor_hold_end);

	/* Touch support: initialize the touch point list and connect cursor touch
	 * events. */
	wl_list_init(&server.touch_points);
	wl_signal_add(&server.cursor->events.touch_down,
				  &server.cursor_touch_down_listener);
	wl_signal_add(&server.cursor->events.touch_up,
				  &server.cursor_touch_up_listener);
	wl_signal_add(&server.cursor->events.touch_cancel,
				  &server.cursor_touch_cancel_listener);
	wl_signal_add(&server.cursor->events.touch_motion,
				  &server.cursor_touch_motion_listener);
	wl_signal_add(&server.cursor->events.touch_frame,
				  &server.cursor_touch_frame_listener);

	server.seat = wlr_seat_create(server.display, "seat0");

	wl_list_init(&server.last_cursor_surface_destroy_listener.link);
	wl_signal_add(&server.seat->events.request_set_cursor,
				  &server.request_cursor_listener);
	wl_signal_add(&server.seat->events.request_set_selection,
				  &server.request_set_sel_listener);
	wl_signal_add(&server.seat->events.request_set_primary_selection,
				  &server.request_set_psel_listener);
	wl_signal_add(&server.seat->events.request_start_drag,
				  &server.request_start_drag_listener);
	wl_signal_add(&server.seat->events.start_drag, &server.start_drag_listener);

	server.keyboard_group = keyboard_group_create();
	server.last_active_keyboard = server.keyboard_group->keyboard;
	wl_list_init(&server.keyboard_group->destroy.link);

	server.keyboard_shortcuts_inhibit =
		wlr_keyboard_shortcuts_inhibit_v1_create(server.display);
	wl_signal_add(&server.keyboard_shortcuts_inhibit->events.new_inhibitor,
				  &server.keyboard_shortcuts_inhibit_new_inhibitor_listener);

	server.output_manager = wlr_output_manager_v1_create(server.display);
	wl_signal_add(&server.output_manager->events.apply,
				  &server.output_manager_apply_listener);
	wl_signal_add(&server.output_manager->events.test,
				  &server.output_manager_test_listener);

	wlr_scene_set_blur_data(
		server.scene, config.blur_params.num_passes, config.blur_params.radius,
		config.blur_params.noise, config.blur_params.brightness,
		config.blur_params.contrast, config.blur_params.saturation);

	/* create text_input-, and input_method-protocol relevant globals */
	server.input_method_manager =
		wlr_input_method_manager_v2_create(server.display);
	server.text_input_manager =
		wlr_text_input_manager_v3_create(server.display);

	server.input_method_relay = mango_im_relay_create();

	server.drm_lease_manager =
		wlr_drm_lease_v1_manager_create(server.display, server.backend);
	if (server.drm_lease_manager) {
		wl_signal_add(&server.drm_lease_manager->events.request,
					  &server.drm_lease_request_listener);
	} else {
		mango_error(true, WLR_DEBUG,
					"Failed to create wlr_drm_lease_device_v1.");
		mango_error(true, WLR_INFO, "VR will not be available.");
	}

	// Create the toplevel manager handle.
	server.foreign_toplevel_manager =
		wlr_foreign_toplevel_manager_v1_create(server.display);
	struct wlr_xdg_foreign_registry *foreign_registry =
		wlr_xdg_foreign_registry_create(server.display);
	wlr_xdg_foreign_v1_create(server.display, foreign_registry);
	wlr_xdg_foreign_v2_create(server.display, foreign_registry);

	// ext-workspace protocol.
	workspaces_init();
#ifdef XWAYLAND
	/*
	 * Initialise the XWayland X server.
	 * It will be started when the first X client is started.
	 */
	server.xwayland = wlr_xwayland_create(server.display, server.compositor,
										  !config.xwayland_persistence);
	if (server.xwayland) {
		wl_signal_add(&server.xwayland->events.ready,
					  &server.xwayland_ready_listener);
		wl_signal_add(&server.xwayland->events.new_surface,
					  &server.new_xwayland_surface_listener);

		setenv("DISPLAY", server.xwayland->display_name, 1);
	} else {
		mango_error(
			true, WLR_ERROR,
			"failed to setup XWayland X server, continuing without it\n");
	}
	server.sync_keymap = wl_event_loop_add_timer(
		wl_display_get_event_loop(server.display), keyboard_sync_keymap, NULL);
#endif
}

int32_t main(int32_t argc, char *argv[]) {
	char *startup_cmd = NULL;
	int32_t c;
	int readiness_fd = 0;

	while ((c = getopt(argc, argv, "s:c:r:hdvp")) != -1) {
		if (c == 's') {
			startup_cmd = optarg;
		} else if (c == 'd') {
			server.cli_debug_log = true;
		} else if (c == 'v') {
			printf("mango " VERSION "\n");
			return EXIT_SUCCESS;
		} else if (c == 'c') {
			snprintf(server.cli_config_path, sizeof(server.cli_config_path),
					 "%s", optarg);
		} else if (c == 'p') {
			return parse_config() ? EXIT_SUCCESS : EXIT_FAILURE;
		} else if (c == 'r') {
			readiness_fd = atoi(optarg);
			if (readiness_fd < 3) {
				goto usage;
			}
		} else {
			goto usage;
		}
	}
	if (optind < argc)
		goto usage;

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications
	 * socket
	 */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	run(startup_cmd, readiness_fd);
	cleanup();
	return EXIT_SUCCESS;
usage:
	printf("Usage: mango [OPTIONS]\n"
		   "\n"
		   "Options:\n"
		   "  -v             Show mango version\n"
		   "  -d             Enable debug log\n"
		   "  -c <file>      Use custom configuration file\n"
		   "  -s <command>   Execute startup command\n"
		   "  -r <fdnum>     When WM is ready, write '\\n' to the given file "
		   "descriptor and close it. fdnum >= 3\n"
		   "  -p             Check configuration file error\n");
	return EXIT_SUCCESS;
}
