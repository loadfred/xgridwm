/* INCLUDE BEFORE CONFIG */
#include <xcb/xcb.h>


/* TYPEDEFS BEFORE CONFIG */
typedef union {
    const void *v;
} Arg;

typedef struct {
  uint16_t mod;
  xcb_keycode_t keycode;
  void (*func)(const Arg *);
  const Arg arg;
} Key;


/* PROTOTYPES BEFORE CONFIG */
static void spawn (const Arg *arg);
static void lowerwin (const Arg *arg);
static void raisewin (const Arg *arg);
static void togglefocus (const Arg *arg);
static void nextwin (const Arg *arg);
static void fullclient (const Arg *arg);
static void killclient (const Arg *arg);
static void quit (const Arg *arg);


/* INCLUDE */
#include "config.h"
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>


/* VARIABLES */
static xcb_connection_t *dpy;
static xcb_screen_t *screen;
static xcb_window_t win = XCB_NONE, root = XCB_NONE,
    focuswin = XCB_NONE, prevfocuswin = XCB_NONE,
    snapxwin = XCB_NONE, snapywin = XCB_NONE;
static xcb_window_t wins[MAXWINS];
static xcb_atom_t a_protocols;
static xcb_atom_t a_delete;
static uint32_t values[7];
static uint32_t snapxy[2] = {0, 0};
static bool ongrid[2] = {true, true};
static bool isrunning = true;
static const int KEYSIZE = sizeof(keys) / sizeof(keys[0]);
static const uint32_t U32NEG1 = 4294967295; /* 4294967295 = (2^32 - 1) = -1 (uint32_t) */
static const uint32_t U32POSF = 2147483647; /* positive final */


/* FUNCTIONS */
static uint32_t max(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

static uint32_t min(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static uint32_t u32abs(uint32_t a, uint32_t b) {
    return max(a, b) - min(a, b);
}

static bool
notroot (xcb_window_t w) {
  if ((w != root) && (w != XCB_NONE) && (w != snapxwin) && (w != snapywin))
    return true;
  else
    return false;
}

void
spawn (const Arg *arg) {
  if (fork() == 0) {
    setsid();
    if (fork() != 0) {
      exit(0);
    }
    execvp(((char **)arg->v)[0], (char **)arg->v);
    exit(0);
  }
  wait(NULL);
}

static void
setfocus (xcb_window_t w) {
  if (notroot(w) && (w != focuswin)) {
    xcb_set_input_focus(dpy, XCB_INPUT_FOCUS_PARENT, w, XCB_CURRENT_TIME);
    if (notroot(focuswin) && (focuswin != 0)) {
      prevfocuswin = focuswin;
    }
    focuswin = w;
  }
}

void
lowerwin () {
  if (notroot(win)) {
    values[0] = XCB_STACK_MODE_BELOW;
    xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_configure_window(dpy, snapxwin, XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_configure_window(dpy, snapywin, XCB_CONFIG_WINDOW_STACK_MODE, values);
  }
}

void
raisewin () {
  if (notroot(win)) {
    values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(dpy, focuswin, XCB_CONFIG_WINDOW_STACK_MODE, values);
  }
}

static void
desktowin (xcb_window_t w) {
  uint32_t winsnpdifs[MAXWINS][2]; /* win - snapxy: x, y */
  xcb_get_geometry_reply_t *geoms[MAXWINS];
  for (int i=0; i<MAXWINS; i++) {
    if (wins[i] == 0) continue;
    geoms[i] = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, wins[i]), NULL);
    winsnpdifs[i][0] = geoms[i]->x - snapxy[0];
    winsnpdifs[i][1] = geoms[i]->y - snapxy[1];
  }

  xcb_get_geometry_reply_t *geom;
  geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, w), NULL);
  values[5] = geom->x + geom->width / 2;
  values[6] = geom->y + geom->height / 2;

  snapxy[0] = ((int)values[5] - (int)snapxy[0] < 0)
      ? screen->width_in_pixels
      : ((int)values[5] - (int)snapxy[0] <= screen->width_in_pixels)
        ? 0
        : (1 + U32NEG1 - screen->width_in_pixels);
  snapxy[1] = ((int)values[6] - (int)snapxy[1] < 0)
      ? screen->height_in_pixels
      : ((int)values[6] - (int)snapxy[1] <= screen->height_in_pixels)
        ? 0
        : (1 + U32NEG1 - screen->height_in_pixels);

  for (int i=0; i<MAXWINS; i++) {
    if (wins[i] == 0) continue;
    values[0] = snapxy[0] + winsnpdifs[i][0];
    values[1] = snapxy[1] + winsnpdifs[i][1];
    xcb_configure_window(dpy, wins[i], XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
  }

  values[0] = (snapxy[0] > U32POSF)
      ? (snapxy[0] + screen->width_in_pixels - MARKSIZE / 2)
      : (snapxy[0] - MARKSIZE / 2);
  xcb_configure_window(dpy, snapxwin, XCB_CONFIG_WINDOW_X, values);
  values[0] = (snapxy[1] > U32POSF)
      ? (snapxy[1] + screen->height_in_pixels - MARKSIZE / 2)
      : (snapxy[1] - MARKSIZE / 2);
  xcb_configure_window(dpy, snapywin, XCB_CONFIG_WINDOW_Y, values);
  xcb_unmap_window(dpy, snapxwin);
  xcb_unmap_window(dpy, snapywin);
  ongrid[0] = true;
  ongrid[1] = true;
}

