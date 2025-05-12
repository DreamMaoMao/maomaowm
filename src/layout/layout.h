void fibonacci(Monitor *mon, int s) {
  unsigned int i = 0, n = 0, nx, ny, nw, nh;
  Client *c;
  unsigned int cur_gappih = enablegaps ? mon->gappih : 0;
  unsigned int cur_gappiv = enablegaps ? mon->gappiv : 0;
  unsigned int cur_gappoh = enablegaps ? mon->gappoh : 0;
  unsigned int cur_gappov = enablegaps ? mon->gappov : 0;

  cur_gappih = smartgaps && mon->visible_clients == 1  ? 0 : cur_gappih;
  cur_gappiv = smartgaps && mon->visible_clients == 1  ? 0 : cur_gappiv;
  cur_gappoh = smartgaps && mon->visible_clients == 1  ? 0 : cur_gappoh;
  cur_gappov = smartgaps && mon->visible_clients == 1  ? 0 : cur_gappov;
  // Count visible clients
  wl_list_for_each(c, &clients, link) if (VISIBLEON(c, mon) && !c->isfloating &&
                                          !c->iskilling && !c->isfullscreen &&
                                          !c->ismaxmizescreen &&
                                          !c->animation.tagouting) n++;

  if (n == 0)
    return;

  // Initial dimensions including outer gaps
  nx = mon->w.x + cur_gappoh;
  ny = mon->w.y + cur_gappov;
  nw = mon->w.width - 2 * cur_gappoh;
  nh = mon->w.height - 2 * cur_gappov;

  // First pass: calculate client geometries
  wl_list_for_each(c, &clients, link) {
    if (!VISIBLEON(c, mon) || c->isfloating || c->iskilling ||
        c->isfullscreen || c->ismaxmizescreen || c->animation.tagouting)
      continue;

    c->bw = mon->visible_clients == 1 && no_border_when_single && smartgaps ? 0 : borderpx;
    if ((i % 2 && nh / 2 > 2 * c->bw) || (!(i % 2) && nw / 2 > 2 * c->bw)) {
      if (i < n - 1) {
        if (i % 2) {
          if (i == 1) {
            nh = nh * mon->pertag->smfacts[mon->pertag->curtag];
          } else {
            nh = (nh - cur_gappiv) / 2;
          }
        } else {
          nw = (nw - cur_gappih) / 2;
        }

        if ((i % 4) == 2 && !s)
          nx += nw + cur_gappih;
        else if ((i % 4) == 3 && !s)
          ny += nh + cur_gappiv;
      }

      if ((i % 4) == 0) {
        if (s)
          ny += nh + cur_gappiv;
        else
          ny -= nh + cur_gappiv;
      } else if ((i % 4) == 1)
        nx += nw + cur_gappih;
      else if ((i % 4) == 2)
        ny += nh + cur_gappiv;
      else if ((i % 4) == 3) {
        if (s)
          nx += nw + cur_gappih;
        else
          nx -= nw + cur_gappih;
      }

      if (i == 0) {
        if (n != 1)
          nw = (mon->w.width - 2 * cur_gappoh) *
               mon->pertag->mfacts[mon->pertag->curtag];
        ny = mon->w.y + cur_gappov;
      } else if (i == 1) {
        nw = mon->w.width - 2 * cur_gappoh - nw - cur_gappih;
      } else if (i == 2) {
        nh = mon->w.height - 2 * cur_gappov - nh - cur_gappiv;
      }
      i++;
    }

    c->geom = (struct wlr_box){.x = nx, .y = ny, .width = nw, .height = nh};
  }

  // Second pass: apply gaps between clients
  wl_list_for_each(c, &clients, link) {
    if (!VISIBLEON(c, mon) || c->isfloating || c->iskilling ||
        c->isfullscreen || c->ismaxmizescreen || c->animation.tagouting)
      continue;

    unsigned int right_gap = 0;
    unsigned int bottom_gap = 0;
    Client *nc;

    wl_list_for_each(nc, &clients, link) {
      if (!VISIBLEON(nc, mon) || nc->isfloating || nc->iskilling ||
          nc->isfullscreen || nc->ismaxmizescreen || nc->animation.tagouting)
        continue;

      if (c == nc)
        continue;

      // Check for right neighbor
      if (c->geom.y == nc->geom.y && c->geom.x + c->geom.width == nc->geom.x) {
        right_gap = cur_gappih;
      }

      // Check for bottom neighbor
      if (c->geom.x == nc->geom.x && c->geom.y + c->geom.height == nc->geom.y) {
        bottom_gap = cur_gappiv;
      }
    }

    resize(c,
           (struct wlr_box){.x = c->geom.x,
                            .y = c->geom.y,
                            .width = c->geom.width - right_gap,
                            .height = c->geom.height - bottom_gap},
           0);
  }
}

