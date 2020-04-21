#pragma once

#include <X11/Xlib.h>

void create_x11_window(int is_server, Display **x11_display, Window *x11_window)
{
    // Open X11 display and create window
    Display *display = XOpenDisplay(NULL);
    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 640, 480, 1,
                                        BlackPixel(display, screen), WhitePixel(display, screen));
    XStoreName(display, window, is_server ? "Server" : "Client");
    XMapWindow(display, window);

    // Return
    *x11_display = display;
    *x11_window = window;
}