void
togglefocus () {
  if (notroot(prevfocuswin) && notroot(win) && (win != prevfocuswin) && (win == focuswin) && (prevfocuswin != 0)) {
    setfocus(prevfocuswin);
    raisewin(NULL);
    desktowin(focuswin);
  }
}

void
nextwin () {
  bool iswin = false;
  int focidx = -1;
  int i;
  for (i=0; i<MAXWINS; i++) {
    if (wins[i] == 0) continue;
    if (focidx != -1) {
      iswin = true;
      break;
    }
    if (wins[i] == focuswin) focidx = i;
  }
  if ((focidx > 0) && !iswin && (focidx != -1)) {
    for (i=0; i<focidx; i++) {
      if (wins[i] != 0) {
        iswin = true;
        break;
      }
    }
  }
  if (iswin) {
    desktowin(wins[i]);
    setfocus(wins[i]);
    raisewin(NULL);
  }
}

static void
grid (xcb_query_pointer_reply_t *pointer) {
  /* width */
  values[2] = ((pointer->root_y > screen->height_in_pixels / 3)
        && (pointer->root_y < screen->height_in_pixels / 3 * 2))
      /* middle */
      ? ((pointer->root_x < screen->width_in_pixels / 3)
        /* middle left */
        ? (screen->width_in_pixels * 0.25)
        : ((pointer->root_x > screen->width_in_pixels / 3 * 2)
          /* middle right */
          ? (screen->width_in_pixels * 0.75)
          /* middle middle */
          : (screen->width_in_pixels * 0.5)))
      /* elsewhere */
      : (screen->width_in_pixels / 3);
  /* height */
  values[3] = ((pointer->root_y > screen->height_in_pixels / 3)
        && (pointer->root_y < screen->height_in_pixels / 3 * 2))
      /* middle */
      ? (((pointer->root_x < screen->width_in_pixels / 3)
        || (pointer->root_x > screen->width_in_pixels / 3 * 2))
        /* middle left or right */
        ? screen->height_in_pixels
        /* middle middle */
        : (screen->height_in_pixels * 0.5))
      /* elsewhere */
      : (screen->height_in_pixels * 0.4);
  /* x */
  values[0] = (pointer->root_x < screen->width_in_pixels / 3)
      /* left */
      ? 0
      : ((pointer->root_x < screen->width_in_pixels / 3 * 2)
        /* middle */
        ? (screen->width_in_pixels / 2 - values[2] / 2)
        /* right */
        : (screen->width_in_pixels - values[2]));
  /* y */
  values[1] = (pointer->root_y < screen->height_in_pixels / 3)
      /* top */
      ? 0
      : ((pointer->root_y < screen->height_in_pixels / 3 * 2)
        /* middle */
        ? (screen->height_in_pixels / 2 - values[3] / 2)
        /* bottom */
        : (screen->height_in_pixels - values[3]));
}

