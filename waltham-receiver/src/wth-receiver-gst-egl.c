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

#include <GL/gl.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "xdg-shell-client-protocol.h"

#include "wth-receiver-seat.h"
#include "wth-receiver-comm.h"
#include "os-compatibility.h"
#include "bitmap.h"

#define WINDOW_WIDTH_SIZE       800
#define WINDOW_HEIGHT_SIZE      600

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

static const gchar *vertex_shader_str =
"attribute vec4 a_position;   \n"
"attribute vec2 a_texCoord;   \n"
"varying vec2 v_texCoord;     \n"
"void main()                  \n"
"{                            \n"
"   gl_Position = a_position; \n"
"   v_texCoord = a_texCoord;  \n"
"}                            \n";

static const gchar *fragment_shader_str =
"#ifdef GL_ES                                          \n"
"precision mediump float;                              \n"
"#endif                                                \n"
"varying vec2 v_texCoord;                              \n"
"uniform sampler2D tex;                                \n"
"void main()                                           \n"
"{                                                     \n"
"vec2 uv;                                              \n"
"uv = v_texCoord.xy;                                   \n"
"vec4 c = texture2D(tex, uv);                          \n"
"gl_FragColor = c;                                     \n"
"}                                                     \n";

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

/*
 *  registry callback
 */
