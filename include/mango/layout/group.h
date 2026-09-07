#ifndef __GROUP_H__
#define __GROUP_H__ 1

#include "mango/dispatch/bind.h"

void group_init(const Arg *arg);
void group_all(const Arg *arg);
void group_merge(const Arg *arg);
void group_disband(const Arg *arg);
void group_smart(const Arg *arg);

Client *group_capture_get_parent(void);
bool group_capture_spawn(Client *c, Client *group_parent);

#endif