void
fullclient () {
  if (notroot(win)) {
    xcb_get_geometry_reply_t *geom;
    geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, win), NULL);
    if (ongrid[0] && ongrid[1]) {
      if ((geom->x == 0) && (geom->y == 0) && (geom->width == screen->width_in_pixels) && (geom->height == screen->height_in_pixels)) {
        /* float client */
        xcb_query_pointer_reply_t *pointer;
        pointer = xcb_query_pointer_reply(dpy, xcb_query_pointer(dpy, root), 0);
        grid(pointer);
      } else {
        /* full client */
        values[0] = 0;
        values[1] = 0;
        values[2] = screen->width_in_pixels;
        values[3] = screen->height_in_pixels;
      }
      xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
      raisewin(NULL);
    } else {
      uint32_t winsnpdifs[MAXWINS][2]; /* win - snapxy: x, y */
      xcb_get_geometry_reply_t *geoms[MAXWINS];
      for (int i=0; i<MAXWINS; i++) {
        if (wins[i] == 0) continue;
        geoms[i] = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, wins[i]), NULL);
        winsnpdifs[i][0] = geoms[i]->x - snapxy[0];
        winsnpdifs[i][1] = geoms[i]->y - snapxy[1];
      }

      snapxy[0] = ((snapxy[0] > screen->width_in_pixels / 2) && (snapxy[0] <= U32POSF))
          ? screen->width_in_pixels
          : ((snapxy[0] < 1 + U32NEG1 - screen->width_in_pixels / 2) && (snapxy[0] > U32POSF))
            ? (1 + U32NEG1 - screen->width_in_pixels)
            : 0;
      snapxy[1] = ((snapxy[1] > screen->height_in_pixels / 2) && (snapxy[1] <= U32POSF))
          ? screen->height_in_pixels
          : ((snapxy[1] < 1 + U32NEG1 - screen->height_in_pixels / 2) && (snapxy[1] > U32POSF))
            ? (1 + U32NEG1 - screen->height_in_pixels)
            : 0;

      for (int i=0; i<MAXWINS; i++) {
        if (wins[i] == 0) continue;
        values[0] = snapxy[0] + winsnpdifs[i][0];
        values[1] = snapxy[1] + winsnpdifs[i][1];
        xcb_configure_window(dpy, wins[i], XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
      }

      values[0] = 0;
      values[1] = 0;
      values[2] = screen->width_in_pixels;
      values[3] = screen->height_in_pixels;
      xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
      raisewin(NULL);

      values[0] = (snapxy[0] > U32POSF)
          ? (snapxy[0] + screen->width_in_pixels - MARKSIZE / 2)
          : (snapxy[0] - MARKSIZE / 2);
      xcb_configure_window(dpy, snapxwin, XCB_CONFIG_WINDOW_X, values);
      values[0] = (snapxy[1] > U32POSF)
          ? (snapxy[1] + screen->height_in_pixels - MARKSIZE / 2)
          : (snapxy[1] - MARKSIZE / 2);
      xcb_configure_window(dpy, snapywin, XCB_CONFIG_WINDOW_Y, values);
      xcb_unmap_window(dpy, snapxwin);
      xcb_unmap_window(dpy, snapywin);
      ongrid[0] = true;
      ongrid[1] = true;
    }
  }
}

static bool
getisdelete() {
  /* WM_DELETE_WINDOW */
  bool isdelete = false;
  xcb_icccm_get_wm_protocols_reply_t reply;
  if (xcb_icccm_get_wm_protocols_reply(dpy,
      xcb_icccm_get_wm_protocols_unchecked(dpy, win, a_protocols), &reply, NULL)) {
    for (int i=0; !isdelete && i<reply.atoms_len; i++) {
      if (reply.atoms[i] == a_delete) {
        isdelete = true;
      }
    }
    xcb_icccm_get_wm_protocols_reply_wipe(&reply);
  }
  return isdelete;
}

