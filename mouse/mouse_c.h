#include "mouse.h"
#include "../base/deadbeef_rand.h"
#include "../base/microsleep.h"

#include <math.h> /* For floor() */
#if defined(IS_MACOSX)
	// #include </System/Library/Frameworks/ApplicationServices.framework/Headers/ApplicationServices.h>
	#include <ApplicationServices/ApplicationServices.h>
	// #include </System/Library/Frameworks/ApplicationServices.framework/Versions/A/Headers/ApplicationServices.h>
#elif defined(USE_X11)
	#include <X11/Xlib.h>
	#include <X11/extensions/XTest.h>
	#include <stdlib.h>
#endif

/* Some convenience macros for converting our enums to the system API types. */
#if defined(IS_MACOSX)
	CGEventType MMMouseDownToCGEventType(MMMouseButton button) {
		if (button == LEFT_BUTTON) {
			return kCGEventLeftMouseDown;
		}
		if (button == RIGHT_BUTTON) { 
			return kCGEventRightMouseDown;
		}
		return kCGEventOtherMouseDown;
	}

	CGEventType MMMouseUpToCGEventType(MMMouseButton button) {
		if (button == LEFT_BUTTON) { return kCGEventLeftMouseUp; }
		if (button == RIGHT_BUTTON) { return kCGEventRightMouseUp; }		
		return kCGEventOtherMouseUp;
	}

	CGEventType MMMouseDragToCGEventType(MMMouseButton button) {
		if (button == LEFT_BUTTON) { return kCGEventLeftMouseDragged; }
		if (button == RIGHT_BUTTON) { return kCGEventRightMouseDragged; }
		return kCGEventOtherMouseDragged;
	}

	CGEventType MMMouseToCGEventType(bool down, MMMouseButton button) {
		if (down) { return MMMouseDownToCGEventType(button); }
		return MMMouseUpToCGEventType(button);
	}

#elif defined(IS_WINDOWS)
 
	DWORD MMMouseUpToMEventF(MMMouseButton button) {
		if (button == LEFT_BUTTON) { return MOUSEEVENTF_LEFTUP; }
		if (button == RIGHT_BUTTON) { return MOUSEEVENTF_RIGHTUP; } 
		return MOUSEEVENTF_MIDDLEUP;
	}

	DWORD MMMouseDownToMEventF(MMMouseButton button) {
		if (button == LEFT_BUTTON) { return MOUSEEVENTF_LEFTDOWN; }
		if (button == RIGHT_BUTTON) { return MOUSEEVENTF_RIGHTDOWN; } 
		return MOUSEEVENTF_MIDDLEDOWN;
	}

	DWORD MMMouseToMEventF(bool down, MMMouseButton button) {
		if (down) { return MMMouseDownToMEventF(button); }
		return MMMouseUpToMEventF(button);
	}
#endif

#if defined(IS_MACOSX)
	/* Calculate the delta for a mouse move and add them to the event. */
	void calculateDeltas(CGEventRef *event, MMPointInt32 point) {
		/* The next few lines are a workaround for games not detecting mouse moves. */
		CGEventRef get = CGEventCreate(NULL);
		CGPoint mouse = CGEventGetLocation(get);

		// Calculate the deltas.
		int64_t deltaX = point.x - mouse.x;
		int64_t deltaY = point.y - mouse.y;

		CGEventSetIntegerValueField(*event, kCGMouseEventDeltaX, deltaX);
		CGEventSetIntegerValueField(*event, kCGMouseEventDeltaY, deltaY);

		CFRelease(get);
	}
#endif

/* Move the mouse to a specific point. */
void moveMouse(MMPointInt32 point){
	#if defined(IS_MACOSX)
		CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
		CGEventRef move = CGEventCreateMouseEvent(source, kCGEventMouseMoved, 
								CGPointFromMMPointInt32(point), kCGMouseButtonLeft);

		calculateDeltas(&move, point);

		CGEventPost(kCGHIDEventTap, move);
		CFRelease(move);
		CFRelease(source);
	#elif defined(USE_X11)
		Display *display = XGetMainDisplay();
		XWarpPointer(display, None, DefaultRootWindow(display), 0, 0, 0, 0, point.x, point.y);

		XSync(display, false);
	#elif defined(IS_WINDOWS)
		SetCursorPos(point.x, point.y);
	#endif
}

