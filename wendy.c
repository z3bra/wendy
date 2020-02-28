#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/wait.h>

#include "arg.h"
#include "queue.h"
#include "strlcpy.h"

#define EVSZ (sizeof(struct inotify_event) + NAME_MAX + 1)

struct watcher {
	int wd;
	char path[PATH_MAX];
	SLIST_ENTRY(watcher) entries;
};

SLIST_HEAD(watchers, watcher) head;

char *evname[] = {
	[IN_ACCESS] =        "ACCESS",
	[IN_MODIFY] =        "MODIFY",
	[IN_ATTRIB] =        "ATTRIB",
	[IN_CLOSE_WRITE] =   "CLOSE_WRITE",
	[IN_CLOSE_NOWRITE] = "CLOSE_NOWRITE",
	[IN_OPEN] =          "OPEN",
	[IN_MOVED_FROM] =    "MOVED_FROM",
	[IN_MOVED_TO] =      "MOVED_TO",
	[IN_CREATE] =        "CREATE",
	[IN_DELETE] =        "DELETE",
	[IN_DELETE_SELF] =   "DELETE_SELF",
	[IN_MOVE_SELF] =     "MOVE_SELF",
};

int verbose = 1;

void
usage(char *name)
{
	fprintf(stderr, "usage: %s [-v] [-m mask] [-f file] [cmd [args..]]\n", name);
	exit(1);
}

void
listwatcher(struct watchers *h)
{
	struct watcher *tmp;

	fprintf(stderr, "watchlist:\n");
	SLIST_FOREACH(tmp, h, entries)
		fprintf(stderr, "%s (%lu)\n", tmp->path, tmp->wd);
}


struct watcher *
getwatcher(struct watchers *h, int wd)
{
	struct watcher *tmp;

	SLIST_FOREACH(tmp, h, entries)
		if (tmp->wd == wd)
			return tmp;

	return NULL;

}

struct watcher *
watch(int fd, char *pathname, int mask)
{
	size_t len;
	struct watcher *w;

	w = malloc(sizeof(*w));
	if (!w)
		return NULL;

	/* store full inode path */
	realpath(pathname, w->path);
	w->wd = inotify_add_watch(fd, w->path, mask);
	if (w->wd < 0) {
		perror(pathname);
		return NULL;
	}

	SLIST_INSERT_HEAD(&head, w, entries);
	return w;
}

char *
wdpath(struct inotify_event *e)
{
	int event;
	struct watcher *w;
	static char pathname[PATH_MAX];

	if (e->len)
		snprintf(pathname, PATH_MAX, "%s/%s", w->path, e->name);
	else
		strlcpy(pathname, w->path, PATH_MAX);

	return pathname;

}


int
main (int argc, char **argv)
{
	int fd;
	uint8_t buf[EVSZ];
	uint32_t mask = IN_ALL_EVENTS;
	ssize_t len, off = 0;
	char *argv0 = NULL;
	struct watcher *tmp, *w;
	struct inotify_event *e;

	/* get file descriptor */
	fd = inotify_init();
	if (fd < 0)
		perror("inotify_init");

	ARGBEGIN {
	case 'm':
		mask = atoi(EARGF(usage(argv0)));
		break;
	case 'v':
		verbose++;
		break;
	case 'f':
		watch(fd, EARGF(usage(argv0)), mask);
		break;
	default:
		usage(argv0);
	} ARGEND;

	while (!SLIST_EMPTY(&head) && (off || (len=read(fd, buf, EVSZ))>0)) {

		/* cast buffer into the event structure */
		e = (struct inotify_event *) (buf + off);

		/* skip watch descriptors not in out list */
		if (!(w = getwatcher(&head, e->wd))) {
			inotify_rm_watch(fd, e->wd);
			goto skip;
		}

		if (verbose && e->mask & IN_ALL_EVENTS) {
			printf("%s\t%s\n", evname[e->mask & IN_ALL_EVENTS], wdpath(e));
			fflush(stdout);
		}

		/*
		 * IN_IGNORED is triggered when a file watched
		 * doesn't exists anymore. In this case we first try to
		 * add the watcher back, and if it fails, remove the
		 * watcher completely.
		 */
		if (e->mask & IN_IGNORED) {
			inotify_rm_watch(fd, e->wd);
			if ((w->wd = inotify_add_watch(fd, w->path, mask)) < 0)
				SLIST_REMOVE(&head, w, watcher, entries);
			break;
		}

skip:
		/* shift buffer offset when there's more events to read */
		off += sizeof(*e) + e->len;
		if (off >= len)
			off = 0;
	}

	close(fd);

	return 0;
}
