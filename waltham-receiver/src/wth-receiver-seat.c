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

#include "wth-receiver-comm.h"
#include "wth-receiver-seat.h"

/*
 * APIs to send touch events to waltham client
 */
void
waltham_touch_down(struct window *window, uint32_t serial,
		uint32_t time, int32_t id,
		wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct seat *seat = window->receiver_seat;
	struct touch *touch = seat->touch;
	struct surface *surface = window->receiver_surf;

	if (!touch) {
		fprintf(stderr, "We do not have touch device!\n");
		return;
	}

	if (touch->obj) {
		wthp_touch_send_down(touch->obj, serial, time, surface->obj, id, x_w, y_w);
	}
	return;
}

void
waltham_touch_up(struct window *window, uint32_t serial,
		uint32_t time, int32_t id)
{
	struct seat *seat = window->receiver_seat;
	struct touch *touch = seat->touch;

	if (!touch) {
		fprintf(stderr, "We do not have touch device!\n");
		return;
	}

	if (touch->obj) {
		wthp_touch_send_up(touch->obj, serial, time, id);
	}
	return;
}

void
waltham_touch_motion(struct window *window, uint32_t time,
		int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct seat *seat = window->receiver_seat;
	struct touch *touch = seat->touch;

	if (!touch) {
		fprintf(stderr, "We do not have touch device!\n");
		return;
	}

	if (touch->obj) {
		wthp_touch_send_motion(touch->obj, time, id, x_w, y_w);
	}
}

void
waltham_touch_frame(struct window *window)
{
	struct seat *seat = window->receiver_seat;
	struct touch *touch = seat->touch;

	if (!touch) {
		fprintf(stderr, "We do not have touch device!\n");
		return;
	}

	if (touch->obj) {
		wthp_touch_send_frame(touch->obj);
	}
}

void
waltham_touch_cancel(struct window *window)
{
	struct seat *seat = window->receiver_seat;
	struct touch *touch = seat->touch;

	if (!touch) {
		fprintf(stderr, "We do not have touch device!\n");
		return;
	}

	if (touch->obj) {
		fprintf(stdout, "waltham_touch_cancel() sending cancel\n");
		wthp_touch_send_cancel(touch->obj);
	}
}

/*
 * APIs to send pointer events to waltham client
 */

void
waltham_pointer_enter(struct window *window, uint32_t serial,
                      wl_fixed_t sx, wl_fixed_t sy)
{
	struct surface *surface = window->receiver_surf;
	struct seat *seat = window->receiver_seat;
	struct pointer *pointer = seat->pointer;

	if (!pointer) {
		fprintf(stderr, "We do not have pointer device!\n");
		return;
	}

	if (pointer && pointer->obj) {
		fprintf(stdout, "waltham_pointer_enter() sending enter\n");
		wthp_pointer_send_enter(pointer->obj, serial, surface->obj, sx, sy);
	}
}

void
waltham_pointer_leave(struct window *window, uint32_t serial)
{
	struct surface *surface = window->receiver_surf;
	struct seat *seat = window->receiver_seat;
	struct pointer *pointer = seat->pointer;

	if (!pointer) {
		fprintf(stderr, "We do not have pointer device!\n");
		return;
	}

	if (pointer && pointer->obj) {
		fprintf(stdout, "waltham_pointer_leave() sending leave\n");
		wthp_pointer_send_leave(pointer->obj, serial, surface->obj);
	}
}

void
waltham_pointer_motion(struct window *window, uint32_t time,
                       wl_fixed_t sx, wl_fixed_t sy)
{
	struct seat *seat = window->receiver_seat;
	struct pointer *pointer = seat->pointer;

	if (!pointer) {
		fprintf(stderr, "We do not have pointer device!\n");
		return;
	}

	if (pointer && pointer->obj) {
		fprintf(stdout, "waltham_pointer_motion() sending motion\n");
		wthp_pointer_send_motion(pointer->obj, time, sx, sy);
	}
}

void
waltham_pointer_button(struct window *window, uint32_t serial,
               uint32_t time, uint32_t button,
               uint32_t state)
{
	struct seat *seat = window->receiver_seat;
	struct pointer *pointer = seat->pointer;

	if (!pointer) {
		fprintf(stderr, "We do not have pointer device!\n");
		return;
	}

	if (pointer && pointer->obj) {
		fprintf(stdout, "waltham_pointer_button() sending button\n");
		wthp_pointer_send_button(pointer->obj, serial, time, button, state);
	}
}

