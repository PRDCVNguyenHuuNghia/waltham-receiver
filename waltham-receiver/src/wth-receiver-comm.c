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
**                                                                            **
**  TARGET    : linux                                                         **
**                                                                            **
**  PROJECT   : waltham-receiver                                              **
**                                                                            **
**  PURPOSE   :  This file acts as interface to waltham IPC library           **
**                                                                            **
**                                                                            **
*******************************************************************************/
#include <sys/types.h>
#include <signal.h>

#include "wth-receiver-comm.h"
#include "wth-receiver-surface.h"
#include "wth-receiver-seat.h"
#include "wth-receiver-buffer.h"

#include <waltham-util.h>

extern uint16_t tcp_port;
extern const char *my_app_id;

void
client_post_out_of_memory(struct client *c)
{
	struct wth_display *disp;

	disp = wth_connection_get_display(c->connection);
	wth_object_post_error((struct wth_object *) disp, 1, "out of memory");
}

/*
 *  waltam ivi surface implementation
 */
static void
wthp_ivi_surface_destroy(struct wthp_ivi_surface * ivi_surface)
{
	struct ivisurface *ivisurf = wth_object_get_user_data((struct wth_object *)ivi_surface);
	struct client *client = ivisurf->appid->client;

	if (client) {
		client->pid_destroying = true;
		fprintf(stdout, "client pid_destroying to true for client %p pid %d\n",
				client, client->pid);
		if (kill(client->pid, SIGINT) < 0) {
			fprintf(stderr, "Failed to send SIGINT to child %d\n",
					client->pid);
		}
	}

	free(ivisurf);
}

static const struct wthp_ivi_surface_interface wthp_ivi_surface_implementation = {
	wthp_ivi_surface_destroy,
};


/**
 * app_id version
 */
static void
wthp_ivi_app_id_surface_create(struct wthp_ivi_app_id *ivi_application,
			       const char *app_id,
			       struct wthp_surface * wthp_surface,
			       struct wthp_ivi_surface *obj)
{
	struct surface *surface =
		wth_object_get_user_data((struct wth_object *)wthp_surface);
	struct application_id *appid =
		wth_object_get_user_data((struct wth_object *) ivi_application);
	pid_t cpid;

	struct ivisurface *ivisurf = zalloc(sizeof *ivisurf);
	if (!ivisurf) {
		return;
	}

	ivisurf->obj = obj;
	ivisurf->surf = surface;
	ivisurf->appid = appid;

	wthp_ivi_surface_set_interface(obj,
				       &wthp_ivi_surface_implementation, ivisurf);

	cpid = fork();
	if (cpid == -1) {
		fprintf(stderr, "Failed to fork()\n");
		exit(EXIT_FAILURE);
	}

	if (cpid == 0) {
		if (my_app_id)
			wth_receiver_weston_main(surface->shm_window, my_app_id, tcp_port);
		else
			wth_receiver_weston_main(surface->shm_window, app_id, tcp_port);
	} else {
		/* this is parent, in wthp_ivi_surface_destroy() we mark that the
		 * client should be waited for so wait4() will be blocked.
		 */
		appid->client->pid = cpid;
	}

}

static const struct wthp_ivi_app_id_interface wthp_ivi_app_id_implementation = {
	wthp_ivi_app_id_surface_create,
};

static void
client_bind_wthp_ivi_app_id(struct client *c, struct wthp_ivi_app_id *obj)
{
	struct application_id *app;

	app = zalloc(sizeof *app);
	if (!app) {
		client_post_out_of_memory(c);
		return;
	}

	app->obj = obj;
	app->client = c;
	wl_list_insert(&c->compositor_list, &app->link);

	wthp_ivi_app_id_set_interface(obj, &wthp_ivi_app_id_implementation, app);
}

/*
 * waltham registry implementation
 */
static void
registry_destroy(struct registry *reg)
{
	wthp_registry_free(reg->obj);
	wl_list_remove(&reg->link);
	free(reg);
}

static void
registry_handle_destroy(struct wthp_registry *registry)
{
	struct registry *reg = wth_object_get_user_data((struct wth_object *)registry);
	registry_destroy(reg);
}