void dwindle(Monitor *mon) { fibonacci(mon, 1); }

void spiral(Monitor *mon) { fibonacci(mon, 0); }

// 网格布局窗口大小和位置计算
void grid(Monitor *m) {
  unsigned int i, n;
  unsigned int cx, cy, cw, ch;
  unsigned int dx;
  unsigned int cols, rows, overcols;
  Client *c;
  n = 0;

  // 第一次遍历，计算 n 的值
  wl_list_for_each(c, &clients, link) {
    if (VISIBLEON(c, c->mon) && !c->iskilling && !c->animation.tagouting &&
        c->mon == selmon) {
      n++;
    }
  }

  if (n == 0) {
    return; // 没有需要处理的客户端，直接返回
  }

  if (n == 1) {
    wl_list_for_each(c, &clients, link) {
      c->bw = m->visible_clients == 1 && no_border_when_single && smartgaps ? 0 : borderpx;
      if (VISIBLEON(c, c->mon) && !c->iskilling && !c->animation.tagouting &&
          c->mon == selmon) {
        cw = (m->w.width - 2 * overviewgappo) * 0.7;
        ch = (m->w.height - 2 * overviewgappo) * 0.8;
        c->geom.x = m->w.x + (m->w.width - cw) / 2;
        c->geom.y = m->w.y + (m->w.height - ch) / 2;
        c->geom.width = cw - 2 * c->bw;
        c->geom.height = ch - 2 * c->bw;
        resize(c, c->geom, 0);
        return;
      }
    }
  }

  if (n == 2) {
    cw = (m->w.width - 2 * overviewgappo - overviewgappi) / 2;
    ch = (m->w.height - 2 * overviewgappo) * 0.65;
    i = 0;
    wl_list_for_each(c, &clients, link) {
      c->bw = m->visible_clients == 1 && no_border_when_single && smartgaps ? 0 : borderpx;
      if (VISIBLEON(c, c->mon) && !c->iskilling && !c->animation.tagouting &&
          c->mon == selmon) {
        if (i == 0) {
          c->geom.x = m->w.x + overviewgappo;
          c->geom.y = m->w.y + (m->w.height - ch) / 2 + overviewgappo;
          c->geom.width = cw - 2 * c->bw;
          c->geom.height = ch - 2 * c->bw;
          resize(c, c->geom, 0);
        } else if (i == 1) {
          c->geom.x = m->w.x + cw + overviewgappo + overviewgappi;
          c->geom.y = m->w.y + (m->w.height - ch) / 2 + overviewgappo;
          c->geom.width = cw - 2 * c->bw;
          c->geom.height = ch - 2 * c->bw;
          resize(c, c->geom, 0);
        }
        i++;
      }
    }
    return;
  }

  // 计算列数和行数
  for (cols = 0; cols <= n / 2; cols++) {
    if (cols * cols >= n) {
      break;
    }
  }
  rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;

  // 计算每个客户端的高度和宽度
  ch = (m->w.height - 2 * overviewgappo - (rows - 1) * overviewgappi) / rows;
  cw = (m->w.width - 2 * overviewgappo - (cols - 1) * overviewgappi) / cols;

  // 处理多余的列
  overcols = n % cols;
  if (overcols) {
    dx = (m->w.width - overcols * cw - (overcols - 1) * overviewgappi) / 2 -
         overviewgappo;
  }

  // 调整每个客户端的位置和大小
  i = 0;
  wl_list_for_each(c, &clients, link) {
    c->bw = m->visible_clients == 1 && no_border_when_single && smartgaps ? 0 : borderpx;
    if (VISIBLEON(c, c->mon) && !c->iskilling && !c->animation.tagouting &&
        c->mon == selmon) {
      cx = m->w.x + (i % cols) * (cw + overviewgappi);
      cy = m->w.y + (i / cols) * (ch + overviewgappi);
      if (overcols && i >= n - overcols) {
        cx += dx;
      }
      c->geom.x = cx + overviewgappo;
      c->geom.y = cy + overviewgappo;
      c->geom.width = cw - 2 * c->bw;
      c->geom.height = ch - 2 * c->bw;
      resize(c, c->geom, 0);
      i++;
    }
  }
}