void dragMouse(MMPointInt32 point, const MMMouseButton button){
	#if defined(IS_MACOSX)
		const CGEventType dragType = MMMouseDragToCGEventType(button);
		CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
		CGEventRef drag = CGEventCreateMouseEvent(source, dragType, 
								CGPointFromMMPointInt32(point), (CGMouseButton)button);

		calculateDeltas(&drag, point);

		CGEventPost(kCGHIDEventTap, drag);
		CFRelease(drag);
		CFRelease(source);
	#else
		moveMouse(point);
	#endif
}

MMPointInt32 location() {
	#if defined(IS_MACOSX)
		CGEventRef event = CGEventCreate(NULL);
		CGPoint point = CGEventGetLocation(event);
		CFRelease(event);

		return MMPointInt32FromCGPoint(point);
	#elif defined(USE_X11)
		int x, y; 	/* This is all we care about. Seriously. */
		Window garb1, garb2; 	/* Why you can't specify NULL as a parameter */
		int garb_x, garb_y;  	/* is beyond me. */
		unsigned int more_garbage;

		Display *display = XGetMainDisplay();
		XQueryPointer(display, XDefaultRootWindow(display), &garb1, &garb2, &x, &y, 
						&garb_x, &garb_y, &more_garbage);

		return MMPointInt32Make(x, y);
	#elif defined(IS_WINDOWS)
		POINT point;
		GetCursorPos(&point);
		return MMPointInt32FromPOINT(point);
	#endif
}

/* Press down a button, or release it. */
void toggleMouse(bool down, MMMouseButton button) {
	#if defined(IS_MACOSX)
		const CGPoint currentPos = CGPointFromMMPointInt32(location());
		const CGEventType mouseType = MMMouseToCGEventType(down, button);
		CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
		CGEventRef event = CGEventCreateMouseEvent(source, mouseType, currentPos, (CGMouseButton)button);

		CGEventPost(kCGHIDEventTap, event);
		CFRelease(event);
		CFRelease(source);
	#elif defined(USE_X11)
		Display *display = XGetMainDisplay();
		XTestFakeButtonEvent(display, button, down ? True : False, CurrentTime);
		XSync(display, false);
	#elif defined(IS_WINDOWS)
		// mouse_event(MMMouseToMEventF(down, button), 0, 0, 0, 0);
		INPUT mouseInput;

		mouseInput.type = INPUT_MOUSE;
		mouseInput.mi.dx = 0;
		mouseInput.mi.dy = 0;
		mouseInput.mi.dwFlags = MMMouseToMEventF(down, button);
		mouseInput.mi.time = 0;
		mouseInput.mi.dwExtraInfo = 0;
		mouseInput.mi.mouseData = 0;
		SendInput(1, &mouseInput, sizeof(mouseInput));
	#endif
}

void clickMouse(MMMouseButton button){
	toggleMouse(true, button);
	microsleep(5.0);
	toggleMouse(false, button);
}

/* Special function for sending double clicks, needed for MacOS. */
void doubleClick(MMMouseButton button){
	#if defined(IS_MACOSX)
		/* Double click for Mac. */
		const CGPoint currentPos = CGPointFromMMPointInt32(location());
		const CGEventType mouseTypeDown = MMMouseToCGEventType(true, button);
		const CGEventType mouseTypeUP = MMMouseToCGEventType(false, button);

		CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
		CGEventRef event = CGEventCreateMouseEvent(source, mouseTypeDown, currentPos, kCGMouseButtonLeft);

		/* Set event to double click. */
		CGEventSetIntegerValueField(event, kCGMouseEventClickState, 2);
		CGEventPost(kCGHIDEventTap, event);

		CGEventSetType(event, mouseTypeUP);
		CGEventPost(kCGHIDEventTap, event);

		CFRelease(event);
		CFRelease(source);
	#else
		/* Double click for everything else. */
		clickMouse(button);
		microsleep(200);
		clickMouse(button);
	#endif
}