static void
registry_handle_bind(struct wthp_registry *registry,
		     uint32_t name, struct wth_object *id,
		     const char *interface, uint32_t version)
{

	struct registry *reg = wth_object_get_user_data((struct wth_object *)registry);

	if (strcmp(interface, "wthp_compositor") == 0) {
		client_bind_compositor(reg->client, (struct wthp_compositor *)id);
	} else if (strcmp(interface, "wthp_blob_factory") == 0) {
		struct client *client = reg->client;
		struct seat *seat, *tmp, *get_seat;

		client_bind_blob_factory(reg->client, (struct wthp_blob_factory *)id);

		get_seat = NULL;
		wl_list_for_each_safe(seat, tmp, &client->seat_list, link) {
			get_seat = seat;
		}

		if (get_seat)
			seat_send_updated_caps(get_seat);
	} else if (strcmp(interface, "wthp_ivi_app_id") == 0) {
		client_bind_wthp_ivi_app_id(reg->client, (struct wthp_ivi_app_id *) id);
	} else if (strcmp(interface, "wthp_seat") == 0) {
		client_bind_seat(reg->client, (struct wthp_seat *)id);
	} else {
		wth_object_post_error((struct wth_object *)registry, 0,
				"%s: unknown name %u", __func__, name);
		wth_object_delete(id);
	}
}

static const struct wthp_registry_interface registry_implementation = {
	registry_handle_destroy,
	registry_handle_bind
};

/*
 * waltham display implementation
 */

static void
display_handle_client_version(struct wth_display *wth_display,
		uint32_t client_version)
{
	wth_object_post_error((struct wth_object *)wth_display, 0,
			"unimplemented: %s", __func__);
}

static void
display_handle_sync(struct wth_display * wth_display, struct wthp_callback * callback)
{
	wthp_callback_send_done(callback, 0);
	wthp_callback_free(callback);
}

static void
display_handle_get_registry(struct wth_display *wth_display,
			    struct wthp_registry *registry)
{
	struct client *c = wth_object_get_user_data((struct wth_object *)wth_display);
	struct registry *reg;

	reg = zalloc(sizeof *reg);
	if (!reg) {
		client_post_out_of_memory(c);
		return;
	}

	reg->obj = registry;
	reg->client = c;
	wl_list_insert(&c->registry_list, &reg->link);
	wthp_registry_set_interface(registry,
			&registry_implementation, reg);

	wthp_registry_send_global(registry, 1, "wthp_compositor", 4);
	wthp_registry_send_global(registry, 1, "wthp_ivi_app_id", 1);
	wthp_registry_send_global(registry, 1, "wthp_seat", 4);
	wthp_registry_send_global(registry, 1, "wthp_blob_factory", 4);

}

const struct wth_display_interface display_implementation = {
	display_handle_client_version,
	display_handle_sync,
	display_handle_get_registry
};

/*
 * utility functions
 */
static int
watch_ctl(struct watch *w, int op, uint32_t events)
{
	struct epoll_event ee;

	ee.events = events;
	ee.data.ptr = w;
	return epoll_ctl(w->receiver->epoll_fd, op, w->fd, &ee);
}

/**
* client_destroy
*
* Destroy client connection
*
* @param names        struct client *c
* @param value        client data
* @return             none
*/
void
client_destroy(struct client *c)
{
	struct region *region;
	struct compositor *comp;
	struct registry *reg;
	struct surface *surface;

	/* clean up remaining client resources in case the client
	 * did not.
	 */
	wl_list_last_until_empty(region, &c->region_list, link)
		region_destroy(region);

	wl_list_last_until_empty(comp, &c->compositor_list, link)
		compositor_destroy(comp);

	wl_list_last_until_empty(reg, &c->registry_list, link)
		registry_destroy(reg);

	wl_list_last_until_empty(surface, &c->surface_list, link)
		surface_destroy(surface);

	wl_list_remove(&c->link);
	watch_ctl(&c->conn_watch, EPOLL_CTL_DEL, 0);
	wth_connection_destroy(c->connection);
	free(c);
}

