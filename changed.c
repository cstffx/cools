#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define PATH_BUF 4096

static int parse_date(const char *s, time_t *out)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	const char *fmts[] = {
		"%Y-%m-%dT%H:%M:%S",
		"%Y-%m-%d %H:%M:%S",
		"%Y-%m-%d %H:%M",
		"%Y-%m-%d",
		NULL
	};
	for (int i = 0; fmts[i]; i++) {
		char *r = strptime(s, fmts[i], &tm);
		if (r && *r == '\0') {
			*out = mktime(&tm);
			return 0;
		}
	}
	return -1;
}

static void walk(const char *dir, time_t cutoff)
{
	DIR *d = opendir(dir);
	if (!d) {
		fprintf(stderr, "error: cannot open directory %s: %s\n", dir, strerror(errno));
		return;
	}
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
			continue;
		char path[PATH_BUF];
		int len = snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
		if (len < 0 || (size_t)len >= sizeof(path)) {
			fprintf(stderr, "error: path too long: %s/%s\n", dir, ent->d_name);
			continue;
		}
		struct stat st;
		if (lstat(path, &st) != 0) {
			fprintf(stderr, "error: stat %s: %s\n", path, strerror(errno));
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			walk(path, cutoff);
		} else if (S_ISREG(st.st_mode)) {
			if (st.st_mtime >= cutoff) {
				printf("%ld\t%s\n", (long)st.st_mtime, path);
			}
		}
	}
	closedir(d);
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [DIRECTORY] (--days N | --since DATE)\n", prog);
	fprintf(stderr, "  --days N     files modified in the last N days\n");
	fprintf(stderr, "  --since DATE files modified since DATE (YYYY-MM-DD[ HH:MM[:SS]])\n");
	fprintf(stderr, "Output: MTIME_EPOCH<TAB>PATH\n");
}

int main(int argc, char **argv)
{
	const char *directory = ".";
	int have_cutoff = 0;
	time_t cutoff = 0;
	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--days") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			char *end = NULL;
			errno = 0;
			long v = strtol(argv[++i], &end, 10);
			if (errno != 0 || *end != '\0' || v < 0) {
				fprintf(stderr, "error: invalid --days value: %s\n", argv[i]);
				return 2;
			}
			cutoff = time(NULL) - v * 86400;
			have_cutoff = 1;
		} else if (strcmp(argv[i], "--since") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			if (parse_date(argv[++i], &cutoff) != 0) {
				fprintf(stderr, "error: invalid date: %s\n", argv[i]);
				return 2;
			}
			have_cutoff = 1;
		} else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "error: unknown option: %s\n", argv[i]);
			usage(argv[0]);
			return 2;
		} else {
			directory = argv[i];
		}
	}
	if (!have_cutoff) {
		usage(argv[0]);
		return 2;
	}

	char realdir[PATH_BUF];
	if (!realpath(directory, realdir)) {
		fprintf(stderr, "error: cannot resolve %s: %s\n", directory, strerror(errno));
		return 1;
	}
	walk(realdir, cutoff);
	return 0;
}
