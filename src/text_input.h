#ifdef IM
#include <assert.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_input_method_v2.h>

#ifdef HANDWRITE
#include <handwrite-unstable-v1-protocol.h>
#endif

struct dwl_input_method_relay {
	struct wl_list text_inputs; // dwl_text_input::link
	struct wlr_input_method_v2 *input_method; // doesn't have to be present

        struct dwl_input_popup *popup;
		struct wlr_surface *focused_surface;
		struct wl_listener focused_surface_destroy;
	struct wl_listener text_input_new;

	struct wl_listener input_method_new;
	struct wl_listener input_method_commit;
	struct wl_listener input_method_destroy;
	struct wl_listener input_method_new_popup_surface;
	struct wl_listener input_method_grab_keyboard;
	struct wl_listener input_method_keyboard_grab_destroy;
};

struct dwl_text_input {
	struct dwl_input_method_relay *relay;

	struct wlr_text_input_v3 *input;
	struct wlr_surface *pending_focused_surface;

	struct wl_list link;

	struct wl_listener pending_focused_surface_destroy;

	struct wl_listener text_input_enable;
  //struct wl_listener text_input_commit;
  //struct wl_listener text_input_disable;
	struct wl_listener text_input_destroy;
};


struct dwl_input_popup {
	struct dwl_input_method_relay *relay;
	struct wlr_input_popup_surface_v2 *popup_surface;

  //struct wlr_scene_node *scene;
        struct wlr_scene_tree *scene;
  //struct wlr_scene_node *scene_surface;
        struct wlr_scene_tree *scene_surface;

	int x, y;
	bool visible;

	
	struct wl_listener popup_map;
	struct wl_listener popup_unmap;
	struct wl_listener popup_destroy;
  //struct wl_listener popup_surface_commit;
  //struct wl_listener focused_surface_unmap;
};


void dwl_input_method_relay_init(struct dwl_input_method_relay *relay);
// Updates currently focused surface. Surface must belong to the same
// seat.
void dwl_input_method_relay_set_focus(struct dwl_input_method_relay *relay,
	struct wlr_surface *surface);

static void handle_im_grab_keyboard(struct wl_listener *listener, void *data);
static void handle_im_keyboard_grab_destroy(struct wl_listener *listener,
                                            void *data);
static void input_popup_update(struct dwl_input_popup *popup);
static struct dwl_text_input *relay_get_focused_text_input(struct dwl_input_method_relay *relay);

struct wlr_input_method_manager_v2 *input_method_manager;
struct wlr_text_input_manager_v3 *text_input_manager;
struct dwl_input_method_relay *input_relay;
// static int NO_printstatus=0; // i suspect that printstatus will make dwl hang with fcitx5

#ifdef HANDWRITE
static void receive_handwrite_text_from_handwrite_input_app(struct wl_client *client, struct wl_resource *resource, const char *text){
  struct dwl_text_input *text_input;

  wlr_log(WLR_INFO,"receive_handwrite_text_from_handwrite_input_app called with:%s",text);
  text_input = relay_get_focused_text_input(input_relay);

  if (text_input){
                        wlr_log(WLR_INFO,"receive_handwrite_text_from_handwrite_input_app will commit %s",text);
			wlr_text_input_v3_send_commit_string(text_input->input,text);
			wlr_text_input_v3_send_done(text_input->input);
  }else wlr_log(WLR_INFO, "no focused text_input for handwrite");
}

const struct zwp_handwrite_v1_interface handwrite_interface_impl = {
    .send_handwrite_text = receive_handwrite_text_from_handwrite_input_app,
};

#endif

static struct wlr_input_method_keyboard_grab_v2 *keyboard_get_im_grab(KeyboardGroup* kb) {
	struct wlr_input_method_v2 *input_method = input_relay->input_method;

	if (!input_method){
	  wlr_log(WLR_DEBUG, "keypress keyboard_get_im_grab return NULL:no input_method");
	  return NULL;
	} else if (!input_method->keyboard_grab){
	  wlr_log(WLR_DEBUG, "keypress keyboard_get_im_grab return NULL:no input_method->keyboard_grab");
	  return NULL;
	} else if (kb != kb_group) {
	  wlr_log(WLR_DEBUG, "keypress keyboard_get_im_grab return NULL:virtual_keyboard");
	  return NULL;
	} else return input_method->keyboard_grab;
}