/* Function used to scroll the screen in the required direction. */
void scrollMouseXY(int x, int y) {
	#if defined(IS_WINDOWS)
		// Fix for #97, C89 needs variables declared on top of functions (mouseScrollInput)
		INPUT mouseScrollInputH;
		INPUT mouseScrollInputV;
	#endif

	#if defined(IS_MACOSX)
		CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
		CGEventRef event = CGEventCreateScrollWheelEvent(source, kCGScrollEventUnitPixel, 2, y, x);
		CGEventPost(kCGHIDEventTap, event);

		CFRelease(event);
		CFRelease(source);
	#elif defined(USE_X11)
		int ydir = 4; /* Button 4 is up, 5 is down. */
		int xdir = 6;
		Display *display = XGetMainDisplay();

		if (y < 0) { ydir = 5; }
		if (x < 0) { xdir = 7; }

		int xi; int yi;
		for (xi = 0; xi < abs(x); xi++) {
			XTestFakeButtonEvent(display, xdir, 1, CurrentTime);
			XTestFakeButtonEvent(display, xdir, 0, CurrentTime);
		}
		for (yi = 0; yi < abs(y); yi++) {
			XTestFakeButtonEvent(display, ydir, 1, CurrentTime);
			XTestFakeButtonEvent(display, ydir, 0, CurrentTime);
		}

		XSync(display, false);
	#elif defined(IS_WINDOWS)
		mouseScrollInputH.type = INPUT_MOUSE;
		mouseScrollInputH.mi.dx = 0;
		mouseScrollInputH.mi.dy = 0;
		mouseScrollInputH.mi.dwFlags = MOUSEEVENTF_WHEEL;
		mouseScrollInputH.mi.time = 0;
		mouseScrollInputH.mi.dwExtraInfo = 0;
		mouseScrollInputH.mi.mouseData = WHEEL_DELTA * x;

		mouseScrollInputV.type = INPUT_MOUSE;
		mouseScrollInputV.mi.dx = 0;
		mouseScrollInputV.mi.dy = 0;
		mouseScrollInputV.mi.dwFlags = MOUSEEVENTF_WHEEL;
		mouseScrollInputV.mi.time = 0;
		mouseScrollInputV.mi.dwExtraInfo = 0;
		mouseScrollInputV.mi.mouseData = WHEEL_DELTA * y;

		SendInput(1, &mouseScrollInputH, sizeof(mouseScrollInputH));
		SendInput(1, &mouseScrollInputV, sizeof(mouseScrollInputV));
	#endif
}

/* A crude, fast hypot() approximation to get around the fact that hypot() is not a standard ANSI C function. */
#if !defined(M_SQRT2)
	#define M_SQRT2 1.4142135623730950488016887 /* Fix for MSVC. */
#endif

static double crude_hypot(double x, double y){
	double big = fabs(x); /* max(|x|, |y|) */
	double small = fabs(y); /* min(|x|, |y|) */

	if (big > small) {
		double temp = big;
		big = small;
		small = temp;
	}

	return ((M_SQRT2 - 1.0) * small) + big;
}

bool smoothlyMoveMouse(MMPointInt32 endPoint, double lowSpeed, double highSpeed){
	MMPointInt32 pos = location();
	// MMSizeInt32 screenSize = getMainDisplaySize();
	double velo_x = 0.0, velo_y = 0.0;
	double distance;

	while ((distance =crude_hypot((double)pos.x - endPoint.x, (double)pos.y - endPoint.y)) > 1.0) {
		double gravity = DEADBEEF_UNIFORM(5.0, 500.0);
		// double gravity = DEADBEEF_UNIFORM(lowSpeed, highSpeed);
		double veloDistance;
		velo_x += (gravity * ((double)endPoint.x - pos.x)) / distance;
		velo_y += (gravity * ((double)endPoint.y - pos.y)) / distance;

		/* Normalize velocity to get a unit vector of length 1. */
		veloDistance = crude_hypot(velo_x, velo_y);
		velo_x /= veloDistance;
		velo_y /= veloDistance;

		pos.x += floor(velo_x + 0.5);
		pos.y += floor(velo_y + 0.5);

		/* Make sure we are in the screen boundaries! (Strange things will happen if we are not.) */
		// if (pos.x >= screenSize.w || pos.y >= screenSize.h) {
		// 	return false;
		// }
		moveMouse(pos);

		/* Wait 1 - 3 milliseconds. */
		microsleep(DEADBEEF_UNIFORM(lowSpeed, highSpeed));
		// microsleep(DEADBEEF_UNIFORM(1.0, 3.0));
	}

	return true;
}
