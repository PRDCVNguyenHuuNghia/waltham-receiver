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

/*******************************************************************************
 **                                                                              **
 **  TARGET    : linux                                                           **
 **                                                                              **
 **  PROJECT   : waltham-receiver                                                **
 **                                                                              **
 **  PURPOSE   : This file is acts as interface to weston compositor at receiver **
 **  side                                                                        **
 **                                                                              **
 *******************************************************************************/

#define GST_USE_UNSTABLE_API

#include <sys/mman.h>
#include <signal.h>
#include <sys/time.h>

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsink.h>

#include <pthread.h>
#include <gst/wayland/wayland.h>
#include <gst/video/videooverlay.h>

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "xdg-shell-client-protocol.h"

#include "wth-receiver-comm.h"
#include "wth-receiver-seat.h"
#include "os-compatibility.h"
#include "bitmap.h"

#define WINDOW_WIDTH_SIZE       1920
#define WINDOW_HEIGHT_SIZE      760

#define PIPELINE_SIZE		4096

static int running = 1;

typedef struct _GstAppContext {
	GMainLoop *loop;
	GstBus *bus;

	GstElement *pipeline;
	GstElement *sink;

	GstWaylandVideo *wl_video;
	GstVideoOverlay *overlay;

	struct display *display;
	struct window *window;
	GstVideoInfo info;

} GstAppContext;

/*
 * pointer callbcak functions
 */
static void
pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *wl_surface,
		wl_fixed_t sx, wl_fixed_t sy)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_enter(window, serial, sx, sy);
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		uint32_t serial, struct wl_surface *surface)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_leave(window, serial);
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_motion(window, time, sx, sy);
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button,
		uint32_t state)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_button(window, serial, time, button, state);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_pointer_axis(window, time, axis, value);
}

static void
pointer_handle_frame(void *data, struct wl_pointer *pointer)
{
	(void) data;
	(void) pointer;
}

static void
pointer_handle_axis_source(void *data, struct wl_pointer *pointer,
		uint32_t source)
{
	(void) data;
	(void) pointer;
	(void) source;
}

static void
pointer_handle_axis_stop(void *data, struct wl_pointer *pointer,
		uint32_t time, uint32_t axis)
{
	(void) data;
	(void) pointer;
	(void) time;
	(void) axis;
}

static void
pointer_handle_axis_discrete(void *data, struct wl_pointer *pointer,
		uint32_t axis, int32_t discrete)
{
	(void) data;
	(void) pointer;
	(void) axis;
	(void) discrete;
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
	pointer_handle_frame,
	pointer_handle_axis_source,
	pointer_handle_axis_stop,
	pointer_handle_axis_discrete
};

/*
 * touch callbcak functions
 */
static void
touch_handle_down(void *data, struct wl_touch *touch, uint32_t serial,
		uint32_t time, struct wl_surface *surface, int32_t id,
		wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_down(window, serial, time, id, x_w, y_w);
}

static void
touch_handle_up(void *data, struct wl_touch *touch, uint32_t serial,
		uint32_t time, int32_t id)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_up(window, serial, time, id);
}

static void
touch_handle_motion(void *data, struct wl_touch *touch, uint32_t time,
		int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_motion(window, time, id, x_w, y_w);
}

static void
touch_handle_frame(void *data, struct wl_touch *touch)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_frame(window);
}

static void
touch_handle_cancel(void *data, struct wl_touch *touch)
{
	struct display *display = data;
	struct window *window = display->window;

	waltham_touch_cancel(window);
}

static void
touch_handle_shape(void *data, struct wl_touch *touch,
		int32_t id, wl_fixed_t maj, wl_fixed_t min)
{
	(void) data;
	(void) touch;

	(void) id;
	(void) maj;
	(void) min;
}

static void
touch_handle_orientation(void *data, struct wl_touch *touch, int32_t id, wl_fixed_t orientation)
{
	(void) data;
	(void) touch;

	(void) id;
	(void) orientation;
}

static const struct wl_touch_listener touch_listener = {
	touch_handle_down,
	touch_handle_up,
	touch_handle_motion,
	touch_handle_frame,
	touch_handle_cancel,
	touch_handle_shape,
	touch_handle_orientation,
};

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
	xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	xdg_wm_base_ping,
};


/*
 * seat callback
 */
