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
#include "wth-receiver-buffer.h"
#include "wth-receiver-seat.h"
#include "wth-receiver-surface.h"

void
wth_receiver_weston_shm_attach(struct window *window, uint32_t data_sz, void * data,
		int32_t width, int32_t height, int32_t stride, uint32_t format)
{
	/* stub */
}

void
wth_receiver_weston_shm_damage(struct window *window)
{
	/* stub */
}

void
wth_receiver_weston_shm_commit(struct window *window)
{
	/* stub */
}

/*
 * waltham surface implementation
 */
void
surface_destroy(struct surface *surface)
{
	wthp_surface_free(surface->obj);
	wl_list_remove(&surface->link);
	free(surface);
}

static void
surface_handle_destroy(struct wthp_surface *wthp_surface)
{
	struct surface *surface = wth_object_get_user_data((struct wth_object *)wthp_surface);

	assert(wthp_surface == surface->obj);
	surface_destroy(surface);
}

static void
surface_handle_attach(struct wthp_surface *wthp_surface,
		struct wthp_buffer *wthp_buff, int32_t x, int32_t y)
{
	struct surface *surf = wth_object_get_user_data((struct wth_object *)wthp_surface);
	struct buffer *buf = container_of(&wthp_buff, struct buffer, obj);

	if (surf->ivi_id != 0) {
		wth_receiver_weston_shm_attach(surf->shm_window,
				buf->data_sz,
				buf->data,
				buf->width,
				buf->height,
				buf->stride,
				buf->format);

		wthp_buffer_send_complete(wthp_buff, 0);
	}
}

static void
surface_handle_damage(struct wthp_surface *wthp_surface,
		int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct surface *surf = wth_object_get_user_data((struct wth_object *)wthp_surface);

	if (surf->ivi_id != 0) {
		wth_receiver_weston_shm_damage(surf->shm_window);
	}
}

static void
surface_handle_frame(struct wthp_surface *wthp_surface,
		struct wthp_callback *callback)
{
	struct surface *surf = wth_object_get_user_data((struct wth_object *)wthp_surface);
	surf->cb = callback;
}

static void
surface_handle_set_opaque_region(struct wthp_surface *wthp_surface,
		struct wthp_region *region)
{

}

static void
surface_handle_set_input_region(struct wthp_surface *wthp_surface,
		struct wthp_region *region)
{

}

static void
surface_handle_commit(struct wthp_surface *wthp_surface)
{
	struct surface *surf = wth_object_get_user_data((struct wth_object *)wthp_surface);

	if (surf->ivi_id != 0) {
		wth_receiver_weston_shm_commit(surf->shm_window);
	}
}

static void
surface_handle_set_buffer_transform(struct wthp_surface *wthp_surface,
		int32_t transform)
{

}

static void
surface_handle_set_buffer_scale(struct wthp_surface *wthp_surface,
		int32_t scale)
{

}

static void
surface_handle_damage_buffer(struct wthp_surface *wthp_surface,
		int32_t x, int32_t y, int32_t width, int32_t height)
{

}

static const struct wthp_surface_interface surface_implementation = {
	surface_handle_destroy,
	surface_handle_attach,
	surface_handle_damage,
	surface_handle_frame,
	surface_handle_set_opaque_region,
	surface_handle_set_input_region,
	surface_handle_commit,
	surface_handle_set_buffer_transform,
	surface_handle_set_buffer_scale,
	surface_handle_damage_buffer
};

static void
compositor_handle_create_surface(struct wthp_compositor *compositor,
				 struct wthp_surface *id)
{

	struct compositor *comp = wth_object_get_user_data((struct wth_object *)compositor);
	struct client *client = comp->client;
	struct surface *surface;
	struct seat *seat, *tmp;

	surface = zalloc(sizeof *surface);
	if (!surface) {
		client_post_out_of_memory(comp->client);
		return;
	}

	surface->obj = id;
	wl_list_insert(&comp->client->surface_list, &surface->link);

	wthp_surface_set_interface(id, &surface_implementation, surface);

	surface->shm_window = calloc(1, sizeof *surface->shm_window);
	if (!surface->shm_window)
		return;

	surface->shm_window->receiver_surf = surface;
	surface->shm_window->ready = false;
	surface->ivi_id = 0;

	wl_list_for_each_safe(seat, tmp, &client->seat_list, link) {
		surface->shm_window->receiver_seat = seat;
	}
}

/*
 * waltham region implementation
 */
void
region_destroy(struct region *region)
{
	wthp_region_free(region->obj);
	wl_list_remove(&region->link);
	free(region);
}

static void
region_handle_destroy(struct wthp_region *wthp_region)
{
	struct region *region = wth_object_get_user_data((struct wth_object *)wthp_region);
	assert(wthp_region == region->obj);
	region_destroy(region);
}

static void
region_handle_add(struct wthp_region *wthp_region,
		int32_t x, int32_t y, int32_t width, int32_t height)
{
}

static void
region_handle_subtract(struct wthp_region *wthp_region,
		int32_t x, int32_t y,
		int32_t width, int32_t height)
{
}

static const struct wthp_region_interface region_implementation = {
	region_handle_destroy,
	region_handle_add,
	region_handle_subtract
};

/*
 * waltham compositor implementation
 */
void
compositor_destroy(struct compositor *comp)
{
	wthp_compositor_free(comp->obj);
	wl_list_remove(&comp->link);
	free(comp);
}

static void
compositor_handle_create_region(struct wthp_compositor *compositor,
				struct wthp_region *id)
{
	struct compositor *comp = wth_object_get_user_data((struct wth_object *)compositor);
	struct region *region;

	region = zalloc(sizeof *region);
	if (!region) {
		client_post_out_of_memory(comp->client);
		return;
	}

	region->obj = id;
	wl_list_insert(&comp->client->region_list, &region->link);

	wthp_region_set_interface(id, &region_implementation, region);
}

static const struct wthp_compositor_interface compositor_implementation = {
	compositor_handle_create_surface,
	compositor_handle_create_region
};

void
client_bind_compositor(struct client *c, struct wthp_compositor *obj)
{

	struct compositor *comp;

	comp = zalloc(sizeof *comp);
	if (!comp) {
		client_post_out_of_memory(c);
		return;
	}

	comp->obj = obj;
	comp->client = c;
	wl_list_insert(&c->compositor_list, &comp->link);

	wthp_compositor_set_interface(obj, &compositor_implementation, comp);
}
