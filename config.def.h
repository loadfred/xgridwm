/* WINDOW MANAGER CONFIG */
#define MAXWINS 22 /* maximum windows allowed open */
#define COLOR 0x00ffa200 /* bg color */
#define MARKSIZE 2 /* line marking when moving desktop */
/*#define MARKCOLOR COLOR | 0x003f3f3f  | -> brighten */
#define MARKCOLOR 0x002c1b00
#define MOVESPEED 3.3 /* move desktop speed */
#define SNAPMARGIN 66 /* edge snap leeway */


/* MOD MASKS AND KEYCODES */
#define MOD1 XCB_MOD_MASK_4 /* move on grid = MOD1 + pointer */
#define MOD2 XCB_MOD_MASK_SHIFT /* resize = MOD1|MOD2 + pointer */
#define MOD3 XCB_MOD_MASK_CONTROL /* move freely = MOD1|MOD3 + pointer */
#define MOD4 XCB_MOD_MASK_1 /* move all windows = MOD1|MOD4 + pointer */

/* you can find your keycodes easily with xev */
#define KEY_MUTEVOL 121
#define KEY_DECVOL  122
#define KEY_INCVOL  123
#define KEY_DECBRIGHT 232
#define KEY_INCBRIGHT 233
#define KEY_ENTER 36
#define KEY_TAB 23
#define KEY_COLON 47
#define KEY_B 56
#define KEY_E 26
#define KEY_F 41
#define KEY_K 45
#define KEY_L 46
#define KEY_M 58
#define KEY_P 33
#define KEY_Q 24


/* COMMANDS */
static const char *termcmd[] = { "st", NULL };
static const char *menucmd[] = { "dmenu_run", NULL };
static const char *bwsrcmd[] = { "firefox", NULL };
static const char *filecmd[] = { "thunar", NULL };

static const char *incvol[]  = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "0.01+",  "-l", "0.6", NULL };
static const char *inclvol[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "0.001+", "-l", "0.6", NULL };
static const char *decvol[]  = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "0.01-",  NULL };
static const char *declvol[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "0.001-", NULL };
static const char *mutevol[] = { "wpctl", "set-mute",   "@DEFAULT_AUDIO_SINK@", "toggle", NULL };

static const char *incbright[]  = { "brightnessctl", "s", "5%+",  NULL };
static const char *inclbright[] = { "brightnessctl", "s", "1%+",  NULL };
static const char *decbright[]  = { "brightnessctl", "s", "5%-",  "-n1", NULL };
static const char *declbright[] = { "brightnessctl", "s", "1%-",  "-n1", NULL };
static const char *maxbright[]  = { "brightnessctl", "s", "100%", NULL };
static const char *minbright[]  = { "brightnessctl", "s", "1",    NULL };


/* KEYBINDS */
static const Key keys[] = {
  /* mod masks, key, function, command  */
  { 0,    KEY_INCVOL,  spawn, {.v= incvol} },
  { MOD1, KEY_INCVOL,  spawn, {.v= inclvol} },
  { 0,    KEY_DECVOL,  spawn, {.v= decvol} },
  { MOD1, KEY_DECVOL,  spawn, {.v= declvol} },
  { 0,    KEY_MUTEVOL, spawn, {.v= mutevol} },

  { 0,         KEY_INCBRIGHT, spawn, {.v= incbright} },
  { MOD1,      KEY_INCBRIGHT, spawn, {.v= inclbright} },
  { 0,         KEY_DECBRIGHT, spawn, {.v= decbright} },
  { MOD1,      KEY_DECBRIGHT, spawn, {.v= declbright} },
  { MOD1|MOD2, KEY_INCBRIGHT, spawn, {.v= maxbright} },
  { MOD1|MOD2, KEY_DECBRIGHT, spawn, {.v= minbright} },

  { MOD1,      KEY_ENTER, spawn, {.v= termcmd} },
  { MOD1,      KEY_P,     spawn, {.v= menucmd} },
  { MOD1|MOD2, KEY_B,     spawn, {.v= bwsrcmd} },
  { MOD1,      KEY_E,     spawn, {.v= filecmd} },

  { MOD1,      KEY_TAB,   togglefocus, NULL },
  { MOD1,      KEY_K,     nextwin,     NULL },
  { MOD1,      KEY_L,     lowerwin,    NULL },
  { MOD1,      KEY_COLON, raisewin,    NULL },
  { MOD1,      KEY_F,     fullclient,  NULL },
  { MOD1,      KEY_Q,     killclient,  NULL },
  { MOD1|MOD2, KEY_Q,     quit,        NULL },
};