static void
seat_capabilities(void *data, struct wl_seat *wl_seat, enum wl_seat_capability caps)
{
	struct display *display = data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !display->wl_pointer) {
		display->wl_pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_set_user_data(display->wl_pointer, display);
		wl_pointer_add_listener(display->wl_pointer, &pointer_listener, display);
		wl_display_roundtrip(display->display);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && display->wl_pointer) {
		wl_pointer_destroy(display->wl_pointer);
		display->wl_pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !display->wl_touch) {
		display->wl_touch = wl_seat_get_touch(wl_seat);
		wl_touch_set_user_data(display->wl_touch, display);
		wl_touch_add_listener(display->wl_touch, &touch_listener, display);
		wl_display_roundtrip(display->display);
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && display->wl_touch) {
		wl_touch_destroy(display->wl_touch);
		display->wl_touch = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_capabilities,
	NULL
};

static void
add_seat(struct display *display, uint32_t id, uint32_t version)
{
	display->wl_pointer = NULL;
	display->wl_touch = NULL;
	display->wl_keyboard = NULL;
	display->seat = wl_registry_bind(display->registry, id,
			&wl_seat_interface, 1);
	wl_seat_add_listener(display->seat, &seat_listener, display);
}

static void
redraw(void *data, struct wl_callback *callback, uint32_t time);

static void
paint_pixels(void *image, int padding, int width, int height, uint32_t time)
{
	memset(image, 0x00, width * height * 4);
}

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct shm_buffer *mybuf = data;
	mybuf->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static int
create_shm_buffer(struct display *display, struct shm_buffer *buffer,
		  int width, int height, uint32_t format)
{
	struct wl_shm_pool *pool;
	int fd, size, stride;
	void *data;

	stride = width * 4;
	size = stride * height;

	fd = os_create_anonymous_file(size);
	if (fd < 0) {
		fprintf(stderr, "creating a buffer file for %d B failed: %s\n",
				size, strerror(errno));
		return -1;
	}

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	pool = wl_shm_create_pool(display->shm, fd, size);
	buffer->buffer = wl_shm_pool_create_buffer(pool, 0, width,
						   height, stride, format);
	wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);
	wl_shm_pool_destroy(pool);
	close(fd);

	buffer->shm_data = data;
	return 0;
}

static struct shm_buffer *
get_next_buffer(struct window *window)
{
	struct shm_buffer *buffer;
	int ret = 0;

	if (!window->buffers[0].busy)
		buffer = &window->buffers[0];
	else if (!window->buffers[1].busy)
		buffer = &window->buffers[1];
	else
		return NULL;

	if (!buffer->buffer) {
		fprintf(stdout, "get_next_buffer() buffer is not set, setting with "
				"width %d, height %d\n", window->width, window->height);
		ret = create_shm_buffer(window->display, buffer, window->width,
					window->height, WL_SHM_FORMAT_XRGB8888);

		if (ret < 0)
			return NULL;

		/* paint the padding */
		memset(buffer->shm_data, 0x00, window->width * window->height * 4);
	}

	return buffer;
}


static const struct wl_callback_listener frame_listener = {
	redraw
};

static struct client *to_client(struct surface *surface)
{
	struct ivisurface *ivisurface = NULL;
	struct client *client = NULL;

	if (!surface)
		return NULL;

	if (!surface->ivisurf)
		return NULL;

	ivisurface = surface->ivisurf;
	if (!ivisurface->appid)
		return client;

	return ivisurface->appid->client;
}

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
        struct window *window = data;
        struct shm_buffer *buffer;

        buffer = get_next_buffer(window);
        if (!buffer) {
		struct client *client = to_client(window->receiver_surf);
                fprintf(stderr,
                        !callback ? "Failed to create the first buffer.\n" :
                        "Both buffers busy at redraw(). Server bug?\n");
		client->pid_destroying = true;
		exit(EXIT_FAILURE);
        }

	// do the actual painting
	paint_pixels(buffer->shm_data, 0x0, window->width, window->height, time);

        wl_surface_attach(window->surface, buffer->buffer, 0, 0);
        wl_surface_damage(window->surface, 0, 0, window->width, window->height);

        if (callback)
                wl_callback_destroy(callback);

        window->callback = wl_surface_frame(window->surface);
        wl_callback_add_listener(window->callback, &frame_listener, window);
        wl_surface_commit(window->surface);

        buffer->busy = 1;
}

static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	struct display *d = data;

	if (format == WL_SHM_FORMAT_XRGB8888)
		d->has_xrgb = true;
}

static const struct wl_shm_listener shm_listener = {
	shm_format
};