static void handle_im_grab_keyboard(struct wl_listener *listener, void *data) {
	struct dwl_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_grab_keyboard);
	//wl_container_of 宏的第二个参数sample, 这里是relay,无须是已经初始化的变量,只要是返回值类型的变量就可以了,这里就是dwl_input_method_relay类型的变量
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab = data;

	// send modifier state to grab
	struct wlr_keyboard *active_keyboard = wlr_seat_get_keyboard(seat);
        if (active_keyboard){
	       wlr_log(WLR_INFO,"im_grab_keyboard");
	       wlr_input_method_keyboard_grab_v2_set_keyboard(keyboard_grab,active_keyboard);
               wlr_input_method_keyboard_grab_v2_send_modifiers(keyboard_grab, &active_keyboard->modifiers);
	}
        else
	       wlr_log(WLR_INFO,"im_grab_keyboard but no active keyboard");
		       

	wl_signal_add(&keyboard_grab->events.destroy,
		&relay->input_method_keyboard_grab_destroy);
	relay->input_method_keyboard_grab_destroy.notify =
		handle_im_keyboard_grab_destroy;
}

static void handle_im_keyboard_grab_destroy(struct wl_listener *listener, void *data) {
	struct dwl_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_keyboard_grab_destroy);
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab = data;
	wlr_log(WLR_DEBUG,"im_keyboard_grab_destroy");
        wl_list_remove(&relay->input_method_keyboard_grab_destroy.link);
	if (keyboard_grab->keyboard) {
		// send modifier state to original client
		wlr_seat_keyboard_notify_modifiers(keyboard_grab->input_method->seat,
			&keyboard_grab->keyboard->modifiers);
	}
}


static struct dwl_text_input *relay_get_focusable_text_input(
		struct dwl_input_method_relay *relay) {
	struct dwl_text_input *text_input = NULL;
	wl_list_for_each(text_input, &relay->text_inputs, link) {
		if (text_input->pending_focused_surface) {
			return text_input;
		}
	}
	return NULL;
}

static struct dwl_text_input *relay_get_focused_text_input(
		struct dwl_input_method_relay *relay) {
	struct dwl_text_input *text_input = NULL;
	wl_list_for_each(text_input, &relay->text_inputs, link) {
		if (text_input->input->focused_surface) {
			return text_input;
		}
	}
	return NULL;
}

static void handle_im_commit(struct wl_listener *listener, void *data) {
	struct wlr_input_method_v2 *context;

	struct dwl_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_commit);

	struct dwl_text_input *text_input = relay_get_focused_text_input(relay);
	if (!text_input) {
		return;
	}

	context = data;
	assert(context == relay->input_method);
	if (context->current.preedit.text) {
		wlr_text_input_v3_send_preedit_string(text_input->input,
			context->current.preedit.text,
			context->current.preedit.cursor_begin,
			context->current.preedit.cursor_end);
		wlr_log(WLR_DEBUG,"preedit_text: %s", context->current.preedit.text);
	}
	if (context->current.commit_text) {
		wlr_text_input_v3_send_commit_string(text_input->input,
			context->current.commit_text);
		wlr_log(WLR_DEBUG,"commit_text: %s", context->current.commit_text);
	}
	if (context->current.delete.before_length
			|| context->current.delete.after_length) {
		wlr_text_input_v3_send_delete_surrounding_text(text_input->input,
			context->current.delete.before_length,
			context->current.delete.after_length);
	}
	wlr_text_input_v3_send_done(text_input->input);
}

static void text_input_set_pending_focused_surface(
		struct dwl_text_input *text_input, struct wlr_surface *surface) {
	wl_list_remove(&text_input->pending_focused_surface_destroy.link);
	text_input->pending_focused_surface = surface;

	if (surface) {
		wl_signal_add(&surface->events.destroy,
			&text_input->pending_focused_surface_destroy);
	} else {
		wl_list_init(&text_input->pending_focused_surface_destroy.link);
	}
}

static void handle_im_destroy(struct wl_listener *listener, void *data) {
	struct dwl_text_input *text_input;

	struct dwl_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_destroy);
	struct wlr_input_method_v2 *context = data;
	wlr_log(WLR_INFO,"IM destroy");
        assert(context == relay->input_method);

	wl_list_remove(&relay->input_method_commit.link);
	wl_list_remove(&relay->input_method_grab_keyboard.link);
	wl_list_remove(&relay->input_method_new_popup_surface.link);
	wl_list_remove(&relay->input_method_destroy.link);

	relay->input_method = NULL;

	text_input = relay_get_focused_text_input(relay);
	if (text_input) {
		// keyboard focus is still there, so keep the surface at hand in case
		// the input method returns
		text_input_set_pending_focused_surface(text_input,
			text_input->input->focused_surface);
		wlr_text_input_v3_send_leave(text_input->input);
	}
}