void
waltham_pointer_axis(struct window *window, uint32_t time,
             uint32_t axis, wl_fixed_t value)
{
	struct seat *seat = window->receiver_seat;
	struct pointer *pointer = seat->pointer;

	if (!pointer) {
		fprintf(stderr, "We do not have pointer device!\n");
		return;
	}

	if (pointer && pointer->obj) {
		fprintf(stdout, "waltham_pointer_axis() sending pointer_axis\n");
		wthp_pointer_send_axis(pointer->obj, time, axis, value);
	}
}

/*
 *  waltham seat implementation
 */

/*
 *  waltham touch implementation
 */
static void
touch_release(struct wthp_touch *wthp_touch)
{
	struct touch *touch = wth_object_get_user_data((struct wth_object *)wthp_touch);
	fprintf(stdout, "touch_release %p touch %p\n", wthp_touch, touch);
}

static const struct wthp_touch_interface touch_implementation = {
	touch_release,
};

/*
 *  waltham pointer implementation
 */
static void
pointer_set_cursor(struct wthp_pointer *wthp_pointer, uint32_t serial, struct wthp_surface *surface,
           int32_t hotspot_x, int32_t hotspot_y)
{
}

static void
pointer_release(struct wthp_pointer *wthp_pointer)
{
}

static const struct wthp_pointer_interface pointer_implementation = {
	pointer_set_cursor,
	pointer_release
};

static void
seat_get_pointer(struct wthp_seat *wthp_seat, struct wthp_pointer *wthp_pointer)
{

	fprintf(stdout, "wthp_seat %p get_pointer(%p)\n", wthp_seat, wthp_pointer);

	struct seat *seat = wth_object_get_user_data((struct wth_object *)wthp_seat);
	struct pointer *pointer;

	pointer = zalloc(sizeof *pointer);
	if (!pointer) {
		client_post_out_of_memory(seat->client);
		return;
	}

	pointer->obj = wthp_pointer;
	pointer->seat = seat;
	seat->pointer = pointer;
	wl_list_insert(&seat->client->pointer_list, &pointer->link);

	wthp_pointer_set_interface(wthp_pointer, &pointer_implementation, pointer);

}

static void
seat_get_touch(struct wthp_seat *wthp_seat, struct wthp_touch *wthp_touch)
{
	struct seat *seat = wth_object_get_user_data((struct wth_object *)wthp_seat);
	struct touch *touch;

	fprintf(stdout, "wthp_seat %p get_touch(%p)\n", wthp_seat, wthp_touch);

	touch = zalloc(sizeof(*touch));
	if (!touch) {
		client_post_out_of_memory(seat->client);
		return;
	}

	touch->obj = wthp_touch;
	touch->seat = seat;

	seat->touch = touch;

	fprintf(stdout, "seat_get_touch() with obj %p\n", touch->obj);

	wl_list_insert(&seat->client->touch_list, &touch->link);
	wthp_touch_set_interface(wthp_touch, &touch_implementation, touch);
}

static void
seat_get_keyboard(struct wthp_seat *wthp_seat, struct wthp_keyboard *kid)
{
	(void) wthp_seat;
	(void) kid;
}

static void
seat_release(struct wthp_seat *wthp_seat)
{

}

static const struct wthp_seat_interface seat_implementation = {
	seat_get_pointer,
	seat_get_keyboard,
	seat_get_touch,
	seat_release,
};

void
seat_send_updated_caps(struct seat *seat)
{
	enum wthp_seat_capability caps = 0;

	caps |= WTHP_SEAT_CAPABILITY_POINTER;
	fprintf(stdout, "WTHP_SEAT_CAPABILITY_POINTER %d\n", caps);

	caps |= WTHP_SEAT_CAPABILITY_TOUCH;
	fprintf(stdout, "WTHP_SEAT_CAPABILITY_TOUCH %d\n", caps);

	wthp_seat_send_capabilities(seat->obj, caps);
}

void
client_bind_seat(struct client *c, struct wthp_seat *obj)
{
	struct seat *seat;

	seat = zalloc(sizeof *seat);
	if (!seat) {
		client_post_out_of_memory(c);
		return;
	}

	seat->obj = obj;
	seat->client = c;
	wl_list_insert(&c->seat_list, &seat->link);

	wthp_seat_set_interface(obj, &seat_implementation, seat);
	seat_send_updated_caps(seat);
}
