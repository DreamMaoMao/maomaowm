#include "mango/ext-protocol/hdr.h"

static uint32_t output_formats_8bit[10];
static uint32_t output_formats_10bit[8];
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/manage/monitor.h"

bool output_set_render_format(Monitor *m, uint32_t candidates[], size_t count,
							  struct wlr_output_state *state) {
	for (size_t i = 0; i < count; i++) {
		struct wlr_output_state test_state = *state;
		wlr_output_state_set_render_format(&test_state, candidates[i]);
		if (wlr_output_test_state(m->wlr_output, &test_state)) {
			wlr_output_state_set_render_format(state, candidates[i]);
			return true;
		}
	}

	mango_error(true, WLR_DEBUG, "HDR: Failed to set render format");
	return false;
}

bool output_format_in_candidates(uint32_t format, uint32_t candidates[],
								 size_t count) {
	for (size_t i = 0; i < count; i++)
		if (candidates[i] == format)
			return true;
	return false;
}
enum render_bit_depth bit_depth_from_format(uint32_t render_format) {
	if (output_format_in_candidates(render_format, output_formats_10bit,
									ARRAY_SIZE(output_formats_10bit)))
		return MANGO_RENDER_BIT_DEPTH_10;
	if (output_format_in_candidates(render_format, output_formats_8bit,
									ARRAY_SIZE(output_formats_8bit)))
		return MANGO_RENDER_BIT_DEPTH_8;
	return MANGO_RENDER_BIT_DEPTH_DEFAULT;
}
bool output_supports_hdr(const Monitor *m, const char **reason) {
	const struct wlr_output *output = m->wlr_output;
	const char *r = NULL;

	// hdr_force skips the two EDID-derived checks, for panels that declare HDR
	// only inside a DisplayID 2.0 extension. It does not skip the third:
	// output_color_transform is a renderer capability, not an EDID claim.
	if (!m->hdr_force &&
		!(output->supported_primaries & WLR_COLOR_NAMED_PRIMARIES_BT2020))
		r = "BT2020 primaries not supported";
	else if (!m->hdr_force && !(output->supported_transfer_functions &
								WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ))
		r = "PQ transfer function not supported";
	else if (!server.renderer->features.output_color_transform)
		r = "renderer doesn't support output color transforms";
	if (reason)
		*reason = r;
	return !r;
}
void output_enable_hdr(Monitor *m, struct wlr_output_state *os, bool enabled,
					   bool silent) {
	if (!m->is_hdr_enabling && !enabled)
		return;

	if (!output_supports_hdr(m, NULL)) {
		m->is_hdr_enabling = false;
		return;
	}

	if (!enabled) {
		if (m->wlr_output->supported_primaries ||
			m->wlr_output->supported_transfer_functions) {
			if (!silent)
				mango_error(true, WLR_DEBUG, "Disabling HDR on output %s",
							m->wlr_output->name);
			wlr_output_state_set_image_description(os, NULL);
		}
		m->is_hdr_enabling = false;
		return;
	}

	if (!silent)
		mango_error(true, WLR_DEBUG, "Enabling HDR on output %s",
					m->wlr_output->name);
	struct wlr_output_image_description desc = {
		.primaries = WLR_COLOR_NAMED_PRIMARIES_BT2020,
		.transfer_function = WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ,
	};

	// Mastering display metadata, from the monitorrule: wlroots does not expose
	// the EDID luminances. Values are in cd/m²; 0 means unset and leaves the
	// field zero, which is the previous behaviour.
	if (m->hdr_max_lum > 0) {
		wlr_color_primaries_from_named(&desc.mastering_display_primaries,
									   WLR_COLOR_NAMED_PRIMARIES_BT2020);
		desc.mastering_luminance.min = m->hdr_min_lum;
		desc.mastering_luminance.max = m->hdr_max_lum;
		desc.max_cll = m->hdr_max_lum;
		desc.max_fall = m->hdr_max_avg_lum;
	}

	m->is_hdr_enabling = true;
	wlr_output_state_set_image_description(os, &desc);
}
void output_state_setup_hdr(Monitor *m, bool silent,
							struct wlr_output_state *state) {
	uint32_t render_format = m->wlr_output->render_format;
	const char *unsupported_reason = NULL;
	bool hdr_supported = output_supports_hdr(m, &unsupported_reason);
	bool hdr_succeeded = false;

	if (!hdr_supported) {
		if (!silent)
			mango_error(true, WLR_INFO, "HDR not supported on output %s: %s",
						m->wlr_output->name, unsupported_reason);
		return;
	}

	enum render_bit_depth depth = config.hdr_depth;
	if (depth == MANGO_RENDER_BIT_DEPTH_DEFAULT)
		depth = bit_depth_from_format(render_format);

	if (depth == MANGO_RENDER_BIT_DEPTH_10 &&
		bit_depth_from_format(render_format) == depth) {
		hdr_succeeded = true; // 10-bit was already set successfully last time;
							  // reuse it directly.
	} else if (depth == MANGO_RENDER_BIT_DEPTH_10) {
		hdr_succeeded = output_set_render_format(
			m, output_formats_10bit, ARRAY_SIZE(output_formats_10bit), state);
		if (!hdr_succeeded) {
			if (!silent)
				mango_error(true, WLR_INFO,
							"No 10 bit color formats supported, HDR disabled.");
			hdr_succeeded = output_set_render_format(
				m, output_formats_8bit, ARRAY_SIZE(output_formats_8bit), state);
			if (!hdr_succeeded) {
				if (!silent)
					mango_error(true, WLR_ERROR,
								"No 8 bit color formats supported!");
			}
		}
	} else {
		// Explicitly requires 8-bit or falls back automatically.
		hdr_succeeded = output_set_render_format(
			m, output_formats_8bit, ARRAY_SIZE(output_formats_8bit), state);
		if (!hdr_succeeded) {
			if (!silent)
				mango_error(true, WLR_ERROR,
							"No 8 bit color formats supported!");
		}
	}

	output_enable_hdr(m, state, hdr_succeeded, silent);
}
/* togglehdr[,on|off|toggle][,<monitor name>|all] -- apply to one output */
bool togglehdr_output(Monitor *target, bool want) {
	if (!target || !target->wlr_output->enabled || !target->scene_output)
		return false;

	if (want == target->hdr_enable)
		return false;

	// Check before mutating, so a refusal leaves the state untouched. This is
	// also what makes the "all" form safe when some outputs are SDR.
	const char *reason = NULL;
	if (want && !output_supports_hdr(target, &reason)) {
		wlr_log(WLR_INFO, "togglehdr: HDR unavailable on %s: %s",
				target->wlr_output->name, reason);
		return false;
	}

	// Snapshot both flags: output_enable_hdr() writes is_hdr_enabling before
	// the commit is attempted, so a failed commit must restore both or the two
	// disagree -- mango would render SDR on a connector still in PQ.
	bool prev_enable = target->hdr_enable;
	bool prev_enabling = target->is_hdr_enabling;

	target->hdr_enable = want;

	if (want)
		output_state_setup_hdr(target, false, &target->pending);
	else
		output_enable_hdr(target, &target->pending, false, false);

	// wlroots damages the whole output for a geometry, transform, scale or
	// rendered gamma LUT change, but not for an image description change, which
	// also changes how every pixel is encoded. Without this, anything left
	// undamaged keeps the luminance mapping it had when last drawn.
	wlr_output_schedule_frame(target->wlr_output);
	wlr_damage_ring_add_whole(&target->scene_output->damage_ring);

	// Swapping the image description reconfigures the output; wlroots refuses
	// such a commit unless the disruption is explicitly allowed.
	target->pending.allow_reconfiguration = true;

	if (!mango_scene_output_commit(target->scene_output, &target->pending)) {
		wlr_log(WLR_ERROR, "togglehdr: commit failed on %s, reverting",
				target->wlr_output->name);
		target->hdr_enable = prev_enable;
		target->is_hdr_enabling = prev_enabling;
		// The commit helper only recycles pending on success.
		wlr_output_state_finish(&target->pending);
		wlr_output_state_init(&target->pending);
		return false;
	}

	wlr_output_effective_resolution(target->wlr_output, &target->m.width,
									&target->m.height);
	return true;
}
void toggle_hdr(const Arg *arg) {
	// arg->i: 1 = on, 0 = off, -1 = toggle (also the default when no argument
	// was given, so a bare `togglehdr` binding does the obvious thing).
	if (arg->v && strcmp(arg->v, "all") == 0) {
		Monitor *m = NULL;
		bool want;

		if (arg->i < 0) {
			// One decision for every output: flipping each against its own
			// state would leave a multi-monitor desk half on and half off, and
			// the next press would swap the halves instead of converging.
			bool any_on = false;
			wl_list_for_each(m, &server.monitors, link) {
				if (m->wlr_output->enabled && m->hdr_enable) {
					any_on = true;
					break;
				}
			}
			want = !any_on;
		} else {
			want = arg->i != 0;
		}

		wl_list_for_each(m, &server.monitors, link) togglehdr_output(m, want);
		return;
	}

	Monitor *m = NULL, *target = NULL;

	if (arg->v && *arg->v) {
		wl_list_for_each(m, &server.monitors, link) {
			if (m->wlr_output->enabled &&
				strcmp(m->wlr_output->name, arg->v) == 0) {
				target = m;
				break;
			}
		}
		if (!target) {
			wlr_log(WLR_ERROR, "togglehdr: no enabled output named %s", arg->v);
			return;
		}
	} else {
		target = server.selected_monitor;
	}

	if (!target)
		return;

	togglehdr_output(target, arg->i < 0 ? !target->hdr_enable : (arg->i != 0));
}

static uint32_t output_formats_8bit[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888, DRM_FORMAT_RGBX8888,
	DRM_FORMAT_BGRX8888, DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888, DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
};

static uint32_t output_formats_10bit[] = {
	DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_RGBX1010102,
	DRM_FORMAT_BGRX1010102, DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_RGBA1010102, DRM_FORMAT_BGRA1010102,
};