void
killclient () {
  if(getisdelete()) {
    xcb_client_message_event_t e;
    e.response_type = XCB_CLIENT_MESSAGE;
    e.window = win;
    e.format = 32;
    e.type = a_protocols;
    e.data.data32[0] = a_delete;
    e.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(dpy, false, win, XCB_EVENT_MASK_NO_EVENT, (const char*)&e);
  } else {
    xcb_kill_client(dpy, win);
  }

  for (int i=0; i<MAXWINS; i++) {
    if (wins[i] == win) {
      wins[i] = 0;
      focuswin = XCB_NONE;
      break;
    }
  }
  if ((prevfocuswin != XCB_NONE) && (prevfocuswin != focuswin)) {
    setfocus(prevfocuswin);
  }
  prevfocuswin = XCB_NONE;
  for (int i=0; i<MAXWINS; i++) {
    if (notroot(wins[i]) && (wins[i] != focuswin)) {
      prevfocuswin = wins[i];
      break;
    }
  }
}

void
quit () {
  isrunning = false;
}

static void
run () {
  uint32_t ptrwindif[2]; /* ptr - win: x, y */
  uint32_t ptrsnpdif[2]; /* ptr - snapxy: x, y */
  uint32_t winsnpdifs[MAXWINS][2]; /* win - snapxy: x, y */
  xcb_generic_event_t *ev;

  while (isrunning) {
    ev = xcb_wait_for_event(dpy);
    xcb_button_press_event_t *e; /* up here to save e->state & MASK beyond button event */

    switch (ev->response_type & 0x7f) {
    
    case XCB_BUTTON_PRESS: {
      e = (xcb_button_press_event_t *)ev;
      win = e->child;

      if (notroot(win) && !(e->state & MOD4)) {
        if (1 == e->detail) {
          values[4] = 1; 
          setfocus(win);

          if (e->state & MOD1) {
            raisewin(NULL);

            xcb_get_geometry_reply_t *geom;
            geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, win), NULL);

            if ((e->state & MOD2) && notroot(win)) {
              /* resize */
              xcb_warp_pointer(dpy, XCB_NONE, win, 0, 0, 0, 0, geom->width, geom->height);
            } else if ((e->state & MOD1) && notroot(win)) {
              /* move freely */
              xcb_query_pointer_reply_t *pointer;
              pointer = xcb_query_pointer_reply(dpy, xcb_query_pointer(dpy, root), 0);
              ptrwindif[0] = pointer->root_x - geom->x;
              ptrwindif[1] = pointer->root_y - geom->y;
            }
            xcb_grab_pointer(dpy, 0, root, XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT, 
                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE, XCB_CURRENT_TIME);
          }
        } else if ((3 == e->detail) && notroot(win)) {
          values[4] = 3;
          lowerwin(NULL);
        }
      } else if ((1 == e->detail) && (e->state & MOD1 | MOD4)) {
          /* move all windows */
          values[4] = 1; 
          xcb_get_geometry_reply_t *geoms[MAXWINS];
          xcb_query_pointer_reply_t *pointer;
          pointer = xcb_query_pointer_reply(dpy, xcb_query_pointer(dpy, root), 0);
          ptrsnpdif[0] = pointer->root_x * MOVESPEED - snapxy[0];
          ptrsnpdif[1] = pointer->root_y * MOVESPEED - snapxy[1];
          for (int i=0; i<MAXWINS; i++) {
            if (wins[i] == 0) continue;
            geoms[i] = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, wins[i]), NULL);
            winsnpdifs[i][0] = geoms[i]->x - snapxy[0];
            winsnpdifs[i][1] = geoms[i]->y - snapxy[1];
          }
          xcb_map_window(dpy, snapxwin);
          xcb_map_window(dpy, snapywin);
          xcb_grab_pointer(dpy, 0, root, XCB_EVENT_MASK_BUTTON_RELEASE |
              XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT, 
              XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE, XCB_CURRENT_TIME);
      }
    } break;

    case XCB_MOTION_NOTIFY: {
      if (values[4] == 1) {
        xcb_query_pointer_reply_t *pointer;
        pointer = xcb_query_pointer_reply(dpy, xcb_query_pointer(dpy, root), 0);
        xcb_get_geometry_reply_t *geom;
        geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, win), NULL);
        if ((e->state & MOD2) && notroot(win)) {
          /* resize */
          values[0] = pointer->root_x - geom->x;
          values[1] = pointer->root_y - geom->y;
          xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
        } else if (e->state & MOD4) {
          /* move all windows */
          values[5] = pointer->root_x * MOVESPEED - ptrsnpdif[0];
          values[6] = pointer->root_y * MOVESPEED - ptrsnpdif[1];

          snapxy[0] = (values[5] > U32POSF) /* negative */
              ? ((values[5] < U32NEG1 - screen->width_in_pixels + SNAPMARGIN * MOVESPEED)
                /* right */
                ? (1 + U32NEG1 - screen->width_in_pixels)
                : ((values[5] > U32NEG1 - SNAPMARGIN * MOVESPEED)
                  /* middle */
                  ? 0
                  : values[5]))
              : ((values[5] <= SNAPMARGIN * MOVESPEED)
                /* middle */
                ? 0
                : (values[5] >= screen->width_in_pixels - SNAPMARGIN * MOVESPEED)
                  /* left */
                  ? screen->width_in_pixels
                  : values[5]);
          snapxy[1] = (values[6] > U32POSF) /* negative */
              ? ((values[6] < U32NEG1 - screen->height_in_pixels + SNAPMARGIN * MOVESPEED)
                /* right */
                ? (1 + U32NEG1 - screen->height_in_pixels)
                : ((values[6] > U32NEG1 - SNAPMARGIN * MOVESPEED)
                  /* middle */
                  ? 0
                  : values[6]))
              : ((values[6] <= SNAPMARGIN * MOVESPEED)
                /* middle */
                ? 0
                : (values[6] >= screen->height_in_pixels - SNAPMARGIN * MOVESPEED)
                  /* left */
                  ? screen->height_in_pixels
                  : values[6]);
          for (int i=0; i<MAXWINS; i++) {
            if (wins[i] == 0) continue;
            values[0] = snapxy[0] + winsnpdifs[i][0];
            values[1] = snapxy[1] + winsnpdifs[i][1];
            xcb_configure_window(dpy, wins[i], XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
          }

          values[0] = (snapxy[0] > U32POSF)
              ? (snapxy[0] + screen->width_in_pixels - MARKSIZE / 2)
              : (snapxy[0] - MARKSIZE / 2);
          xcb_configure_window(dpy, snapxwin, XCB_CONFIG_WINDOW_X, values);
          values[0] = (snapxy[1] > U32POSF)
              ? (snapxy[1] + screen->height_in_pixels - MARKSIZE / 2)
              : (snapxy[1] - MARKSIZE / 2);
          xcb_configure_window(dpy, snapywin, XCB_CONFIG_WINDOW_Y, values);

          if ((snapxy[0] == 0) || (snapxy[0] == screen->width_in_pixels) || (snapxy[0] == 1+U32NEG1-screen->width_in_pixels)) {
            xcb_unmap_window(dpy, snapxwin);
            ongrid[0] = true;
          } else {
            xcb_map_window(dpy, snapxwin);
            ongrid[0] = false;
          }
          if ((snapxy[1] == 0) || (snapxy[1] == screen->height_in_pixels) || (snapxy[1] == 1+U32NEG1-screen->height_in_pixels)) {
            xcb_unmap_window(dpy, snapywin);
            ongrid[1] = true;
          } else {
            xcb_map_window(dpy, snapywin);
            ongrid[1] = false;
          }
        } else if ((e->state & MOD3) && notroot(win)) {
          /* move freely */
          values[5] = pointer->root_x - ptrwindif[0];
          values[6] = pointer->root_y - ptrwindif[1];
          if (ongrid[0] && ongrid[1]) {
            values[2] = values[5] + geom->width;
            values[3] = values[6] + geom->height;

            values[0] = ((u32abs(screen->width_in_pixels, values[2])
                    < ((values[5] > U32POSF) ? 1 + U32NEG1 - values[5] : values[5])))
                ? (((values[2] >= screen->width_in_pixels - SNAPMARGIN)
                    && (values[2] <= screen->width_in_pixels + SNAPMARGIN))
                  /* right */
                  ? (screen->width_in_pixels - geom->width)
                  : values[5])
                : (((U32NEG1 - values[5] < SNAPMARGIN)
                    || (values[5] <= SNAPMARGIN))
                  /* left */
                  ? 0
                  : values[5]);
            values[1] = ((u32abs(screen->height_in_pixels, values[3])
                    < ((values[6] > U32POSF) ? 1 + U32NEG1 - values[6] : values[6])))
                ? (((values[3] >= screen->height_in_pixels - SNAPMARGIN)
                    && (values[3] <= screen->height_in_pixels + SNAPMARGIN))
                  /* bottom */
                  ? (screen->height_in_pixels - geom->height)
                  : values[6])
                : (((U32NEG1 - values[6] < SNAPMARGIN)
                    || (values[6] <= SNAPMARGIN))
                  /* top */
                  ? 0
                  : values[6]);
          } else {
            values[0] = values[5];
            values[1] = values[6];
          }
          xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
        } else if ((e->state & MOD1) && notroot(win)) {
          /* move on grid */
          if (ongrid[0] && ongrid[1]) {
            grid(pointer);
            xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
          } else if (ongrid[0]) {
            xcb_get_geometry_reply_t *geomsnapy;
            geomsnapy = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, snapywin), NULL);
            uint32_t gsy = geomsnapy->y + MARKSIZE / 2;

            values[5] = pointer->root_x - ptrwindif[0];
            values[6] = pointer->root_y - ptrwindif[1];
            values[2] = values[5] + geom->width;
            values[3] = values[6] + geom->height;

            values[0] = ((u32abs(screen->width_in_pixels, values[2])
                    < ((values[5] > U32POSF) ? 1 + U32NEG1 - values[5] : values[5])))
                ? (((values[2] >= screen->width_in_pixels - SNAPMARGIN)
                    && (values[2] <= screen->width_in_pixels + SNAPMARGIN))
                  /* right */
                  ? (screen->width_in_pixels - geom->width)
                  : values[5])
                : (((U32NEG1 - values[5] < SNAPMARGIN)
                    || (values[5] <= SNAPMARGIN))
                  /* left */
                  ? 0
                  : values[5]);
            values[1] = ((values[6] >= gsy - SNAPMARGIN) && (values[6] <= gsy + SNAPMARGIN))
                ? gsy
                : ((values[3] >= gsy - SNAPMARGIN) && (values[3] <= gsy + SNAPMARGIN))
                  ? (gsy - geom->height)
                  : values[6];
            xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
          } else if (ongrid[1]) {
            xcb_get_geometry_reply_t *geomsnapx;
            geomsnapx = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, snapxwin), NULL);
            uint32_t gsx = geomsnapx->x + MARKSIZE / 2;

            values[5] = pointer->root_x - ptrwindif[0];
            values[6] = pointer->root_y - ptrwindif[1];
            values[2] = values[5] + geom->width;
            values[3] = values[6] + geom->height;

            values[0] = ((values[5] >= gsx - SNAPMARGIN) && (values[5] <= gsx + SNAPMARGIN))
                ? gsx
                : ((values[2] >= gsx - SNAPMARGIN) && (values[2] <= gsx + SNAPMARGIN))
                  ? (gsx - geom->width)
                  : values[5];
            values[1] = ((u32abs(screen->height_in_pixels, values[3])
                    < ((values[6] > U32POSF) ? 1 + U32NEG1 - values[6] : values[6])))
                ? (((values[3] >= screen->height_in_pixels - SNAPMARGIN)
                    && (values[3] <= screen->height_in_pixels + SNAPMARGIN))
                  /* bottom */
                  ? (screen->height_in_pixels - geom->height)
                  : values[6])
                : (((U32NEG1 - values[6] < SNAPMARGIN)
                    || (values[6] <= SNAPMARGIN))
                  /* top */
                  ? 0
                  : values[6]);
            xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
          } else {
            xcb_get_geometry_reply_t *geomsnapxy[2];
            geomsnapxy[0] = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, snapxwin), NULL);
            geomsnapxy[1] = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, snapywin), NULL);
            uint32_t gsxy[2] = {geomsnapxy[0]->x + MARKSIZE / 2, geomsnapxy[1]->y + MARKSIZE / 2};

            values[5] = pointer->root_x - ptrwindif[0];
            values[6] = pointer->root_y - ptrwindif[1];
            values[2] = values[5] + geom->width;
            values[3] = values[6] + geom->height;

            values[0] = ((values[5] >= gsxy[0] - SNAPMARGIN) && (values[5] <= gsxy[0] + SNAPMARGIN))
                ? gsxy[0]
                : ((values[2] >= gsxy[0] - SNAPMARGIN) && (values[2] <= gsxy[0] + SNAPMARGIN))
                  ? (gsxy[0] - geom->width)
                  : values[5];
            values[1] = ((values[6] >= gsxy[1] - SNAPMARGIN) && (values[6] <= gsxy[1] + SNAPMARGIN))
                ? gsxy[1]
                : ((values[3] >= gsxy[1] - SNAPMARGIN) && (values[3] <= gsxy[1] + SNAPMARGIN))
                  ? (gsxy[1] - geom->height)
                  : values[6];
            xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
          }
        }
      }
    } break;

    case XCB_BUTTON_RELEASE: {
      values[4] = 0; 
      xcb_ungrab_pointer(dpy, XCB_CURRENT_TIME);
    } break;

    case XCB_KEY_PRESS: {
      xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
      win = focuswin;
      for (int i=0; i<KEYSIZE; ++i) {
        if ((keys[i].mod == e->state) && (keys[i].keycode == e->detail) && keys[i].func)
          keys[i].func(&(keys[i].arg));
      }
    } break;

    case XCB_CREATE_NOTIFY: {
      xcb_create_notify_event_t *e = (xcb_create_notify_event_t *)ev;
      win = e->window;
      values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
      xcb_change_window_attributes(dpy, win, XCB_CW_EVENT_MASK, values);
    } break;

    case XCB_DESTROY_NOTIFY: {
      xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
      xcb_window_t deadwin = e->window;
      for (int i=0; i<MAXWINS; i++) {
        if (wins[i] == deadwin) {
          wins[i] = 0;
          focuswin = XCB_NONE;
          break;
        }
      }
      if ((prevfocuswin != XCB_NONE) && (prevfocuswin != focuswin)) {
        setfocus(prevfocuswin);
      }
      prevfocuswin = XCB_NONE;
      for (int i=0; i<MAXWINS; i++) {
        if (notroot(wins[i]) && (wins[i] != focuswin)) {
          prevfocuswin = wins[i];
          break;
        }
      }
      if ((deadwin == focuswin) && (values[4] != 0)) {
        values[4] = 0; 
        xcb_ungrab_pointer(dpy, XCB_CURRENT_TIME);
      }
    } break;

    case XCB_UNMAP_NOTIFY: {
      xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
      xcb_window_t deadwin = e->window;
      for (int i=0; i<MAXWINS; i++) {
        if (wins[i] == deadwin) {
          wins[i] = 0;
          focuswin = XCB_NONE;
          break;
        }
      }
      if ((prevfocuswin != XCB_NONE) && (prevfocuswin != focuswin)) {
        setfocus(prevfocuswin);
      }
      prevfocuswin = XCB_NONE;
      for (int i=0; i<MAXWINS; i++) {
        if (notroot(wins[i]) && (wins[i] != focuswin)) {
          prevfocuswin = wins[i];
          break;
        }
      }
      if ((deadwin == focuswin) && (values[4] != 0)) {
        values[4] = 0; 
        xcb_ungrab_pointer(dpy, XCB_CURRENT_TIME);
      }
    } break;

    case XCB_MAP_REQUEST: {
      xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
      win = e->window;

      if (ongrid[0] && ongrid[1]) {
        values[0] = 0;
        values[1] = 0;
        values[2] = screen->width_in_pixels;
        values[3] = screen->height_in_pixels;
      } else {
        values[2] = screen->width_in_pixels / 2;
        values[3] = screen->height_in_pixels / 2;
        values[0] = screen->width_in_pixels / 2 - values[2] / 2;
        values[1] = screen->height_in_pixels / 2 - values[3] / 2;
      }

      xcb_size_hints_t hints;
      if (xcb_icccm_get_wm_normal_hints_reply(dpy, xcb_icccm_get_wm_normal_hints(dpy, win), &hints, NULL) == 1) {
        if ((hints.flags & (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE))
            && hints.min_width == hints.max_width && hints.min_height == hints.max_height) {
          values[2] = hints.max_width;
          values[3] = hints.max_height;
          values[0] = screen->width_in_pixels / 2 - values[2] / 2;
          values[1] = screen->height_in_pixels / 2 - values[3] / 2;
        }
      }

      xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
      xcb_map_window(dpy, win);
      /*setfocus(win);*/

      /* no more than MAXWINS windows please */
      if (notroot(win)) {
        int i;
        for (i=0; i<MAXWINS; i++) {
          if (wins[i] == win) {
            /* exists */
            break;
          } else if (wins[i] == 0) {
            /* added */
            wins[i] = win;
            break;
          }
        }
        if (i == MAXWINS) {
          /* too many */
          killclient(NULL);
        }
      }
    } break;

    case XCB_MAP_NOTIFY: {
      xcb_map_notify_event_t *e = (xcb_map_notify_event_t *)ev;
      win = e->window;
      xcb_window_t trans = XCB_NONE;
      xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_transient_for(dpy, win);
      xcb_get_property_reply_t* reply = xcb_get_property_reply(dpy, cookie, NULL);
      xcb_icccm_get_wm_transient_for_from_reply(&trans, reply);
      if(trans != XCB_NONE)
        setfocus(trans);
    } break;

    case XCB_ENTER_NOTIFY: {
      xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;
      win = e->event;
      if (notroot(win) && (e->detail != XCB_NOTIFY_DETAIL_INFERIOR)) {
        xcb_window_t trans = XCB_NONE;
        xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_transient_for(dpy, win);
        xcb_get_property_reply_t* reply = xcb_get_property_reply(dpy, cookie, NULL);
        xcb_icccm_get_wm_transient_for_from_reply(&trans, reply);
        if (trans == XCB_NONE) {
          setfocus(win);
        } else if (focuswin != trans) {
          setfocus(trans);
        }
      }
    } break;

    }
    xcb_flush(dpy);
  }
}