static void relay_send_im_state(struct dwl_input_method_relay *relay,
	struct wlr_text_input_v3 *input) {
struct wlr_input_method_v2 *input_method = relay->input_method;
if (!input_method) {
	return;
}

// 只发送已修改的状态
if (input->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT) {
	wlr_input_method_v2_send_surrounding_text(input_method,
		input->current.surrounding.text, input->current.surrounding.cursor,
		input->current.surrounding.anchor);
}

wlr_input_method_v2_send_text_change_cause(input_method,
	input->current.text_change_cause);
	
if (input->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE) {
	wlr_input_method_v2_send_content_type(input_method,
		input->current.content_type.hint, input->current.content_type.purpose);
}

wlr_input_method_v2_send_done(input_method);
}

static void handle_text_input_enable(struct wl_listener *listener, void *data) {
	struct dwl_text_input *text_input = wl_container_of(listener, text_input,
		text_input_enable);
	if (text_input->relay->input_method == NULL) {
		wlr_log(WLR_INFO, "text_input_enable but input method is NULL");
		return;
	}

        wlr_log(WLR_INFO,"text_input_enable");
#ifdef XWAYLAND
	wlr_input_method_v2_send_activate(text_input->relay->input_method);
	wlr_log(WLR_INFO,"input_method activate for xwayland");
#endif

	relay_send_im_state(text_input->relay, text_input->input);
}

static void relay_disable_text_input(struct dwl_input_method_relay *relay,
		struct dwl_text_input *text_input) {
	if (relay->input_method == NULL) {
		wlr_log(WLR_INFO, "text_input_disable, but input method is NULL");
		return;
	}
        wlr_log(WLR_INFO,"text_input_disable");
	
#ifdef XWAYLAND
	// https://gitee.com/guyuming76/dwl/commit/59328d6ecbbef1b1cd6e5ea8d90d78ccddd5c263
	wlr_input_method_v2_send_deactivate(relay->input_method);
	wlr_log(WLR_INFO,"input_method deactivate for xwayland");
#endif
	//but if you keep the line above while remove the line below, input Chinese in geogebra(xwayland) won't work 
	relay_send_im_state(relay, text_input->input);
}


static void handle_text_input_destroy(struct wl_listener *listener,
		void *data) {
	struct dwl_text_input *text_input = wl_container_of(listener, text_input,
		text_input_destroy);

	if (text_input->input->current_enabled) {
	    wlr_log(WLR_INFO,"text_input_destroy when still enabled");
	    relay_disable_text_input(text_input->relay, text_input);
	}
	else {
	    wlr_log(WLR_INFO,"text_input_destroy");
        }
	
	text_input_set_pending_focused_surface(text_input, NULL);
	//wl_list_remove(&text_input->text_input_commit.link);
	wl_list_remove(&text_input->text_input_destroy.link);
	//wl_list_remove(&text_input->text_input_disable.link);
	wl_list_remove(&text_input->text_input_enable.link);
	wl_list_remove(&text_input->link);
	free(text_input);
}

static void handle_pending_focused_surface_destroy(struct wl_listener *listener,
		void *data) {
	struct dwl_text_input *text_input = wl_container_of(listener, text_input,
		pending_focused_surface_destroy);
	struct wlr_surface *surface = data;
	assert(text_input->pending_focused_surface == surface);
	text_input->pending_focused_surface = NULL;
	wl_list_remove(&text_input->pending_focused_surface_destroy.link);
	wl_list_init(&text_input->pending_focused_surface_destroy.link);
}


static void relay_handle_text_input_new(struct wl_listener *listener,
		void *data) {
	struct dwl_input_method_relay *relay = wl_container_of(listener, relay,
		text_input_new);
	struct wlr_text_input_v3 *wlr_text_input = data;
	if (seat != wlr_text_input->seat) {
		return;
	}

        struct dwl_text_input *input;
	input = calloc(1, sizeof(*input));
	if (!input) {
        	wlr_log(WLR_ERROR, "dwl_text_input calloc failed");
		return;
	}
	wlr_log(WLR_INFO, "dwl_text_input calloc");
	input->input = wlr_text_input;
	input->relay = relay;

	wl_list_insert(&relay->text_inputs, &input->link);

	input->text_input_enable.notify = handle_text_input_enable;
	wl_signal_add(&wlr_text_input->events.enable, &input->text_input_enable);

	input->text_input_destroy.notify = handle_text_input_destroy;
	wl_signal_add(&wlr_text_input->events.destroy, &input->text_input_destroy);

	input->pending_focused_surface_destroy.notify =
		handle_pending_focused_surface_destroy;
	wl_list_init(&input->pending_focused_surface_destroy.link);
}


