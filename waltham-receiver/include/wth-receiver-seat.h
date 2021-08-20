/*
 * Copyright © 2019 Advanced Driver Information Technology GmbH
 * Copyright © 2020 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WTH_SERVER_WALTHAM_SEAT_H_
#define WTH_SERVER_WALTHAM_SEAT_H_

struct pointer;
struct touch;
struct window;
struct receiver;
struct client;

struct wthp_seat;

enum wthp_seat_capability {
    /**
     * the seat has pointer devices
     */
    WTHP_SEAT_CAPABILITY_POINTER = 1,
    /**
     * the seat has one or more keyboards
     */
    WTHP_SEAT_CAPABILITY_KEYBOARD = 2,
    /**
     * the seat has touch devices
     */
    WTHP_SEAT_CAPABILITY_TOUCH = 4,
};


/* wthp_seat protocol object */
struct seat {
    struct wthp_seat *obj;
    struct client *client;
    struct pointer *pointer;
    struct keyboard *keyboard;
    struct touch *touch;
    struct wl_list link; /* struct client::seat_list */
};

/* wthp_pointer protocol object */
struct pointer {
    struct wthp_pointer *obj;
    struct seat *seat;
    struct wl_list link; /* struct client::pointer_list */
};

/* wthp_keyboard protocol object */
struct keyboard {
    struct wthp_keyboard *obj;
    struct seat *seat;
    struct wl_list link; /* struct client::keyboard_list */
};

/* wthp_touch protocol object */
struct touch {
    struct wthp_touch *obj;
    struct seat *seat;
    struct wl_list link; /* struct client::touch_list */
};


/**
* waltham_pointer_enter
*
* Send pointer enter event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t serial
*             wl_fixed_t sx
*             wl_fixed_t sy
* @param value        window - window information
*                     serial - serial number of the enter event
*             sx     - surface-local x coordinate
*             sy     - surface-local y coordinate
* @return             none
*/
void waltham_pointer_enter(struct window *window, uint32_t serial,
                      wl_fixed_t sx, wl_fixed_t sy);

/**
* waltham_pointer_leave
*
* Send pointer leave event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t serial
* @param value        window - window information
*                     serial - serial number of the leave event
* @return             none
*/
void waltham_pointer_leave(struct window *window, uint32_t serial);

/**
* waltham_pointer_motion
*
* Send pointer motion event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t time
*             wl_fixed_t sx
*             wl_fixed_t sy
* @param value        window - window information
*                     time   - timestamp with millisecond granularity
*             sx     - surface-local x coordinate
*             sy     - surface-local y coordinate
* @return             none
*/
void waltham_pointer_motion(struct window *window, uint32_t time,
                       wl_fixed_t sx, wl_fixed_t sy);

/**
* waltham_pointer_button
*
* Send pointer button event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t serial
*             uint32_t time
*                     uint32_t button
*             uint32_t state
* @param value        window - window information
*             serial - serial number of the button event
*                     time   - timestamp with millisecond granularity
*             button - button that produced the event
*             state  - physical state of the button
* @return             none
*/
void waltham_pointer_button(struct window *window, uint32_t serial,
               uint32_t time, uint32_t button,
               uint32_t state);

/**
* waltham_pointer_axis
*
* Send pointer axis event received from weston to waltham client
*
* @param names        struct window *window
*             uint32_t time
*             uint32_t axis
*             wl_fixed_t value
* @param value        window - window information
*             time   - timestamp with millisecond granularity
*             axis   - axis type
*                     value  - length of vector in surface-local coordinate space
* @return             none
*/
void waltham_pointer_axis(struct window *window, uint32_t time,
             uint32_t axis, wl_fixed_t value);

/**
* waltham_touch_down
*
* Send touch down event received from weston to waltham client
*
* @param names     struct window *window
*          uint32_t serial
*                  uint32_t time
*          int32_t id
*          wl_fixed_t x_w
*          wl_fixed_t y_w
* @param value     window - window information
*          serial - serial number of the touch down event
*          time   - timestamp with millisecond granularity
*          id     - the unique ID of this touch point
*          x_w    - surface-local x coordinate
*          y_w    - surface-local y coordinate
* @return          none
*/
void waltham_touch_down(struct window *window, uint32_t serial,
                   uint32_t time, int32_t id,
           wl_fixed_t x_w, wl_fixed_t y_w);

/**
* waltham_touch_up
*
* Send touch up event received from weston to waltham client
*
* @param names     struct window *window
*          uint32_t serial
*                  uint32_t time
*          int32_t id
* @param value     window - window information
*          serial - serial number of the touch up event
*          time   - timestamp with millisecond granularity
*          id     - the unique ID of this touch point
* @return          none
*/
void waltham_touch_up(struct window *window, uint32_t serial,
                 uint32_t time, int32_t id);

/**
* waltham_touch_motion
*
* Send touch motion event received from weston to waltham client
*
* @param names     struct window *window
*                  uint32_t time
*          int32_t id
*          wl_fixed_t x_w
*          wl_fixed_t y_w
* @param value     window - window information
*          time   - timestamp with millisecond granularity
*          id     - the unique ID of this touch point
*          x_w    - surface-local x coordinate
*          y_w    - surface-local y coordinate
* @return          none
*/
void waltham_touch_motion(struct window *window, uint32_t time,
             int32_t id, wl_fixed_t x_w, wl_fixed_t y_w);

/**
* waltham_touch_frame
*
* Send touch frame event received from weston to waltham client
*
* @param names        struct window *window
* @param value        window - window information
* @return             none
*/
void waltham_touch_frame(struct window *window);

/**
* waltham_touch_cancel
*
* Send touch cancel event received from weston to waltham client
*
* @param names        struct window *window
* @param value        window - window information
* @return             none
*/
void waltham_touch_cancel(struct window *window);

void
client_bind_seat(struct client *c, struct wthp_seat *obj);

void
seat_send_updated_caps(struct seat *seat);

#endif