void
deck(Monitor *m)
{
	unsigned int mw, my;
	int i, n = 0;
	Client *c;
	unsigned int cur_gappih = enablegaps ? m->gappih : 0;
	unsigned int cur_gappiv = enablegaps ? m->gappiv : 0;
	unsigned int cur_gappoh = enablegaps ? m->gappoh : 0;
	unsigned int cur_gappov = enablegaps ? m->gappov : 0;

  cur_gappih = smartgaps && m->visible_clients == 1  ? 0 : cur_gappih;
  cur_gappiv = smartgaps && m->visible_clients == 1  ? 0 : cur_gappiv;
  cur_gappoh = smartgaps && m->visible_clients == 1  ? 0 : cur_gappoh;
  cur_gappov = smartgaps && m->visible_clients == 1  ? 0 : cur_gappov;

	wl_list_for_each(c, &clients, link)
		if (VISIBLEON(c, m) && !c->isfloating && !c->isfullscreen)
			n++;
	if (n == 0)
		return;

	// Calculate master width using mfact from pertag
	float mfact = m->pertag ? m->pertag->mfacts[m->pertag->curtag] : m->mfact;

	// Calculate master width including outer gaps
	if (n > m->nmaster)
		mw = m->nmaster ? round((m->w.width - 2 * cur_gappoh) * mfact) : 0;
	else
		mw = m->w.width - 2 * cur_gappoh;
	
	i = my = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;
		if (i < m->nmaster) {
			// Master area clients
			resize(c, (struct wlr_box){
				.x = m->w.x + cur_gappoh, 
				.y = m->w.y + cur_gappov + my, 
				.width = mw,
				.height = (m->w.height - cur_gappov - my - cur_gappiv) / (MIN(n, m->nmaster) - i)
			}, 0);
			my += c->geom.height + cur_gappiv;
		} else {
			// Stack area clients
			resize(c, (struct wlr_box){
				.x = m->w.x + mw + cur_gappoh + cur_gappih, 
				.y = m->w.y + cur_gappov,
				.width = m->w.width - mw - 2 * cur_gappoh - cur_gappih,
				.height = m->w.height - 2 * cur_gappov
			}, 0);
			if (c == focustop(m))
				wlr_scene_node_raise_to_top(&c->scene->node);
		}
		i++;
	}
}