static LayerSurface* layer_surface_from_wlr_layer_surface_v1(
		struct wlr_layer_surface_v1* layer_surface) {
	return layer_surface->data;
}


static void get_parent_and_output_box(struct wlr_surface *focused_surface,
		struct wlr_box *parent, struct wlr_box *output_box) {
	struct wlr_output *output;
	struct wlr_box output_box_tmp;
	struct wlr_layer_surface_v1 *layer_surface;

	if ((layer_surface=wlr_layer_surface_v1_try_from_wlr_surface(focused_surface))) {
		LayerSurface* layer =
			layer_surface_from_wlr_layer_surface_v1(layer_surface);
		output = layer->layer_surface->output;
		wlr_output_layout_get_box(output_layout, output,&output_box_tmp);
		*parent = layer->geom;
		parent->x += output_box_tmp.x;
		parent->y += output_box_tmp.y;
		wlr_log(WLR_INFO,"get_parent_and_output_box layersurface  output_box_tmp->x %d y %d",output_box_tmp.x, output_box_tmp.y);
		wlr_log(WLR_INFO,"get_parent_and_output_box layersurface  parent->x %d y %d",parent->x,parent->y);
	} else {
	       //Client *client = client_from_wlr_surface(focused_surface);
        Client *client = NULL;
        LayerSurface *l = NULL;
        toplevel_from_wlr_surface(focused_surface, &client, &l);
		
		output = wlr_output_layout_output_at(output_layout,
			client->geom.x, client->geom.y);
		wlr_output_layout_get_box(output_layout, output,&output_box_tmp);
		
		parent->x = client->geom.x + client->bw;
		parent->y = client->geom.y + client->bw;
		parent->width = client->geom.width;
		parent->height = client->geom.height;
		wlr_log(WLR_INFO,"get_parent_and_output_box client  output_box_tmp->x %d y %d",output_box_tmp.x, output_box_tmp.y);
		wlr_log(WLR_INFO,"get_parent_and_output_box client  client->geom.x %d y %d",client->geom.x,client->geom.y);
		wlr_log(WLR_INFO,"get_parent_and_output_box client  client->bw %d",client->bw);
		wlr_log(WLR_INFO,"get_parent_and_output_box client  parent->x %d y %d",parent->x,parent->y);
	}

	//*output_box = output_box_tmp;
	memcpy(output_box,&output_box_tmp,sizeof(struct wlr_box));
	wlr_log(WLR_INFO,"get_parent_and_output_box output_box  x %d y %d width %d height %d",output_box->x,output_box->y,output_box->width,output_box->height);
}

