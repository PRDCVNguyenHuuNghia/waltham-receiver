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
#ifndef WTH_SERVER_WALTHAM_BUFFER_H_
#define WTH_SERVER_WALTHAM_BUFFER_H_

/* wthp_blob_factory protocol object */
struct blob_factory {
    struct wthp_blob_factory *obj;
    struct client *client;
    struct wl_list link; /* struct client::blob_factory_list */
};

/* wthp_buffer protocol object */
struct buffer {
    struct wthp_buffer *obj;
    uint32_t data_sz;
    void *data;
    int32_t width;
    int32_t height;
    int32_t stride;
    uint32_t format;
    struct wl_list link; /* struct client::buffer_list */
};

void
client_bind_blob_factory(struct client *c, struct wthp_blob_factory *obj);

#endif