static void
handle_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
			      int32_t width, int32_t height, struct wl_array *states)
{
	struct window *window = data;
	uint32_t *p;

	window->fullscreen = 0;
	window->maximized = 0;

	wl_array_for_each(p, states) {
		uint32_t state = *p;
		switch (state) {
			case XDG_TOPLEVEL_STATE_FULLSCREEN:
				window->fullscreen = 1;
				break;
			case XDG_TOPLEVEL_STATE_MAXIMIZED:
				window->maximized = 1;
				break;
		}
	}

	fprintf(stdout, "Got handle_xdg_toplevel_configure() "
			"width %d, height %d, full %d, max %d\n", width, height,
			window->fullscreen, window->maximized);

	if (width > 0 && height > 0) {
		if (!window->fullscreen && !window->maximized) {
			window->width = width;
			window->height = height;
		}
		window->width = width;
		window->height = height;
	} else if (!window->fullscreen && !window->maximized) {
		if (width == 0)
			window->width = WINDOW_WIDTH_SIZE;
		else
			window->width = width;

		if (height == 0)
			window->height = WINDOW_HEIGHT_SIZE;
		else
			window->height = height;
	}

	fprintf(stdout, "settting width %d, height %d\n", window->width,
				window->height);
}


static void
handle_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	running = 0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	handle_xdg_toplevel_configure,
	handle_xdg_toplevel_close,
};

static void
handle_xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	struct window *window = data;

	fprintf(stderr, "Sending configure ack\n");

	xdg_surface_ack_configure(surface, serial);

	if (window->wait_for_configure) {
		redraw(window, NULL, 0);
		window->wait_for_configure = false;
	}
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_xdg_surface_configure,
};


static void
create_surface(struct window *window)
{
	struct display *display = window->display;

	window->surface = wl_compositor_create_surface(display->compositor);
	assert(window->surface);

	if (display->wm_base) {
		window->xdg_surface =
			xdg_wm_base_get_xdg_surface(display->wm_base, window->surface);
		assert(window->xdg_surface);

		xdg_surface_add_listener(window->xdg_surface,
					 &xdg_surface_listener, window);
		window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
		assert(window->xdg_toplevel);

		xdg_toplevel_add_listener(window->xdg_toplevel,
					  &xdg_toplevel_listener, window);

		if (window->app_id)
			xdg_toplevel_set_app_id(window->xdg_toplevel, window->app_id);

		wl_surface_commit(window->surface);
		window->wait_for_configure = true;
	}
}

static void
create_window(struct window *window, struct display *display, int width, int height, const char *app_id)
{
	window->callback = NULL;

	window->display = display;
	window->width = width;
	window->height = height;
	window->window_frames = 0;
	window->window_benchmark_time = 0;
	window->app_id = app_id;
	window->frame_sync = 1;

	create_surface(window);

	return;
}

static void
destroy_window(struct window *window)
{
	if (window->callback)
		wl_callback_destroy(window->callback);

	if (window->xdg_toplevel)
		xdg_toplevel_destroy(window->xdg_toplevel);

	if (window->xdg_surface)
		xdg_surface_destroy(window->xdg_surface);


	wl_surface_destroy(window->surface);
	free(window);
}

static void
signal_int(int signum)
{
	running = 0;
}

/*
 *  registry callback
 */
static void
registry_handle_global(void *data, struct wl_registry *registry,
		uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry,
					 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		add_seat(d, id, version);
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		d->wm_base = wl_registry_bind(registry, id,
				&xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(d->wm_base, &xdg_wm_base_listener, d);

	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
		wl_shm_add_listener(d->shm, &shm_listener, d);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name)
{
	/* stub */
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};


static struct display *
create_display(void)
{
	struct display *display;

	display = malloc(sizeof *display);
	if (display == NULL) {
		wth_error("out of memory\n");
		exit(EXIT_FAILURE);
	}
	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->has_xrgb = false;
	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
			&registry_listener, display);

	wl_display_roundtrip(display->display);

	return display;
}

static void
destroy_display(struct display *display)
{
	if (display->compositor)
		wl_compositor_destroy(display->compositor);

	wl_registry_destroy(display->registry);
	wl_display_flush(display->display);
	wl_display_disconnect(display->display);
	free(display);
}

static void
error_cb(GstBus *bus, GstMessage *msg, gpointer user_data)
{
	GstAppContext *d = user_data;

	gchar *debug = NULL;
	GError *err = NULL;

	gst_message_parse_error(msg, &err, &debug);

	g_print("Error: %s\n", err->message);
	g_error_free(err);

	if (debug) {
		g_print("Debug details: %s\n", debug);
		g_free(debug);
	}

	gst_element_set_state(d->pipeline, GST_STATE_NULL);
}