static void input_popup_update(struct dwl_input_popup *popup) {
	struct wlr_surface* focused_surface;
	struct wlr_box output_box, parent, cursor;
	int x1, x2, y1, y2, x, y, available_right, available_left, available_down,
			available_up, popup_width, popup_height;
	bool cursor_rect, x1_in_bounds, y1_in_bounds, x2_in_bounds, y2_in_bounds;

	struct dwl_text_input *text_input =
		relay_get_focused_text_input(popup->relay);
	if (!text_input|| !text_input->input->focused_surface) {
		return;
	}


	if (!popup->popup_surface->surface->mapped) {
		return;
	}

	cursor_rect = text_input->input->current.features
		& WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE;

	focused_surface = text_input->input->focused_surface;
	cursor = text_input->input->current.cursor_rectangle;

	get_parent_and_output_box(focused_surface, &parent, &output_box);

	popup_width = popup->popup_surface->surface->current.width;
	popup_height = popup->popup_surface->surface->current.height;

	if (!cursor_rect) {
		cursor.x = 0;
		cursor.y = 0;
		cursor.width = parent.width;
		cursor.height = parent.height;
		wlr_log(WLR_INFO,"input_popup_update !cursor_rect");

		popup->x=parent.x;
		popup->y=parent.y;
		popup->visible=true;
	}
	else {
	        wlr_log(WLR_INFO,"input_popup_update cursor x %d y %d width %d height %d",cursor.x,cursor.y,cursor.width,cursor.height);

	        x1 = parent.x + cursor.x;
	        x2 = parent.x + cursor.x + cursor.width;
	        y1 = parent.y + cursor.y;
	        y2 = parent.y + cursor.y + cursor.height;
	        x = x1;
	        y = y2;

	        wlr_log(WLR_INFO,"input_popup_update  x1 %d x2 %d y1 %d y2 %d;  x %d y %d",x1,x2,y1,y2,x,y);
	        available_right = output_box.x + output_box.width - x1;
	        available_left = x2 - output_box.x;
	        if (available_right < popup_width && available_left > available_right) {
	               x = x2 - popup_width;
		       wlr_log(WLR_INFO,"input_popup_update available_left %d popup_width %d available_right %d; x %d",available_left,popup_width,available_right,x);
	        }

	        available_down = output_box.y + output_box.height - y2;
	        available_up = y1 - output_box.y;
	        if (available_down < popup_height && available_up > available_down) {
	              y = y1 - popup_height;
		      wlr_log(WLR_INFO,"input_popup_update available up %d popup_height %d available_down %d; y %d",available_up,popup_height,available_down,y);
	        }

	        popup->x = x;
	        popup->y = y;

         	// Hide popup if cursor position is completely out of bounds
	        x1_in_bounds = (cursor.x >= 0 && cursor.x < parent.width);
	        y1_in_bounds = (cursor.y >= 0 && cursor.y < parent.height);
	        x2_in_bounds = (cursor.x + cursor.width >= 0
		                         && cursor.x + cursor.width < parent.width);
	        y2_in_bounds = (cursor.y + cursor.height >= 0
		                         && cursor.y + cursor.height < parent.height);
	        popup->visible =
		                      (x1_in_bounds && y1_in_bounds) || (x2_in_bounds && y2_in_bounds);

                struct wlr_box box = {
			.x = x1 - x,
			.y = y1 - y,
			.width = cursor.width,
			.height = cursor.height,
		};
		wlr_input_popup_surface_v2_send_text_input_rectangle(
			popup->popup_surface, &box);
		wlr_log(WLR_INFO,"input_popup_update send_text_input_rect box.x %d box.y %d",box.x,box.y);

	}
        
        wlr_log(WLR_INFO,"input_popup_update x %d y %d visible %s",popup->x,popup->y,popup->visible?"true":"false");
	wlr_scene_node_set_position(&popup->scene->node, popup->x, popup->y);
}

static void handle_im_popup_map(struct wl_listener *listener, void *data) {
	struct dwl_input_popup *popup =
		wl_container_of(listener, popup, popup_map);

	wlr_log(WLR_INFO, "IM_popup_map");
	
	popup->scene = wlr_scene_tree_create(layers[LyrIMPopup]);
	popup->scene_surface = wlr_scene_subsurface_tree_create(popup->scene,popup->popup_surface->surface);
	popup->scene_surface->node.data = popup;

	input_popup_update(popup);

}

static void handle_im_popup_unmap(struct wl_listener *listener, void *data) {
        struct dwl_input_popup *popup =
		wl_container_of(listener, popup, popup_unmap);

	wlr_log(WLR_INFO,"im_popup_unmap");
	wlr_scene_node_destroy(&popup->scene->node);
}

static void handle_im_popup_destroy(struct wl_listener *listener, void *data) {
        struct dwl_input_method_relay *relay;
        struct dwl_input_popup *popup =
		wl_container_of(listener, popup, popup_destroy);

	wlr_log(WLR_INFO,"im_popup_destroy");
	
        wl_list_remove(&popup->popup_destroy.link);
	wl_list_remove(&popup->popup_unmap.link);
	wl_list_remove(&popup->popup_map.link);

	relay=popup->relay;
	free(popup->relay->popup);
	relay->popup=NULL;
}


static void handle_im_new_popup_surface(struct wl_listener *listener, void *data) {
	// struct dwl_text_input* text_input;

	struct dwl_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_new_popup_surface);
	struct dwl_input_popup *popup = calloc(1, sizeof(*popup));

	wlr_log(WLR_INFO, "IM_new_popup_surface");
	relay->popup = popup;
	
	popup->relay = relay;
	popup->popup_surface = data;
	popup->popup_surface->data = popup;

	
	wl_signal_add(&popup->popup_surface->surface->events.map, &popup->popup_map);
	popup->popup_map.notify = handle_im_popup_map;

	wl_signal_add(&popup->popup_surface->surface->events.unmap, &popup->popup_unmap);
	popup->popup_unmap.notify = handle_im_popup_unmap;

	wl_signal_add(
		&popup->popup_surface->events.destroy, &popup->popup_destroy);
	popup->popup_destroy.notify = handle_im_popup_destroy;
}


