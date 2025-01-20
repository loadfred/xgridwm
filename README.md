# XGridWM
XCB Grid Window Manager for Xorg on Linux

### Description
Minimal window manager, less ram than dwm, simple, but could be smaller, uses XCB instead of Xlib, window focus still needs fixing probably because "_NET_ACTIVE_WINDOW" isn't used, but basically everything works

This was just a testing WM to learn XCB, but I will create a new WM now that I know most of the window quirks, which will have easier to read code (this code is ugly), less lines, and more usability, but this is still usable, I like this XGridWM desktop layout, but it could be better

### Features
- grid like virtual desktops
- move entire desktop w/ meta + alt + pointer, like workspaces but on the same screen
- resize w/ meta + shift + pointer
- all windows fullscreen on open (unless off grid)
- change window focus on hover and whatever else, check `config.def.h`

### Build / Install
install dependencies
- xcb-devel
- xcb-util-wm-devel (icccm)

clone this repo then make
```
git clone https://github.com/loadfred/xgridwm
cd xgridwm
make
```
edit `config.h`, set keycodes to different values if you have to, change cmd shortcuts if needed, mod masks, bg color
```
sudo make install
```
it builds just like dwm

you can start using `xgridwm-session` if you have pipewire + dbus (w/ login manager if you want)

or just start like any other wm using `xgridwm` in your .Xinitrc w/ startx

### Uninstall
```
sudo make uninstall
```
