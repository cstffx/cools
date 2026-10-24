#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define PATH_BUF 4096

static long count_lines(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "error: cannot open %s: %s\n", path, strerror(errno));
		return -1;
	}
	char buf[65536];
	size_t n;
	long lines = 0;
	int last = 0;
	size_t total = 0;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		total += n;
		for (size_t i = 0; i < n; i++) {
			if (buf[i] == '\n') lines++;
			last = (unsigned char)buf[i];
		}
	}
	if (ferror(f)) {
		fprintf(stderr, "error: read %s: %s\n", path, strerror(errno));
		fclose(f);
		return -1;
	}
	fclose(f);
	if (total > 0 && last != '\n') lines++;
	return lines;
}

static void walk(const char *dir, long max_lines)
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
			walk(path, max_lines);
		} else if (S_ISREG(st.st_mode)) {
			long lines = count_lines(path);
			if (lines >= 0 && lines > max_lines) {
				printf("%ld\t%s\n", lines, path);
			}
		}
	}
	closedir(d);
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [DIRECTORY] --max LINES [-l]\n", prog);
	fprintf(stderr, "       %s --max LINES [DIRECTORY] [-l]\n", prog);
	fprintf(stderr, "Lists files (recursively) whose line count exceeds LINES.\n");
	fprintf(stderr, "Output format: LINES<TAB>PATH (counts always shown)\n");
	fprintf(stderr, "  -l, --lines  accepted for compatibility\n");
}

int main(int argc, char **argv)
{
	long max_lines = -1;
	const char *directory = ".";
	int have_max = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--max") == 0 || strcmp(argv[i], "-m") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			char *end = NULL;
			errno = 0;
			long v = strtol(argv[++i], &end, 10);
			if (errno != 0 || *end != '\0' || v < 0) {
				fprintf(stderr, "error: invalid --max value: %s\n", argv[i]);
				return 2;
			}
			max_lines = v;
			have_max = 1;
		} else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--lines") == 0) {
			/* accepted for compatibility; line counts are always printed */
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
	if (!have_max) {
		usage(argv[0]);
		return 2;
	}

	char realdir[PATH_BUF];
	if (!realpath(directory, realdir)) {
		fprintf(stderr, "error: cannot resolve %s: %s\n", directory, strerror(errno));
		return 1;
	}
	walk(realdir, max_lines);
	return 0;
}