static void relay_handle_input_method_new(struct wl_listener *listener,
		void *data) {
	struct dwl_text_input *text_input;

	struct dwl_input_method_relay *relay = wl_container_of(listener, relay,
		input_method_new);

	struct wlr_input_method_v2 *input_method = data;
	if (seat != input_method->seat) {
	        wlr_log(WLR_INFO,"input_method_new Seat does not match");
	        return;
	}

	if (relay->input_method != NULL) {
		wlr_log(WLR_INFO, "input_method_new Attempted to connect second input method to a seat");
		wlr_input_method_v2_send_unavailable(input_method);
		return;
	}

        wlr_log(WLR_INFO,"input_method_new");
	
        relay->input_method = input_method;
	wl_signal_add(&relay->input_method->events.commit,
		&relay->input_method_commit);
	relay->input_method_commit.notify = handle_im_commit;
	wl_signal_add(&relay->input_method->events.new_popup_surface,
		&relay->input_method_new_popup_surface);
	relay->input_method_new_popup_surface.notify = handle_im_new_popup_surface;
	wl_signal_add(&relay->input_method->events.grab_keyboard,
		&relay->input_method_grab_keyboard);
	relay->input_method_grab_keyboard.notify = handle_im_grab_keyboard;
	wl_signal_add(&relay->input_method->events.destroy,
		&relay->input_method_destroy);
	relay->input_method_destroy.notify = handle_im_destroy;

        wlr_input_method_v2_send_activate(relay->input_method);

	text_input = relay_get_focusable_text_input(relay);
	if (text_input) {
		wlr_text_input_v3_send_enter(text_input->input,
			text_input->pending_focused_surface);
		text_input_set_pending_focused_surface(text_input, NULL);
	}
}


/*
 * Usually this function is not called because the client destroys the surface
 * role (like xdg_toplevel) first and input_method_relay_set_focus() is called
 * before wl_surface is destroyed.
 */
static void
relay_handle_focused_surface_destroy(struct wl_listener *listener, void *data)
{
	struct dwl_input_method_relay *relay =
		wl_container_of(listener, relay, focused_surface_destroy);
	assert(relay->focused_surface == data);

	dwl_input_method_relay_set_focus(relay, NULL);
}

void dwl_input_method_relay_init(struct dwl_input_method_relay *relay) {
	wl_list_init(&relay->text_inputs);

	relay->popup=NULL;

	relay->text_input_new.notify = relay_handle_text_input_new;
	wl_signal_add(&text_input_manager->events.text_input,
		&relay->text_input_new);

	relay->input_method_new.notify = relay_handle_input_method_new;
	wl_signal_add(&input_method_manager->events.input_method,
		&relay->input_method_new);

	relay->focused_surface_destroy.notify = relay_handle_focused_surface_destroy;
}

void dwl_input_method_relay_set_focus(struct dwl_input_method_relay *relay,
	struct wlr_surface *surface) {
if (relay->focused_surface == surface) {
	return;
}

// 更新焦点表面
if (relay->focused_surface) {
	wl_list_remove(&relay->focused_surface_destroy.link);
}
relay->focused_surface = surface;
if (surface) {
	wl_signal_add(&surface->events.destroy,
		&relay->focused_surface_destroy);
}

struct dwl_text_input *text_input;
wl_list_for_each(text_input, &relay->text_inputs, link) {
	// 只处理与焦点表面同客户端的文本输入框
	if (surface && wl_resource_get_client(text_input->input->resource) ==
			wl_resource_get_client(surface->resource)) {
		if (relay->input_method) {
			wlr_text_input_v3_send_enter(text_input->input, surface);
		} else {
			text_input_set_pending_focused_surface(text_input, surface);
		}
	} else if (text_input->input->focused_surface) {
		if (text_input->input->current_enabled) {
			relay_disable_text_input(relay, text_input);
		}
		wlr_text_input_v3_send_leave(text_input->input);
	} else if (text_input->pending_focused_surface) {
		text_input_set_pending_focused_surface(text_input, NULL);
	}
}
}
#endif
