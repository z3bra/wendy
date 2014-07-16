/*
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *                    Version 2, December 2004
 *
 * Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
 *
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 *
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

#define DEFAULT_FILE    "." /* defaults to current directory */
#define DEFAULT_CHECK   1   /* defaults to 1 second */

extern char **environ;

void
usage()
{
    fputs("usage: wendy [-C] [-D] [-M] [-m mask] [-l] [-f file] [-t timeout] [-q] "
          "[-e command [args] ..]\n"
          "\t-C           : raise creation events (default)\n"
          "\t-D           : raise deletion events\n"
          "\t-M           : raise modification events\n"
          "\t-m mask      : set mask manually (see -l))\n"
          "\t-l           : list events mask values\n"
          "\t-f file      : file to watch (everything is a file)\n"
          "\t-t timeout   : time between event check (in seconds)\n"
          "\t-v           : talk to me, program\n"
          "\t-e command   : command to launch (must be the last argument!)\n",
         stdout);
    exit(1);
}

void
list_events()
{
    fprintf(stdout,
            "IN_ACCESS ........ %u\n"
            "IN_MODIFY ........ %u\n"
            "IN_ATTRIB ........ %u\n"
            "IN_CLOSE_WRITE ... %u\n"
            "IN_CLOSE_NOWRITE . %u\n"
            "IN_OPEN .......... %u\n"
            "IN_MOVED_FROM .... %u\n"
            "IN_MOVED_TO ...... %u\n"
            "IN_CREATE ........ %u\n"
            "IN_DELETE ........ %u\n"
            "IN_DELETE_SELF ... %u\n"
            "IN_MOVE_SELF ..... %u\n"
            "IN_ALL_EVENTS .... %u\n"
            "IN_UNMOUNT ....... %u\n",
            IN_ACCESS,
            IN_MODIFY,
            IN_ATTRIB,
            IN_CLOSE_WRITE,
            IN_CLOSE_NOWRITE,
            IN_OPEN,
            IN_MOVED_FROM,
            IN_MOVED_TO,
            IN_CREATE,
            IN_DELETE,
            IN_DELETE_SELF,
            IN_MOVE_SELF,
            IN_ALL_EVENTS,
            IN_UNMOUNT
           );
    exit(0);
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
    int  fd, wd, len, i = 0, timeout = 0, ignore = 0, verbose = 0;
    uint32_t mask = 0;
    char buf[BUF_LEN];
    char *file = NULL, **cmd = NULL;
    struct inotify_event *ev;

    if ((argc == 2 && argv[1][0] == '-' && argv[1][1] == 'h')) usage();

    /* parse option given. see usage() above */
    for(i = 1; (i < argc) && (argv[i][0] == '-') && !ignore; i++) {
        switch (argv[i][1]) {
            case 'C': mask |= IN_CREATE; break;
            case 'D': mask |= IN_DELETE; break;
            case 'M': mask |= IN_MODIFY; break;
            case 'm': mask |= atoi(argv[++i]); break;
            case 'l': list_events(); break;
            case 'v': verbose += 1; break;
            case 'f': file = argv[++i]; break;
            case 't': timeout = atoi(argv[++i]); break;
            case 'e': cmd = &argv[++i]; ignore=1; break;
            default: usage();
        }
    }

    /* test given arguments */
    if (!file)      { file = DEFAULT_FILE; }
    if (!timeout)   { timeout = DEFAULT_CHECK; }

    /* get file descriptor */
    fd = inotify_init();
    if (fd < 0)
        perror("inotify_init");

add_watch:
    /* add a watcher on the file */
    wd  = inotify_add_watch(fd, file, mask);

    if (wd < 0) {
        perror("inotify_add_watch");
        exit(1);
    }

    if (verbose) {
        printf( "watching file %s with event mask %u\n", file, mask);
    }

    /* start looping */
    for (;;) {

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

            if (ev->mask & IN_IGNORED) {
                printf("watch removed, attemting to create another\n");
                goto add_watch;
            }

            if (verbose) {
                printf("%u: %s\n", ev->mask, ev->len ? ev->name : file);
            }

            /*
             * Do not do anything if no command given.
             * Also only execute the command if the file concerned by the event
             * is the one we're watching, or if we're not looking for a specific
             * file.
             *
             * If you don't undersand this sentence, don't worry. Me neither.
             * Just trust the if().
             */
            if (cmd) {
                /* OMG a new event ! Quick, raise an alert ! */
                if (!fork()) { execvpe(cmd[0], cmd, environ); }
            }

            /* jump to the next one */
            i += EVENT_SIZE + ev->len;
        }
        /* wait before queuing events */
        sleep(timeout);
    }

    return EXIT_SUCCESS;
}
