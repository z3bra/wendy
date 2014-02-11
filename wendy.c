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
    fputs("usage: wendy [-C] [-D] [-M] [-d directory] [-f file] [-t timeout] "
          "-e command [arguments]\n"
          "\t-C           : raise creation events\n"
          "\t-D           : raise deletion events\n"
          "\t-M           : raise modification events\n"
          "\t-d directory : directory to watch\n"
          "\t-f file      : file to watch in the directory\n"
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
    char *dir = NULL, *file = NULL, **cmd = NULL;
    struct inotify_event *ev;

    if ((argc == 2 && argv[1][0] == '-' && argv[1][1] == 'h')) usage();

    /* parse option given. see usage() above */
    for(i = 1; (i + 1 < argc) && (argv[i][0] == '-') && !ignore; i++) {
        switch (argv[i][1]) {
            case 'C': mask |= IN_CREATE; break;
            case 'D': mask |= IN_DELETE; break;
            case 'M': mask |= IN_MODIFY; break;
            case 'f': file = argv[++i]; break;
            case 'd': dir = argv[++i]; break;
            case 't': timeout = atoi(argv[++i]); break;
            case 'e': cmd = &argv[++i]; ignore=1; break;
        }
    }

    /* test given arguments */
    if (!dir)       { dir = DEFAULT_FILE; }
    if (!timeout)   { timeout = DEFAULT_CHECK; }
    if (!mask)      { mask |= IN_CREATE; }

    /* get file descriptor */
    fd = inotify_init();
    if (fd < 0)
        perror("inotify_init");

    /* add a watcher on the file */
    wd  = inotify_add_watch(fd, dir, mask);

    if (wd < 0)
        perror("inotify_add_watch");

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

            if (ev->len > 0) {
                printf("event on file %s: %u\n", ev->name, ev->mask);
            }

            /*
             * do not do anything if no command given.
             * Also only execute the command if the file concerned by the event
             * is the one we're watching, or if we're not looking for a specific
             * file.
             *
             * If you don't undersand this sentence, don't worry. Me neither.
             * Just trust the if().
             */
            if (cmd && !(file && strncmp(file, ev->name, 255))) {

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
