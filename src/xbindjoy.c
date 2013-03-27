/* xbindjoy.c: event loop and some basic functions for the xbindjoy guile module
 *
 * Copyright 2013 Saul Reynolds-Haertle.
 *
 * This file is part of XBindJoy. 
 * 
 * XBindJoy is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. 
 * 
 * XBindJoy is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with XBindJoy.  If not, see <http://www.gnu.org/licenses/>. */


/* we're going to use a few things here that are specific to the GNU
 * standard libraries; the biggest one is ppoll, which I intend to use
 * eventually so that we can catch signals and release keys before we
 * exit. */
#define _GNU_SOURCE
#include <features.h>

#include <stdlib.h>
#include <libguile.h>
#include <fcntl.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <linux/joystick.h>
#include <assert.h>

#include "xbindjoy.h"
#include "joystick.h"
#include "sender.h"

#define BILLION 1000000000


/* C code structure: */
/*   xbindjoy contains the event loop and as little else as possible */
/*   joystick contains functions for handling joystick data */
/*   sender contains functions for sending x events */


void jsloop_advance(const struct timespec* cur_time, struct timespec* last_tick, struct timespec* next_tick) {
    next_tick->tv_sec = last_tick->tv_sec + (last_tick->tv_nsec + axis_time) / BILLION;
    next_tick->tv_nsec = (last_tick->tv_nsec + axis_time) % BILLION;
    if (next_tick->tv_sec < cur_time->tv_sec ||
        (next_tick->tv_sec == cur_time->tv_sec && next_tick->tv_nsec < cur_time->tv_nsec)) {
        /* we're far enough behind that advancing by a single tick doesn't catch us up */
        next_tick->tv_sec = cur_time->tv_sec;
        next_tick->tv_nsec = cur_time->tv_nsec;
    }
    last_tick->tv_sec = cur_time->tv_sec;
    last_tick->tv_nsec = cur_time->tv_nsec;
}
void jsloop_until(const struct timespec* cur_time, const struct timespec* next_tick, struct timespec* until_next_tick) {
    long int dsec = next_tick->tv_sec - cur_time->tv_sec;
    long int dnsec = dsec * BILLION + next_tick->tv_nsec - cur_time->tv_nsec;
    until_next_tick->tv_sec = dnsec / BILLION;
    until_next_tick->tv_nsec = dnsec % BILLION;
    if (until_next_tick->tv_sec < 0 || (until_next_tick->tv_sec == 0 && until_next_tick->tv_nsec < 0)) {
        until_next_tick->tv_sec = 0;
        until_next_tick->tv_nsec = 0;
    }
}


/* main event loop: call this and it will run forever, listening for
 * joystick inputs and running your code when it gets them */
void joystick_loop(SCM jsdevice, SCM jsmap) {
    /* use the input from guile to build up keymap. */
    keymap_t kmap = build_keymap_from_scm_alist(jsmap);

    /* open the joystick device */
    /* we're only waiting on one joystick at a time for now, so we're
     * going to use a single variable and hardcode the struct for the
     * poll. TODO: handle multiple joysticks. */
    char* jsdevice_c = scm_to_locale_string(jsdevice);
    int jsfd = open(jsdevice_c, O_RDONLY);
    free(jsdevice_c);

    /* set up variables for main loop */
    nfds_t npollfds = 1;
    struct pollfd* pollfds = malloc(npollfds * sizeof(struct pollfd));
    pollfds[0].fd = jsfd;
    pollfds[0].events = POLLIN;

    int last_poll_result;
    struct timespec cur_time;
    struct timespec last_tick;
    struct timespec next_tick;
    struct timespec until_next_tick = {0,0};
    clock_gettime(CLOCK_MONOTONIC, &cur_time);
    clock_gettime(CLOCK_MONOTONIC, &last_tick);
    clock_gettime(CLOCK_MONOTONIC, &next_tick);
    jsloop_advance(&cur_time, &last_tick, &next_tick);
    jsloop_until(&cur_time, &next_tick, &until_next_tick);

    int loops = 0;

    /* run the main loop */
    while(1) {
        /* wait either until the next tick comes around or we get an event */
        last_poll_result = ppoll(pollfds, npollfds, &until_next_tick, NULL);
        clock_gettime(CLOCK_MONOTONIC, &cur_time);

        if (last_poll_result > 0) { /* we got an event */
            /* read a js event out of the file descriptor */
            struct js_event e;
            for(unsigned int i = 0; i < npollfds; i++) {
                if (pollfds[i].revents & POLLIN) {
                    /* we got data! */
                    /* read and process the event, dispatch to the user's bindings */
                    int result = read (pollfds[i].fd, &e, sizeof(struct js_event));
                }
                if (pollfds[i].revents & (POLLNVAL | POLLHUP | POLLERR)) {
                    /* something else happened, print it and exit */
                    printf("joystick loop: bad result from polling js%d: %x\n", i, pollfds[i].revents);
                }
            }

            /* update how long to wait until the next tick */
            jsloop_until(&cur_time, &next_tick, &until_next_tick);
        }
        else if (last_poll_result == 0) { /* we timed out */
            /* compute a precise dt and send axis state to each axis handler */

            /* update how long to wait until the next tick */
            jsloop_advance(&cur_time, &last_tick, &next_tick);
            jsloop_until(&cur_time, &next_tick, &until_next_tick);
        }
        else { /* some kind of error */
            /* we were probably just interrupted by a signal. let the
             * program handle it as normal; we continue with our
             * looping. */
            int errsv = errno;
            char* errstring = strerror(errsv);
            printf("An error occurred while reading js: %s\n", errstring);
            free(errstring);
            /* update how long to wait until the next tick */
            jsloop_until(&cur_time, &next_tick, &until_next_tick);
        }
        printf("it: %d \tres: %d\t#c,s: %d\t|c,ns: %d\t#n,s: %d\t|n,ns: %d\t#u,s: %d\t|u,n: %d\n",
               loops, last_poll_result,
               (int)cur_time.tv_sec, (int)cur_time.tv_nsec,
               (int)next_tick.tv_sec, (int)next_tick.tv_nsec,
               (int)until_next_tick.tv_sec, (int)until_next_tick.tv_nsec);

        if(loops++ >= 99) break;
    }
    /* end loop - we should probably never reach this code */

    free(pollfds);              /* just to be safe */

    exit(0);
}

void inner_main(void* data, int argc, char** argv) {
    /* for going from joystick names to devices or vice versa */
    scm_c_define_gsubr("device->jsname", 1, 0, 0, get_joystick_name_wrapper);

    /* for sending x events to the screen */
    scm_c_define_gsubr("xbindjoy-send-key", 1, 0, 0, send_key_wrapper);
    scm_c_define_gsubr("xbindjoy-send-button", 1, 0, 0, send_button_wrapper);
    scm_c_define_gsubr("xbindjoy-send-mouserel", 2, 0, 0, send_mouserel_wrapper);
    scm_c_define_gsubr("xbindjoy-send-mouseabs", 2, 0, 0, send_mouseabs_wrapper);

    /* and the loop */
    scm_c_define_gsubr("xbindjoy-start", 2, 0, 0, joystick_loop);

    /* and finally start reading lisp */
    scm_shell(argc, argv);
}

int main(int argc, char** argv) {
    /* set up variables */
    axis_time = (1000 * 1000 * 1000);
    verbose = 1;
    display = XOpenDisplay(getenv("DISPLAY"));

    /* boot up guile */
    scm_boot_guile(argc, argv, inner_main, NULL);
}
