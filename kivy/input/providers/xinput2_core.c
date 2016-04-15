/*
 * event.c - show touch events
 *
 * Copyright © 2013 Eon S. Jeon <esjeon@live.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#define TOUCH_DOWN 0
#define TOUCH_MOVE 1
#define TOUCH_UP 2

static Display *dpy;
static int xi_opcode;

// Cython API
typedef struct TouchStruct{
    int id;
    int state;
	float  x;
	float  y;
} TouchStruct;

typedef int (*touch_cb_type)(TouchStruct *touch);
touch_cb_type touch_cb = NULL;

void x11_set_touch_callback(touch_cb_type callback) {
	touch_cb = callback;
}


int start (){
	dpy = XOpenDisplay(NULL);
	int scr = DefaultScreen(dpy);

	//int xi_opcode;
	int devid;
	Window win;

	/* check XInput extension */
	{
		int ev;
		int err;

		if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &ev, &err)) {
			printf("X Input extension not available.\n");
			return 1;
		}
	}

	/* check the version of XInput */
	{
		int rc;
		int major = 2;
		int minor = 3;

		rc = XIQueryVersion(dpy, &major, &minor);
		if (rc != Success)
		{
			printf("No XI2 support. (%d.%d only)\n", major, minor);
			exit(1);
		}
	}

	/* create window */
	{
		XSetWindowAttributes attr = {
			.background_pixel = 0,
			.event_mask = KeyPressMask
		};

		win = XCreateWindow(dpy, RootWindow(dpy, scr),
				0, 0, 500, 500, 0,
				DefaultDepth(dpy, scr),
				InputOutput,
				DefaultVisual(dpy, scr),
				CWEventMask | CWBackPixel,
				&attr);

		XMapWindow(dpy, win);
		XSync(dpy, False);
	}

	/* select device */
	{
		XIDeviceInfo *di;
		XIDeviceInfo *dev;
		XITouchClassInfo *class;
		int cnt;
		int i, j;

		di = XIQueryDevice(dpy, XIAllDevices, &cnt);
		for (i = 0; i < cnt; i ++)
		{
			dev = &di[i];
			for (j = 0; j < dev->num_classes; j ++)
			{
				class = (XITouchClassInfo*)(dev->classes[j]);
				if (class->type != XITouchClass)
				{
					devid = dev->deviceid;
					goto STOP_SEARCH_DEVICE;
				}
			}
		}
STOP_SEARCH_DEVICE:
		XIFreeDeviceInfo(di);
	}

	/* select events to listen */
	{
		XIEventMask mask = {
			.deviceid = devid, //XIAllDevices,
			.mask_len = XIMaskLen(XI_TouchEnd)
		};
		mask.mask = (unsigned char*)calloc(3, sizeof(char));
		XISetMask(mask.mask, XI_TouchBegin);
		XISetMask(mask.mask, XI_TouchUpdate);
		XISetMask(mask.mask, XI_TouchEnd);
        XISetMask(mask.mask, XI_Motion); // Only for testing without overlay

		XISelectEvents(dpy, win, &mask, 1);

		free(mask.mask);
	}


	XFlush(dpy);

    printf("INIT finished.\n");

	return 0;
}

int idle(){
    TouchStruct touch;
	XEvent ev;
	XGenericEventCookie *cookie = &ev.xcookie; // hacks!
	XIDeviceEvent *devev;

	while (XPending(dpy)){
		XNextEvent(dpy, &ev);
		if (XGetEventData(dpy, cookie)) // extended event
		{
			// check if this belongs to XInput
			if(cookie->type == GenericEvent && cookie->extension == xi_opcode)
			{
				static int last = -1;

				devev = cookie->data;
				switch(devev->evtype) {
				case XI_TouchBegin:
                        touch.id = devev->detail;
                        touch.state = TOUCH_DOWN;
                        touch.x = devev->event_x;
                        touch.y = devev->event_y;
                        touch_cb(&touch);
					break;
				case XI_TouchUpdate:
                        touch.id = devev->detail;
                        touch.state = TOUCH_MOVE;
                        touch.x = devev->event_x;
                        touch.y = devev->event_y;
                        touch_cb(&touch);
					break;
				case XI_TouchEnd:
                        touch.id = devev->detail;
                        touch.state = TOUCH_UP;
                        touch.x = devev->event_x;
                        touch.y = devev->event_y;
                        touch_cb(&touch);
					break;
				case XI_Motion: // Just for testing
                        touch.id = devev->detail;
                        touch.state = TOUCH_DOWN;
                        touch.x = devev->event_x;
                        touch.y = devev->event_y;
                        touch_cb(&touch);
					break;
				}
			}
		}
		else // normal event
		{
			if (ev.type == KeyPress)
				break;
		}
	}
	return 0;
}

int main (){
    start();
    while(1){
    	idle();
    }
    return 0;
}