/*
 * functions to handle waltham client connections
 */
static void
connection_handle_data(struct watch *w, uint32_t events)
{

	struct client *c = container_of(w, struct client, conn_watch);
	int ret;

	if (events & EPOLLERR) {
		wth_error("Client %p errored out.\n", c);
		client_destroy(c);
		return;
	}

	if (events & EPOLLHUP) {
		wth_error("Client %p hung up.\n", c);
		client_destroy(c);
		return;
	}

	if (events & EPOLLOUT) {
		ret = wth_connection_flush(c->connection);
		if (ret == 0) {
			watch_ctl(&c->conn_watch, EPOLL_CTL_MOD, EPOLLIN);
		} else if (ret < 0 && errno != EAGAIN) {
			wth_error("Client %p flush error.\n", c);
			client_destroy(c);
			return;
		}
	}

	if (events & EPOLLIN) {
		ret = wth_connection_read(c->connection);
		if (ret < 0) {
			wth_error("Client %p read error.\n", c);
			client_destroy(c);
			return;
		}

		ret = wth_connection_dispatch(c->connection);
		if (ret < 0 && errno != EPROTO) {
			wth_error("Client %p dispatch error.\n", c);
			client_destroy(c);
			return;
		}
	}
}

/**
 * client_create
 *
 * Create new client connection
 *
 * @param srv                   receiver structure
 * @param wth_connection        Waltham connection handle
 * @return                      Pointer to client structure
 */
static struct client *
client_create(struct receiver *srv, struct wth_connection *conn)
{

	struct client *c;
	struct wth_display *disp;

	c = zalloc(sizeof *c);
	if (!c)
		return NULL;

	c->receiver = srv;
	c->connection = conn;

	c->conn_watch.receiver = srv;
	c->conn_watch.fd = wth_connection_get_fd(conn);
	c->conn_watch.cb = connection_handle_data;
	if (watch_ctl(&c->conn_watch, EPOLL_CTL_ADD, EPOLLIN) < 0) {
		free(c);
		return NULL;
	}


	wl_list_insert(&srv->client_list, &c->link);

	wl_list_init(&c->registry_list);
	wl_list_init(&c->compositor_list);
	wl_list_init(&c->seat_list);
	wl_list_init(&c->pointer_list);
	wl_list_init(&c->touch_list);
	wl_list_init(&c->region_list);
	wl_list_init(&c->surface_list);
	wl_list_init(&c->buffer_list);

	disp = wth_connection_get_display(c->connection);
	wth_display_set_interface(disp, &display_implementation, c);

	fprintf(stdout, "Client %p created\n", c);
	return c;
}


/**
* receiver_flush_clients
*
* write all the pending requests from the clients to socket
*
* @param names        struct receiver *srv
* @param value        socket connection info and client data
* @return             none
*/
void
receiver_flush_clients(struct receiver *srv)
{
	struct client *c, *tmp;
	int ret;

	wl_list_for_each_safe(c, tmp, &srv->client_list, link) {
		/* Flush out buffered requests. If the Waltham socket is
		 * full, poll it for writable too.
		 */
		ret = wth_connection_flush(c->connection);
		if (ret < 0 && errno == EAGAIN) {
			watch_ctl(&c->conn_watch, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
		} else if (ret < 0) {
			perror("Connection flush failed");
			client_destroy(c);
			return;
		}
	}
}

/**
* receiver_accept_client
*
* Accepts new waltham client connection and instantiates client structure
*
* @param names        struct receiver *srv
* @param value        socket connection info and client data
* @return             none
*/
void
receiver_accept_client(struct receiver *srv)
{
	struct client *client;
	struct wth_connection *conn;
	struct sockaddr_in addr;
	socklen_t len;

	len = sizeof(addr);
	conn = wth_accept(srv->listen_fd, (struct sockaddr *)&addr, &len);
	if (!conn) {
		wth_error("Failed to accept a connection.\n");
		return;
	}
	client = client_create(srv, conn);
	if (!client) {
		wth_error("Failed client_create().\n");
		return;
	}
}
