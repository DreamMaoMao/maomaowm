#include "mango/animation/tag.h"
#include "mango/animation/client.h"
#include "mango/common/server.h"
#include "mango/layout/layout.h"
#include "mango/manage/monitor.h"
#include "mango/manage/client.h"
#include "mango/common/util.h"

void set_tagin_animation(Monitor *m, Client *c) {
	if (c->animation.running) {
		c->animainit_geom.x = c->animation.current.x;
		c->animainit_geom.y = c->animation.current.y;
		return;
	}

	if ((c->isglobal || c->isunglobal) ||
		(c->tags & (1 << (m->pertag->prevtag - 1)) &&
		 c->tags & (1 << (m->pertag->curtag - 1)))) {
		c->animation.tagouting = false;
		c->animation.tagouted = false;
		c->animation.tagining = false;
		c->animation.action = MOVE;
		return;
	}

	bool going_forward = m->carousel_anim_dir
							 ? m->carousel_anim_dir > 0
							 : m->pertag->curtag > m->pertag->prevtag;

	if (going_forward) {

		c->animainit_geom.x = config.tag_animation_direction == VERTICAL
								  ? c->animation.current.x
								  : MANGO_MAX(c->mon->m.x + c->mon->m.width,
											  c->geom.x + c->mon->m.width);
		c->animainit_geom.y = config.tag_animation_direction == VERTICAL
								  ? MANGO_MAX(c->mon->m.y + c->mon->m.height,
											  c->geom.y + c->mon->m.height)
								  : c->animation.current.y;

	} else {

		c->animainit_geom.x = config.tag_animation_direction == VERTICAL
								  ? c->animation.current.x
								  : MANGO_MIN(m->m.x - c->geom.width,
											  c->geom.x - c->mon->m.width);
		c->animainit_geom.y = config.tag_animation_direction == VERTICAL
								  ? MANGO_MIN(m->m.y - c->geom.height,
											  c->geom.y - c->mon->m.height)
								  : c->animation.current.y;
	}
}

void set_arrange_visible(Monitor *m, Client *c, bool want_animation) {

	/* A swallowed window disappears completely and stays hidden. */
	if (c->is_logic_hide)
		return;

	bool was_enabled = c->scene->node.enabled;

	if (!ISTILED(c) || (!c->is_clip_to_hide || !is_scroller_layout(c->mon))) {
		c->is_clip_to_hide = false;
		c->is_logic_hide = false;
		wlr_scene_node_set_enabled(&c->scene->node, true);
		/* In overview the real surface tree is replaced by the card tree, so it
		 * stays disabled. */
		if (!c->ov_card_tree)
			wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	/* Special workspace clients slide in from above the monitor */
	if (c->tags & TAG0_MASK) {
		c->animation.tag_from_rule = false;
		c->animation.tagouting = false;
		c->animation.tagouted = false;
		client_raise_group(c);
		if (want_animation && config.animations) {
			c->animation.tagining = true;
			c->animainit_geom = c->geom;
			c->animainit_geom.y = c->mon->m.y - c->geom.height;
		} else {
			c->animainit_geom.x = c->animation.current.x;
			c->animainit_geom.y = c->animation.current.y;
		}
		resize_apply(c, c->geom, (ResizeOpts){.skip_ov_enter_anim = true});
		return;
	}

	if (!was_enabled && !c->animation.tag_from_rule && want_animation &&
		m->pertag->prevtag != 0 && m->pertag->curtag != 0 &&
		config.animations) {
		c->animation.tagining = true;
		set_tagin_animation(m, c);
	} else {
		c->animainit_geom.x = c->animation.current.x;
		c->animainit_geom.y = c->animation.current.y;
	}

	c->animation.tag_from_rule = false;
	c->animation.tagouting = false;
	c->animation.tagouted = false;
	resize_apply(c, c->geom, (ResizeOpts){.skip_ov_enter_anim = true});
}

void set_tagout_animation(Monitor *m, Client *c) {
	if ((c->isglobal || c->isunglobal) ||
		(c->tags & (1 << (m->pertag->prevtag - 1)) &&
		 c->tags & (1 << (m->pertag->curtag - 1)))) {
		c->animation.tagouting = false;
		c->animation.tagouted = false;
		c->animation.tagining = false;
		c->animation.action = MOVE;
		return;
	}

	bool going_forward = m->carousel_anim_dir
							 ? m->carousel_anim_dir > 0
							 : m->pertag->curtag > m->pertag->prevtag;
	if (going_forward) {
		c->pending = c->geom;
		c->pending.x = config.tag_animation_direction == VERTICAL
						   ? c->animation.current.x
						   : MANGO_MIN(c->mon->m.x - c->geom.width,
									   c->geom.x - c->mon->m.width);
		c->pending.y = config.tag_animation_direction == VERTICAL
						   ? MANGO_MIN(c->mon->m.y - c->geom.height,
									   c->geom.y - c->mon->m.height)
						   : c->animation.current.y;

		resize(c, c->geom, 0);
	} else {
		c->pending = c->geom;
		c->pending.x = config.tag_animation_direction == VERTICAL
						   ? c->animation.current.x
						   : MANGO_MAX(c->mon->m.x + c->mon->m.width,
									   c->geom.x + c->mon->m.width);
		c->pending.y = config.tag_animation_direction == VERTICAL
						   ? MANGO_MAX(c->mon->m.y + c->mon->m.height,
									   c->geom.y + c->mon->m.height)
						   : c->animation.current.y;
		resize(c, c->geom, 0);
	}
}
void set_arrange_hidden(Monitor *m, Client *c, bool want_animation) {
	/* A swallowed window disappears completely and stays hidden (avoids
	 * triggering a tag animation flash). */
	if (c->is_logic_hide) {
		wlr_scene_node_set_enabled(&c->scene->node, false);
		return;
	}

	/* In overview every tag window must show its card and must not be disabled
	 * by the hiding logic. */
	if (c->ov_card_tree) {
		c->is_logic_hide = false;
		c->is_clip_to_hide = false;
		wlr_scene_node_set_enabled(&c->scene->node, true);
		c->animation.running = false;
		return;
	}

	/* Special workspace windows should animate out or hide when special
	 * workspace is not active */
	if (c->tags & TAG0_MASK) {
		if (want_animation && config.animations && !c->animation.tagouted &&
			c->scene->node.enabled) {
			c->animation.tagouting = true;
			c->animation.tagining = false;
			c->pending = c->geom;
			c->pending.y = c->mon->m.y - c->geom.height;
			client_raise_group(c);
			resize(c, c->geom, 0);
		} else {
			c->animation.running = false;
			c->animation.tagouting = false;
			c->animation.tagining = false;
			wlr_scene_node_set_enabled(&c->scene->node, false);
			c->animainit_geom = c->current = c->pending = c->animation.current =
				c->geom;
		}
		return;
	}

	if ((c->tags & (1 << (m->pertag->prevtag - 1))) &&
		m->pertag->prevtag != 0 && m->pertag->curtag != 0 &&
		config.animations) {
		c->animation.tagouting = true;
		c->animation.tagining = false;
		set_tagout_animation(m, c);
	} else {
		c->animation.running = false;
		wlr_scene_node_set_enabled(&c->scene->node, false);
		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;
	}
}