static void
registry_handle_global(void *data, struct wl_registry *registry,
		uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor = wl_registry_bind(registry,
					 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		add_seat(d, id, version);
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		d->wm_base = wl_registry_bind(registry, id,
				&xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(d->wm_base, &xdg_wm_base_listener, d);
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
		exit(1);
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

static bool
wth_check_egl_extension(const char *extensions, const char *extension)
{
	size_t extlen = strlen(extension);
	const char *end = extensions + strlen(extensions);

	while (extensions < end) {
		size_t n = 0;

		/* Skip whitespaces, if any */
		if (*extensions == ' ') {
			extensions++;
			continue;
		}

		n = strcspn(extensions, " ");

		/* Compare strings */
		if (n == extlen && strncmp(extension, extensions, n) == 0)
			return true; /* Found */

		extensions += n;
	}

	/* Not found */
	return false;
}

static inline void *
wth_get_egl_proc_address(const char *address)
{
	const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS); 

	if (extensions &&
	    (wth_check_egl_extension(extensions, "EGL_EXT_platform_wayland") ||
	     wth_check_egl_extension(extensions, "EGL_KHR_platform_wayland"))) {
		return (void *) eglGetProcAddress(address);
	}

	return NULL;
}

static inline EGLDisplay
wth_get_egl_display(EGLenum platform, void *native_display,
			     const EGLint *attrib_list)
{
	static PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;

	if (!get_platform_display) {
		get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
			wth_get_egl_proc_address("eglGetPlatformDisplayEXT");
	}

	if (get_platform_display)
		return get_platform_display(platform, native_display, attrib_list);

	/* FIXME: no fall-through using eglGetDisplay() */
	return NULL;
}

static void
init_egl(struct display *display)
{
	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLint major, minor, count;
	EGLBoolean ret;

	display->egl.dpy = wth_get_egl_display(EGL_PLATFORM_WAYLAND_KHR,
					       display->display, NULL);
	assert(display->egl.dpy);

	ret = eglInitialize(display->egl.dpy, &major, &minor);
	assert(ret == EGL_TRUE);
	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	ret = eglChooseConfig(display->egl.dpy, config_attribs,
			&display->egl.conf, 1, &count);
	assert(ret && count >= 1);

	display->egl.ctx = eglCreateContext(display->egl.dpy,
			display->egl.conf,
			EGL_NO_CONTEXT, context_attribs);
	assert(display->egl.ctx);

	eglSwapInterval(display->egl.dpy, 1);

	display->egl.create_image =
		(void *) eglGetProcAddress("eglCreateImageKHR");
	assert(display->egl.create_image);

	display->egl.image_texture_2d =
		(void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	assert(display->egl.image_texture_2d);

	display->egl.destroy_image =
		(void *) eglGetProcAddress("eglDestroyImageKHR");
	assert(display->egl.destroy_image);
	
}

GLuint load_shader(GLenum type, const char *shaderSrc)
{
	
	GLuint shader;
	GLint compiled;

	/* Create the shader object */
	shader = glCreateShader(type);
	if (shader == 0)
	{
		printf("\n Failed to create shader \n");
		return 0;
	}
	/* Load the shader source */
	glShaderSource(shader, 1, &shaderSrc, NULL);
	/* Compile the shader */
	glCompileShader(shader);
	/* Check the compile status */
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1)
		{
			char* infoLog = (char*)malloc (sizeof(char) * infoLen );
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			fprintf(stderr, "Error compiling shader:%s\n",infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

void init_gl(struct display *display)
{

	GLint linked;

	/* load vertext/fragment shader */
	display->gl.vertex_shader = load_shader(GL_VERTEX_SHADER, vertex_shader_str);
	display->gl.fragment_shader = load_shader(GL_FRAGMENT_SHADER, fragment_shader_str);

	/* Create the program object */
	display->gl.program_object = glCreateProgram();
	if (display->gl.program_object == 0)
	{
		fprintf(stderr, "error program object\n");
		return;
	}

	glAttachShader(display->gl.program_object, display->gl.vertex_shader);
	glAttachShader(display->gl.program_object, display->gl.fragment_shader);
	/* Bind vPosition to attribute 0 */
	glBindAttribLocation(display->gl.program_object, 0, "a_position");
	/* Link the program */
	glLinkProgram(display->gl.program_object);
	/* Check the link status */
	glGetProgramiv(display->gl.program_object, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		GLint infoLen = 0;
		glGetProgramiv(display->gl.program_object, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1)
		{
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);
			glGetProgramInfoLog(display->gl.program_object, infoLen, NULL, infoLog);
			fprintf(stderr, "Error linking program:%s\n", infoLog);
			free(infoLog);
		}
		glDeleteProgram(display->gl.program_object);
	}

	glGenTextures(1, &display->gl.texture);

	return;
}

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

	if (window->native) {
		fprintf(stdout, "wayland-egl to resize to %dx%d\n", window->width, window->height);
		wl_egl_window_resize(window->native, window->width,
				     window->height, 0, 0);
	}
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

static inline EGLSurface
wth_create_egl_surface(EGLDisplay dpy, EGLConfig config,
			void *native_window, const EGLint *attrib_list)
{
	static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC
					create_platform_window = NULL;

	if (!create_platform_window) {
		create_platform_window = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)
			wth_get_egl_proc_address("eglCreatePlatformWindowSurfaceEXT");
	}

	if (create_platform_window)
		return create_platform_window(dpy, config,
					      native_window, attrib_list);

	return eglCreateWindowSurface(dpy, config,
				      (EGLNativeWindowType) native_window,
				      attrib_list);
}

static inline EGLBoolean
wth_destroy_egl_surface(EGLDisplay display, EGLSurface surface)
{
	return eglDestroySurface(display, surface);
}

void
redraw(struct window *window)
{
	struct wl_region *region;
	struct display *display = window->display;

	if (window->opaque || window->fullscreen) {
		region = wl_compositor_create_region(window->display->compositor);
		wl_region_add(region, 0, 0,
				window->width,
				window->height);
		wl_surface_set_opaque_region(window->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}

	fprintf(stdout, "Doing a redraw\n");
	eglSwapBuffers(display->egl.dpy, window->egl_surface);
}

static void
handle_xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	struct window *window = data;

	fprintf(stderr, "Sending configure ack\n");
	xdg_surface_ack_configure(surface, serial);
	window->wait_for_configure = false;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_xdg_surface_configure,
};


static void
create_surface(struct window *window)
{
	
	struct display *display = window->display;
	int ret;

	window->surface = wl_compositor_create_surface(display->compositor);
	assert(window->surface);

	window->native = wl_egl_window_create(window->surface,
					      window->width, window->height);
	assert(window->native);

	window->egl_surface = wth_create_egl_surface(display->egl.dpy,
						     display->egl.conf,
						     window->native, NULL);

	wl_display_roundtrip(display->display);

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


	ret = eglMakeCurrent(display->egl.dpy, window->egl_surface,
			     window->egl_surface, display->egl.ctx);
	assert(ret == EGL_TRUE);

	if (!window->frame_sync)
		eglSwapInterval(display->egl.dpy, 0);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
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

	fprintf(stdout, "entering bus_sync_handler()  setting it\n");

	if (gst_is_wayland_display_handle_need_context_message(message)) {
		GstContext *context;
		struct wl_display *display_handle = d->display->display;

		context = gst_wayland_display_handle_context_new(display_handle);
		d->wl_video = GST_WAYLAND_VIDEO(GST_MESSAGE_SRC(message));
		gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context);

		fprintf(stdout, "bus_sync_handler(): creating context and setting it\n");

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
	init_egl(gstctx.display);

	/* ToDo: fix the hardcoded value of width, height */
	create_window(window, gstctx.display, WINDOW_WIDTH_SIZE,
		      WINDOW_HEIGHT_SIZE, app_id);
	init_gl(gstctx.display);

	gstctx.window = window;
	gstctx.display->window = window;

	fprintf(stderr, "display %p\n", gstctx.display);
	fprintf(stderr, "display->window %p\n", gstctx.display->window);
	fprintf(stderr, "window %p\n", window);

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

	fprintf(stdout, "pipeline %s\n", pipeline);

	/* parse the pipeline */
	gstctx.pipeline = gst_parse_launch(pipeline, &gerror);
	if (!gstctx.pipeline) {
		fprintf(stderr, "Could not create gstreamer pipeline.\n");
		destroy_display(gstctx.display);
		return -1;
	}

	gstctx.bus = gst_element_get_bus(gstctx.pipeline);
	gst_bus_add_signal_watch(gstctx.bus);

	g_signal_connect(gstctx.bus, "message::error", G_CALLBACK(error_cb), &gstctx);
	gst_bus_set_sync_handler(gstctx.bus, bus_sync_handler, &gstctx, NULL);
	gst_object_unref(gstctx.bus);

	gst_element_set_state(gstctx.pipeline, GST_STATE_PLAYING);

	while (running && ret != -1) {
		if (window->wait_for_configure) {
			ret = wl_display_dispatch(gstctx.display->display);
		} else {
			ret = wl_display_dispatch_pending(gstctx.display->display);
			redraw(window);
		}
	}

	gst_element_set_state(gstctx.pipeline, GST_STATE_NULL);

	destroy_window(window);
	destroy_display(gstctx.display);
	gst_object_unref(gstctx.pipeline);

	return 0;
}
