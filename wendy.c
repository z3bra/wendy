/*
 * Copyright (c) 2013 Willy 'z3bra' Goiffon <willy at mailoo dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>

/* definitions, defaults, bla bla blah */
#define EVENT_SIZE      (sizeof(struct inotify_event))
/* maximum number of event * queuing at the same time */
#define BUF_LEN         (512 * (EVENT_SIZE+16))

#define DEFAULT_directory   "/var/spool/mail/directory/new"
#define DEFAULT_CHECK   300 /* defaults to 5 minutes */

extern char **environ;

void
usage()
{
    fputs("usage: wendy [-C] [-D] [-M] [-d directory] [-t timeout] "
          "-e command [arguments]\n"
          "\t-C           : raise creation events\n"
          "\t-D           : raise deletion events\n"
          "\t-M           : raise modification events\n"
          "\t-d directory : directory to watch\n"
          "\t-t timeout   : time between event check (in seconds)\n"
          "\t-e command   : command to launch (must be the last argument!)\n",
         stdout);
    exit(1);
}

int
execvpe(const char *program, char **argv, char **envp)
{
    char **saved = environ;
    int rc;
    environ = envp;
    rc = execvp(program, argv);
    environ = saved;
    return rc;
}

int
main (int argc, char **argv)
{
    int  fd, wd, len, mask = 0, i = 0, timeout = 0, ignore = 0;
    char buf[BUF_LEN];
    char *directory = NULL, **cmd = NULL;
    struct inotify_event *ev;

    if ((argc == 2 && argv[1][0] == '-' && argv[1][1] == 'h')) usage();

    /* parse option given. see usage() above */
    for(i = 1; (i + 1 < argc) && (argv[i][0] == '-') && !ignore; i++) {
        switch (argv[i][1]) {
            case 'C': mask |= IN_CREATE; break;
            case 'D': mask |= IN_DELETE; break;
            case 'M': mask |= IN_MODIFY; break;
            case 'd': directory = argv[++i]; break;
            case 't': timeout = atoi(argv[++i]); break;
            case 'e': cmd = &argv[++i]; ignore=1; break;
        }
    }

    /* test given arguments */
    if (!directory) { directory = DEFAULT_directory; }
    if (!timeout)   { timeout = DEFAULT_CHECK; }
    if (!cmd)       { usage(); }
    if (!mask)      { mask |= IN_CREATE; }

    /* get file descriptor */
    fd = inotify_init();
    if (fd < 0)
        perror("inotify_init");

    /* add a watcher on the directory */
    wd  = inotify_add_watch(fd, directory, mask);

    if (wd < 0)
        perror("inotify_add_watch");

    /* start looping */
    while (1) {
        /* get every event raised, and queue them */
        len = read(fd, buf, BUF_LEN);

        if (!len || len < 0) {
            perror("read");
        }

        i = 0;


        /* treat all events queued */
        while (i < len) {

            /* get events one by one */
            ev = (struct inotify_event *) &buf[i]; 

            if (ev->len > 0) {
                printf("event on file %s: %u\n", ev->name, ev->mask);
            }

            /* OMG a new event ! Quick, raise an alert ! */
            if (!fork()) {
                execvpe(cmd[0], cmd, environ);
            }

            /* jump to the next one */
            i += EVENT_SIZE + ev->len;
        }
        /* wait before queuing events */
        sleep(timeout);
    }

    return EXIT_SUCCESS;
}