static
xcb_atom_t setatom (const char* name) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(dpy, 0, strlen(name), name);
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(dpy, cookie, NULL);
  xcb_atom_t atom = reply->atom;
  free(reply);
  return atom;
}


/* MAIN */
int
main () {
  dpy = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(dpy)) return 1;

  screen = xcb_setup_roots_iterator(xcb_get_setup(dpy)).data;
  root = screen->root;

  a_protocols = setatom("WM_PROTOCOLS");
  a_delete = setatom("WM_DELETE_WINDOW");

  xcb_grab_button(dpy, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | 
      XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
      XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 1, MOD1);
  xcb_grab_button(dpy, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | 
      XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
      XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 1, MOD1 | MOD2);
  xcb_grab_button(dpy, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | 
      XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
      XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 1, MOD1 | MOD3);
  xcb_grab_button(dpy, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | 
      XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
      XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 1, MOD1 | MOD4);
  xcb_grab_button(dpy, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | 
      XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
      XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 3, MOD1);

  values[0] = COLOR;
  values[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
  xcb_change_window_attributes(dpy, root, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);

  for (int i=0; i<KEYSIZE; ++i) {
    xcb_grab_key(dpy, 1, root, keys[i].mod, keys[i].keycode,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
  }

  snapxwin = xcb_generate_id(dpy);
  snapywin = xcb_generate_id(dpy);
  values[0] = MARKCOLOR;
  xcb_create_window(dpy, screen->root_depth, snapxwin, root, 0, 0,
      screen->width_in_pixels, screen->height_in_pixels, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
      screen->root_visual, XCB_CW_BACK_PIXEL, values);
  xcb_map_window(dpy, snapxwin);
  xcb_flush(dpy);
  values[0] = MARKSIZE;
  xcb_configure_window(dpy, snapxwin, XCB_CONFIG_WINDOW_WIDTH, values);
  xcb_unmap_window(dpy, snapxwin);

  xcb_create_window(dpy, screen->root_depth, snapywin, root, 0, 0,
      screen->width_in_pixels, MARKSIZE, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
      screen->root_visual, XCB_CW_BACK_PIXEL, values);
  xcb_unmap_window(dpy, snapywin);

  xcb_flush(dpy);

  run();
  xcb_set_input_focus(dpy, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
  xcb_flush(dpy);
  xcb_disconnect(dpy);

  return 0;
}
