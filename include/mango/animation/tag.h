#ifndef __ANIMATION_TAG_H__
#define __ANIMATION_TAG_H__ 1

#include "mango/mango.h"

void set_tagin_animation(Monitor *m, Client *c);
void set_arrange_visible(Monitor *m, Client *c, bool want_animation);
void set_tagout_animation(Monitor *m, Client *c);
void set_arrange_hidden(Monitor *m, Client *c, bool want_animation);

#endif
