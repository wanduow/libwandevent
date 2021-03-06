/*
 * This file is part of libwandevent
 *
 * Copyright (c) 2009 The University of Waikato, Hamilton, New Zealand.
 *
 * Authors: Perry Lorier
 * 	    Shane Alcock
 *
 * All rights reserved.
 *
 * This code has been developed by the University of Waikato WAND research
 * group. For further information, please see http://www.wand.net.nz/
 *
 * libwandevent is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License (GPL) as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * libwandevent is distributed in the hope that it will be useful but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libwandevent; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Any feedback (bug reports, suggestions, complaints) should be sent to
 * contact@wand.net.nz
 *
 */

#include <sys/select.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <error.h>
#include <assert.h>
#include <sys/ioctl.h>

#include "epollhelper.h"
void set_epoll_event(struct epoll_event *epev, int fd,
                int flags) {

        epev->data.fd = fd;
        epev->events = 0;

        if (flags & EV_READ)  epev->events |= (EPOLLIN | EPOLLRDHUP);
        if (flags & EV_WRITE) epev->events |= EPOLLOUT;

}

struct epoll_event *create_epoll_event(wand_event_handler_t *ev_hdl,
                int fd, int flags) {

        struct epoll_event *epev = NULL;
        int ret = 0;

        epev = (struct epoll_event *)calloc(1, sizeof(struct epoll_event));
        if (epev == NULL)
                return NULL;

        set_epoll_event(epev, fd, flags);
        ret = epoll_ctl(ev_hdl->epoll_fd, EPOLL_CTL_ADD, fd, epev);

        if (ret < 0) {
                perror("epoll_ctl");
                fprintf(stderr, "Error adding fd %d to epoll\n", fd);
                free(epev);
                return NULL;
        }
        return epev;
}

void process_epoll_event(wand_event_handler_t *ev_hdl,
                struct epoll_event *ev)
{
        int fd, evtype;
        struct wand_fdcb_t *evcb;

        fd = ev->data.fd;
        evtype = ev->events;

        if (!ev_hdl->fd_events[fd])
                return;
        assert(ev_hdl->fd_events[fd]->fd==fd);

        evcb = ev_hdl->fd_events[fd];

        if (evcb->flags & EV_READ) {
		/* epoll can give us multiple events for a single fd, so
		 * we need to check for both new data and a hang up. Process
		 * the data first, then check for a client disconnect.
		 */
                if ((evtype & EPOLLIN) == EPOLLIN) {
                        evcb->callback(ev_hdl, fd, evcb->data, EV_READ);
                }

        	evcb = ev_hdl->fd_events[fd];
		if (evcb == NULL)
			return;
                if ((evtype & EPOLLHUP) || (evtype & EPOLLRDHUP))
                        evcb->callback(ev_hdl, fd, evcb->data, EV_READ);
        }

        /* An earlier callback may invalidate our pointer... */
        evcb = ev_hdl->fd_events[fd];
        if (evcb == NULL)
                return;

        if ((evtype & EPOLLOUT) == EPOLLOUT && (evcb->flags & EV_WRITE)) {

                evcb->callback(ev_hdl, fd, evcb->data, EV_WRITE);
        }

}

int calculate_epoll_delay(wand_event_handler_t *ev_hdl,
                struct wand_timer_t *next) {
        int ms_delay;

        if (next) {
                ms_delay = (next->expire.tv_sec - ev_hdl->monotonictime.tv_sec) * 1000;
                ms_delay += ((next->expire.tv_usec - ev_hdl->monotonictime.tv_usec) / 1000);
        } else {
                ms_delay = -1;
        }
        return ms_delay;
}

