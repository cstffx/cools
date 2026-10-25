#define _POSIX_C_SOURCE 200809L
#include <magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define PATH_BUF 4096
#define SNIFF 8192

static magic_t magic_cookie = NULL;

static int is_text_file(const char *path)
{
	if (magic_cookie) {
		const char *mime = magic_file(magic_cookie, path);
		if (mime) {
			if (strncmp(mime, "text/", 5) == 0) return 1;
			if (strstr(mime, "json") || strstr(mime, "xml") ||
			    strstr(mime, "javascript") || strstr(mime, "script"))
				return 1;
			return 0;
		}
	}
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	unsigned char buf[SNIFF];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	if (n == 0) return 1;
	int bad = 0;
	for (size_t i = 0; i < n; i++) {
		if (buf[i] == 0) return 0;
		unsigned char c = buf[i];
		if (c < 32 && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v')
			bad++;
	}
	if (bad * 10 > (int)n) return 0;
	return 1;
}

static long count_lines(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
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
	fclose(f);
	if (total > 0 && last != '\n') lines++;
	return lines;
}

static void walk(const char *dir)
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
			walk(path);
		} else if (S_ISREG(st.st_mode)) {
			long lines = count_lines(path);
			const char *type = is_text_file(path) ? "text" : "binary";
			printf("%ld\t%ld\t%s\t%s\n", lines, (long)st.st_size, type, path);
		}
	}
	closedir(d);
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [DIRECTORY]\n", prog);
	fprintf(stderr, "Output: LINES<TAB>BYTES<TAB>TYPE<TAB>PATH\n");
}

int main(int argc, char **argv)
{
	const char *directory = ".";
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
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
	char realdir[PATH_BUF];
	if (!realpath(directory, realdir)) {
		fprintf(stderr, "error: cannot resolve %s: %s\n", directory, strerror(errno));
		return 1;
	}
	magic_cookie = magic_open(MAGIC_MIME_TYPE);
	if (magic_cookie) magic_load(magic_cookie, NULL);
	walk(realdir);
	if (magic_cookie) magic_close(magic_cookie);
	return 0;
}
