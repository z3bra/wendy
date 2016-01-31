#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/wait.h>

#include "arg.h"

/* definitions, defaults, bla bla blah */
#define EVENT_SIZE      (sizeof(struct inotify_event))

/* maximum number of event * queuing at the same time */
#define BUF_LEN         (512 * (EVENT_SIZE+16))

/* macro used to retrieve the path of the node triggering the event */
#define EVENT_PATH(e)   (e->len ? e->name : wd_path(e->wd))

/* environment variable names set by wendy to pass to the command */
#define ENV_PATH        "WENDY_INODE"
#define ENV_MASK        "WENDY_EVENT"

/* default value for the mask when -m is not specified */
#define DEFAULT_MASK    (IN_CLOSE_WRITE|IN_MODIFY)

/* timeout value used by default to queue the events */
#define DEFAULT_TIMEOUT 1

struct node_t {
	int wd;
	char path[PATH_MAX];
	struct node_t *next;
};

extern char **environ;

int verbose = 0, nb = 0;
struct node_t *head = NULL;

void
usage(char *name)
{
	fprintf(stderr, "usage: %s [-lq] [-m mask] [-f file] [-t timeout] "
	                "[cmd [args..]]\n", name);
	exit(1);
}

void
list_events()
{
	fprintf(stdout, "IN_ACCESS ........ %u\n"
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
			IN_UNMOUNT);
}

char *
read_filename(int fd)
{
	int i;
	char *fn = NULL, ch;

	fn = malloc(PATH_MAX);
	if (!fn)
		return NULL;

	for (i=0; read(fd, &ch, 1) > 0 && i < PATH_MAX; i++) {
		if (ch == 0 || ch == '\n') {
			*(fn + i + 1) = 0;
			return fn;
		} else {
			*(fn+i) = ch;
		}
	}

	return NULL;
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

struct node_t *
add_node(int wd, const char *path)
{
	struct node_t *n = NULL;

	n = malloc(sizeof(struct node_t));
	if (!n)
		return NULL;

	n->wd = wd;
	strncpy(n->path, path, PATH_MAX);
	n->next = head ? head : NULL;
	head = n;

	return n;
}

const char *
wd_path(int wd)
{
	struct node_t *n = head;

	while (n && n->wd != wd)
		n = n->next;

	return n ? n->path : "unknown";
}

int
watch_node(int fd, const char *path, uint32_t mask)
{
	int wd = -1;

	if (!path)
		return -1;

	/* add a watcher on the file */
	wd  = inotify_add_watch(fd, path, mask);
	if (wd < 0) {
		perror("inotify_add_watch");
		exit(1);
	}

	add_node(wd, path);
	nb++;

	return wd;
}

int
main (int argc, char **argv)
{
	int  fd, len, i = 0, timeout = 0;
	uint32_t mask = DEFAULT_MASK;
	char buf[BUF_LEN], strmask[8];
	char *fn = NULL, *argv0 = NULL;
	char **cmd = NULL;
	struct inotify_event *ev;

	/* get file descriptor */
	fd = inotify_init();
	if (fd < 0)
		perror("inotify_init");


	ARGBEGIN{
	case 'm':
		mask = atoi(EARGF(usage(argv0)));
		break;
	case 'l':
		list_events();
		return 0;
		break; /* NOT REACHED */
	case 'v':
		verbose++;
		break;
	case 'f':
		watch_node(fd, EARGF(usage(argv0)), mask);
		break;
	case 't':
		timeout = atoi(EARGF(usage(argv0)));
		break;
	default:
		usage(argv0);
	}ARGEND;

	if (argc > 0)
		cmd = argv;

	/* test given arguments */
	if (!timeout)   { timeout = DEFAULT_TIMEOUT; }

	if (!nb) {
		while ((fn = read_filename(0)) != NULL)
			watch_node(fd, fn, mask);

		free(fn);
	}

	/* start looping */
	while (nb>0) {
		/* get every event raised, and queue them */
		len = read(fd, buf, BUF_LEN);
		if (!len || len < 0)
			perror("read");

		i = 0;

		/* treat all events queued */
		while (i < len) {

			/* get events one by one */
			ev = (struct inotify_event *) &buf[i];

			if (verbose) {
				/*
				 * IN_IGNORED is triggered when a file watched
				 * doesn't exists anymore. In this case we
				 * decrement the number of files watched so
				 * that if there is none remaining, wendy will
				 * terminate.
				 */
				if (ev->mask & IN_IGNORED) {
					fprintf(stderr, "%s: removing watch\n", EVENT_PATH(ev));
					nb--;
				} else {
					printf("%u\t%s\n", ev->mask, EVENT_PATH(ev));
				}
				fflush(stdout);
			}

			/*
			 * Do not do anything if no command given.  Also only
			 * execute the command if the file concerned by the
			 * event is the one we're watching, or if we're not
			 * looking for a specific file.
			 *
			 * If you don't undersand this sentence, don't worry.
			 * Me neither.  Just trust the if().
			 */
			if (cmd) {
				/*
				 * OMG a new event! raise an alert!
				 * We first add two variables to the environment
				 * and then run the command.
				 * Also, double-forking.
				 */
				snprintf(strmask, 8, "%d", ev->mask);
				setenv(ENV_MASK, strmask, 1);
				setenv(ENV_PATH, EVENT_PATH(ev), 1);
				if (!fork())
				if (!fork()) execvpe(cmd[0], cmd, environ);
				else exit(0);
				else wait(NULL);
			}

			/* jump to the next one */
			i += EVENT_SIZE + ev->len;
		}
		/* wait before queuing events */
		if (nb > 0)
			sleep(timeout);
	}

	return EXIT_SUCCESS;
}