// 滚动布局
void scroller(Monitor *m) {
  unsigned int i, n;

  Client *c, *root_client = NULL;
  Client **tempClients = NULL; // 初始化为 NULL
  n = 0;
  struct wlr_box target_geom;
  int focus_client_index = 0;
  bool need_scroller = false;
  unsigned int cur_gappih = enablegaps ? m->gappih : 0;
  unsigned int cur_gappoh = enablegaps ? m->gappoh : 0;
  unsigned int cur_gappov = enablegaps ? m->gappov : 0;

  cur_gappih = smartgaps && m->visible_clients == 1  ? 0 : cur_gappih;
  cur_gappoh = smartgaps && m->visible_clients == 1  ? 0 : cur_gappoh;
  cur_gappov = smartgaps && m->visible_clients == 1  ? 0 : cur_gappov;

  unsigned int max_client_width =
      m->w.width - 2 * scroller_structs - cur_gappih;

  // 第一次遍历，计算 n 的值
  wl_list_for_each(c, &clients, link) {
    if (VISIBLEON(c, c->mon) && !client_is_unmanaged(c) && !c->isfloating &&
        !c->isfullscreen && !c->ismaxmizescreen && !c->iskilling &&
        !c->animation.tagouting && c->mon == m) {
      n++;
    }
  }

  if (n == 0) {
    return; // 没有需要处理的客户端，直接返回
  }

  // 动态分配内存
  tempClients = malloc(n * sizeof(Client *));
  if (!tempClients) {
    // 处理内存分配失败的情况
    return;
  }

  // 第二次遍历，填充 tempClients
  n = 0;
  wl_list_for_each(c, &clients, link) {
    if (VISIBLEON(c, c->mon) && !client_is_unmanaged(c) && !c->isfloating &&
        !c->isfullscreen && !c->ismaxmizescreen && !c->iskilling &&
        !c->animation.tagouting && c->mon == m) {
      tempClients[n] = c;
      n++;
    }
  }

  if (n == 1) {
    c = tempClients[0];
    target_geom.height = m->w.height - 2 * cur_gappov;
    target_geom.width =
        (m->w.width - 2 * cur_gappoh) * scroller_default_proportion_single;
    target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
    target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
    resize(c, target_geom, 0);
    free(tempClients); // 释放内存
    return;
  }

  if (m->sel && !client_is_unmanaged(m->sel) && !m->sel->isfloating &&
      !m->sel->ismaxmizescreen && !m->sel->isfullscreen) {
    root_client = m->sel;
  } else if (m->prevsel && !client_is_unmanaged(m->prevsel) &&
             !m->prevsel->isfloating && !m->prevsel->ismaxmizescreen &&
             !m->prevsel->isfullscreen) {
    root_client = m->prevsel;
  } else {
    root_client = center_select(m);
  }

  if (!root_client) {
    free(tempClients); // 释放内存
    return;
  }

  for (i = 0; i < n; i++) {
    c = tempClients[i];
    if (root_client == c) {
      if (!c->is_open_animation && c->geom.x >= m->w.x + scroller_structs &&
          c->geom.x + c->geom.width <= m->w.x + m->w.width - scroller_structs) {
        need_scroller = false;
      } else {
        need_scroller = true;
      }
      focus_client_index = i;
      break;
    }
  }

  target_geom.height = m->w.height - 2 * cur_gappov;
  target_geom.width = max_client_width * c->scroller_proportion;
  target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;

  if (need_scroller) {
    if (scroller_focus_center ||
        ((!m->prevsel ||
          (m->prevsel->scroller_proportion * max_client_width) +
                  (root_client->scroller_proportion * max_client_width) >
              m->w.width - 2 * scroller_structs - cur_gappih) &&
         scroller_prefer_center)) {
      target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
    } else {
      target_geom.x =
          root_client->geom.x > m->w.x + (m->w.width) / 2
              ? m->w.x + (m->w.width -
                          root_client->scroller_proportion * max_client_width -
                          scroller_structs)
              : m->w.x + scroller_structs;
    }
    resize(tempClients[focus_client_index], target_geom, 0);
  } else {
    target_geom.x = c->geom.x;
    resize(tempClients[focus_client_index], target_geom, 0);
  }

  for (i = 1; i <= focus_client_index; i++) {
    c = tempClients[focus_client_index - i];
    target_geom.width = max_client_width * c->scroller_proportion;
    target_geom.x = tempClients[focus_client_index - i + 1]->geom.x -
                    cur_gappih - target_geom.width;
    resize(c, target_geom, 0);
  }

  for (i = 1; i < n - focus_client_index; i++) {
    c = tempClients[focus_client_index + i];
    target_geom.width = max_client_width * c->scroller_proportion;
    target_geom.x = tempClients[focus_client_index + i - 1]->geom.x +
                    cur_gappih +
                    tempClients[focus_client_index + i - 1]->geom.width;
    resize(c, target_geom, 0);
  }

  free(tempClients); // 最后释放内存
}

