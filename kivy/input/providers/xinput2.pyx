"""
Xinput2 input provider
=========================
"""

cdef extern from "xinput2_core.c":
    ctypedef struct TouchStruct:
        int id, state
        float x, y

cdef extern int start()
cdef extern int idle()

# Dummy class to bridge between the MotionEventProvider class and C
class InputX11():

    def __init__(self):
        self.on_touch = None

    def start(self, on_touch):
        self.on_touch = on_touch
        start()

    def x11_idle(self):
        idle()

xinput = InputX11()

# Create callback for touch events from C->Cython
ctypedef int (*touch_cb_type)(TouchStruct *touch)
cdef extern void x11_set_touch_callback(touch_cb_type callback)
cdef int event_callback(TouchStruct *touch):
    py_touch = {"id": touch.id,
                "state": touch.state,
                "x": touch.x,
                "y": touch.y}
    xinput.on_touch(py_touch)
x11_set_touch_callback(event_callback)


#######
# Input provider
__all__ = ('Xinput2EventProvider', 'Xinput2Event')

from kivy.logger import Logger
from collections import deque
from kivy.input.provider import MotionEventProvider
from kivy.input.factory import MotionEventFactory
from kivy.input.motionevent import MotionEvent


class Xinput2Event(MotionEvent):

    def depack(self, args):
        super(Xinput2Event, self).depack(args)
        if args[0] is None:
            return
        self.profile = ('pos',)
        self.sx = args[0]
        self.sy = args[1]
        self.is_touch = True


class Xinput2EventProvider(MotionEventProvider):

    __handlers__ = {}

    def start(self):
        xinput.start(on_touch=self.on_touch)
        self.touch_queue = deque()
        self.touches = {}

    def update(self, dispatch_fn):
        xinput.x11_idle()
        try:
            while True:
                event = self.touch_queue.popleft()
                dispatch_fn(*event)
        except IndexError:
            pass

    def on_touch(self, raw_touch):
        touches = self.touches
        args = [raw_touch["x"], raw_touch["y"]]

        if raw_touch["id"] not in self.touches:
            touch = Xinput2Event(self.device, raw_touch["id"], args)
            touches[raw_touch["id"]] = touch
        else:
            touch = touches[raw_touch["id"]]
            touch.move(args)

        if raw_touch["state"] == 0:
            event = ('begin', touch)
        elif raw_touch["state"] == 1:
            event = ('update', touch)
        elif raw_touch["state"] == 2:
            event = ('end', touch)
        self.touch_queue.append(event)


# registers
MotionEventFactory.register('xinput2', Xinput2EventProvider)
