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

static void
buffer_handle_destroy(struct wthp_buffer *wthp_buffer)
{
	struct buffer *buf = wth_object_get_user_data((struct wth_object *)wthp_buffer);

	wthp_buffer_free(wthp_buffer);
	wl_list_remove(&buf->link);
	free(buf);
}

static const struct wthp_buffer_interface buffer_implementation = {
	buffer_handle_destroy
};

/* BEGIN wthp_blob_factory implementation */

static void
blob_factory_create_buffer(struct wthp_blob_factory *blob_factory,
			   struct wthp_buffer *wthp_buffer, uint32_t data_sz, void *data,
			   int32_t width, int32_t height, int32_t stride, uint32_t format)
{
	struct blob_factory *blob = wth_object_get_user_data((struct wth_object *)blob_factory);
	struct buffer *buffer;

	buffer = zalloc(sizeof *buffer);
	if (!buffer) {
		client_post_out_of_memory(blob->client);
		return;
	}

	wl_list_insert(&blob->client->buffer_list, &buffer->link);

	buffer->data_sz = data_sz;
	buffer->data = data;
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;
	buffer->format = format;
	buffer->obj = wthp_buffer;

	wthp_buffer_set_interface(wthp_buffer, &buffer_implementation, buffer);
}

static const struct wthp_blob_factory_interface blob_factory_implementation = {
	blob_factory_create_buffer
};

void
client_bind_blob_factory(struct client *c, struct wthp_blob_factory *obj)
{
	struct blob_factory *blob;

	blob = zalloc(sizeof *blob);
	if (!blob) {
		client_post_out_of_memory(c);
		return;
	}

	blob->obj = obj;
	blob->client = c;
	wl_list_insert(&c->compositor_list, &blob->link);

	wthp_blob_factory_set_interface(obj,
					&blob_factory_implementation, blob);
	fprintf(stderr, "client %p bound wthp_blob_factory\n", c);
}