void tile(Monitor *m) {
  unsigned int i, n = 0, h, r, ie = enablegaps, mw, my, ty;
  Client *c;

  wl_list_for_each(c, &clients,
                   link) if (VISIBLEON(c, m) && !c->animation.tagouting &&
                             !c->iskilling && !c->isfloating &&
                             !c->isfullscreen && !c->ismaxmizescreen) n++;
  if (n == 0)
    return;

  unsigned int cur_gappih = enablegaps ? m->gappih : 0;
  unsigned int cur_gappiv = enablegaps ? m->gappiv : 0;
  unsigned int cur_gappoh = enablegaps ? m->gappoh : 0;
  unsigned int cur_gappov = enablegaps ? m->gappov : 0;

  cur_gappih = smartgaps && m->visible_clients == 1  ? 0 : cur_gappih;
  cur_gappiv = smartgaps && m->visible_clients == 1  ? 0 : cur_gappiv;
  cur_gappoh = smartgaps && m->visible_clients == 1  ? 0 : cur_gappoh;
  cur_gappov = smartgaps && m->visible_clients == 1  ? 0 : cur_gappov;

  if (n > selmon->pertag->nmasters[selmon->pertag->curtag])
    mw = selmon->pertag->nmasters[selmon->pertag->curtag]
             ? (m->w.width + cur_gappiv * ie) *
                   selmon->pertag->mfacts[selmon->pertag->curtag]
             : 0;
  else
    mw = m->w.width - 2 * cur_gappov + cur_gappiv * ie;
  i = 0;
  my = ty =cur_gappoh ;
  wl_list_for_each(c, &clients, link) {
    if (!VISIBLEON(c, m) || c->iskilling || c->animation.tagouting ||
        c->isfloating || c->isfullscreen || c->ismaxmizescreen)
      continue;
    if (i < selmon->pertag->nmasters[selmon->pertag->curtag]) {
      r = MIN(n, selmon->pertag->nmasters[selmon->pertag->curtag]) - i;
      h = (m->w.height - my -cur_gappoh  - cur_gappih * ie * (r - 1)) / r;
      resize(c,
             (struct wlr_box){.x = m->w.x + cur_gappov ,
                              .y = m->w.y + my,
                              .width = mw - cur_gappiv * ie,
                              .height = h},
             0);
      my += c->geom.height + cur_gappih * ie;
    } else {
      r = n - i;
      h = (m->w.height - ty -cur_gappoh  - cur_gappih * ie * (r - 1)) / r;
      resize(c,
             (struct wlr_box){.x = m->w.x + mw + cur_gappov,
                              .y = m->w.y + ty,
                              .width = m->w.width - mw - 2 * cur_gappov,
                              .height = h},
             0);
      ty += c->geom.height + cur_gappih * ie;
    }
    i++;
  }
}

void // 17
monocle(Monitor *m) {
  Client *c;

  wl_list_for_each(c, &clients, link) {
    if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen ||
        c->ismaxmizescreen || c->iskilling || c->animation.tagouting)
      continue;
    resize(c, m->w, 0);
  }
  if ((c = focustop(m)))
    wlr_scene_node_raise_to_top(&c->scene->node);
}