static GstBusSyncReply
bus_sync_handler(GstBus *bus, GstMessage *message, gpointer user_data)
{
	GstAppContext *d = user_data;

	if (gst_is_wayland_display_handle_need_context_message(message)) {
		GstContext *context;
		struct wl_display *display_handle = d->display->display;

		context = gst_wayland_display_handle_context_new(display_handle);
		d->wl_video = GST_WAYLAND_VIDEO(GST_MESSAGE_SRC(message));
		gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context);

		goto drop;
	} else if (gst_is_video_overlay_prepare_window_handle_message(message)) {
		struct wl_surface *window_handle = d->window->surface;

		/* GST_MESSAGE_SRC(message) will be the overlay object that we
		 * have to use. This may be waylandsink, but it may also be
		 * playbin. In the latter case, we must make sure to use
		 * playbin instead of waylandsink, because playbin resets the
		 * window handle and render_rectangle after restarting playback
		 * and the actual window size is lost */
		d->overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));

		g_print("setting window handle and size (%d x %d) w %d, h %d\n", 
				d->window->x, d->window->y,
				d->window->width, d->window->height);

		gst_video_overlay_set_window_handle(d->overlay, (guintptr) window_handle);
		gst_video_overlay_set_render_rectangle(d->overlay,
				d->window->x, d->window->y,
				d->window->width, d->window->height);

		goto drop;
	}

	return GST_BUS_PASS;

drop:
	gst_message_unref(message);
	return GST_BUS_DROP;
}


/**
 * wth_receiver_weston_main
 *
 * This is the main function which will handle connection to the compositor at
 * receiver side
 *
 * @param names        void *data
 * @param value        struct window data
 * @return             0 on success, -1 on error
 */
int
wth_receiver_weston_main(struct window *window, const char *app_id, int port)
{
	struct sigaction sigint;
	GstAppContext gstctx;
	int ret = 0;
	GError *gerror = NULL;
	char pipeline[PIPELINE_SIZE];

	memset(&gstctx, 0, sizeof(gstctx));

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* Initialization for window creation */
	gstctx.display = create_display();

	/* ToDo: fix the hardcoded value of width, height */
	create_window(window, gstctx.display, WINDOW_WIDTH_SIZE,
		      WINDOW_HEIGHT_SIZE, app_id);

	gstctx.window = window;
	gstctx.display->window = window;

	fprintf(stderr, "display %p\n", gstctx.display);
	fprintf(stderr, "display->window %p\n", gstctx.display->window);
	fprintf(stderr, "window %p\n", window);

	/* Initialise damage to full surface, so the padding gets painted */
	wl_surface_damage(window->surface, 0, 0,
			  window->width, window->height);

	if (!window->wait_for_configure)
		redraw(window, NULL, 0);

	int gargc = 2;
	char **gargv = (char**) malloc(2 * sizeof(char*));

	gargv[0] = strdup("waltham-receiver");
	gargv[1] = strdup("--gst-debug-level=2");

	/* create gstreamer pipeline */
	gst_init(&gargc, &gargv);

	const char *pipe = "rtpbin name=rtpbin udpsrc "
		"caps=\"application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=JPEG,payload=26\" "
		"port=%d ! rtpbin.recv_rtp_sink_0 rtpbin. ! "
		"rtpjpegdepay ! jpegdec ! waylandsink";

	memset(pipeline, 0x00, sizeof(pipeline));
	snprintf(pipeline, sizeof(pipeline), pipe, port);

	fprintf(stdout, "Using pipeline %s\n", pipeline);

	/* parse the pipeline */
	gstctx.pipeline = gst_parse_launch(pipeline, &gerror);
	if (!gstctx.pipeline) {
		struct client *client = to_client(window->receiver_surf);

		fprintf(stderr, "Could not create gstreamer pipeline.\n");
		client->pid_destroying = true;
		exit(EXIT_FAILURE);
	}

	gstctx.bus = gst_element_get_bus(gstctx.pipeline);
	gst_bus_add_signal_watch(gstctx.bus);

	fprintf(stdout, "registered bus signal\n");

	g_signal_connect(gstctx.bus, "message::error", G_CALLBACK(error_cb), &gstctx);
	gst_bus_set_sync_handler(gstctx.bus, bus_sync_handler, &gstctx, NULL);
	gst_object_unref(gstctx.bus);

	gst_element_set_state(gstctx.pipeline, GST_STATE_PLAYING);

	while (running && ret != -1)
		ret = wl_display_dispatch(gstctx.display->display);

	gst_element_set_state(gstctx.pipeline, GST_STATE_NULL);
	gst_object_unref(gstctx.pipeline);

	destroy_window(window);
	destroy_display(gstctx.display);
	free(gargv);

	fprintf(stdout, "Exiting, closed down gstreamer pipeline\n");
	/* note, we do a exit here because wth_receiver_weston_main() isn't
	 * really C's main() and we fork() before calling this. Doing a return
	 * will not correctly signal the parent that the child process exited
	 */
	exit(EXIT_SUCCESS);
}
