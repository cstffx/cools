#define _POSIX_C_SOURCE 200809L
#include <magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define PATH_BUF 4096

static magic_t magic_cookie = NULL;

typedef struct { char *path; long size; } Entry;
static Entry *entries = NULL;
static size_t n_entries = 0, cap_entries = 0;

static int is_text_file(const char *path)
{
	if (magic_cookie) {
		const char *mime = magic_file(magic_cookie, path);
		if (mime) {
			if (strncmp(mime, "text/", 5) == 0) return 1;
			if (strstr(mime, "json") || strstr(mime, "xml") ||
			    strstr(mime, "javascript") || strstr(mime, "script")) return 1;
			return 0;
		}
	}
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	unsigned char buf[8192];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	if (n == 0) return 1;
	for (size_t i = 0; i < n; i++) if (buf[i] == 0) return 0;
	int bad = 0;
	for (size_t i = 0; i < n; i++) {
		unsigned char c = buf[i];
		if (c < 32 && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') bad++;
	}
	if (bad * 10 > (int)n) return 0;
	return 1;
}

static int ext_matches(const char *path, char **exts, int n_ext)
{
	if (n_ext == 0) return 1;
	const char *dot = strrchr(path, '.');
	if (!dot || dot == path) return 0;
	char ext[256];
	strncpy(ext, dot + 1, sizeof(ext) - 1);
	ext[sizeof(ext) - 1] = '\0';
	for (char *p = ext; *p; p++) *p = (char)tolower((unsigned char)*p);
	for (int i = 0; i < n_ext; i++) {
		if (strcmp(ext, exts[i]) == 0) return 1;
	}
	return 0;
}

static void add_entry(const char *path, long size)
{
	if (n_entries == cap_entries) {
		cap_entries = cap_entries ? cap_entries * 2 : 256;
		entries = realloc(entries, cap_entries * sizeof(Entry));
	}
	entries[n_entries].path = strdup(path);
	entries[n_entries].size = size;
	n_entries++;
}

static void collect(const char *dir, char **exts, int n_ext, int text_only, long max_file)
{
	DIR *d = opendir(dir);
	if (!d) return;
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
		char path[PATH_BUF];
		snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
		struct stat st;
		if (lstat(path, &st) != 0) continue;
		if (S_ISDIR(st.st_mode)) {
			collect(path, exts, n_ext, text_only, max_file);
		} else if (S_ISREG(st.st_mode)) {
			if (max_file > 0 && st.st_size > max_file) continue;
			if (!ext_matches(path, exts, n_ext)) continue;
			if (text_only && !is_text_file(path)) continue;
			add_entry(path, st.st_size);
		}
	}
	closedir(d);
}

static int cmp_size(const void *a, const void *b)
{
	const Entry *ea = a, *eb = b;
	if (ea->size < eb->size) return -1;
	if (ea->size > eb->size) return 1;
	return strcmp(ea->path, eb->path);
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [DIRECTORY] (--budget BYTES | --tokens N) [options]\n", prog);
	fprintf(stderr, "  --budget N     max bytes of output\n");
	fprintf(stderr, "  --tokens N     max tokens (approx bytes=N*4)\n");
	fprintf(stderr, "  --ext LIST     only these extensions (comma-separated)\n");
	fprintf(stderr, "  --max-file N   skip files larger than N bytes\n");
	fprintf(stderr, "  --all          include non-text files too\n");
	fprintf(stderr, "Output: file sections until budget is reached\n");
}

int main(int argc, char **argv)
{
	const char *directory = ".";
	long budget = -1;
	int text_only = 1;
	long max_file = 0;
	char **exts = NULL;
	int n_ext = 0, cap_ext = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--budget") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			budget = strtol(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "--tokens") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			budget = strtol(argv[++i], NULL, 10) * 4;
		} else if (strcmp(argv[i], "--ext") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			char tmp[1024];
			strncpy(tmp, argv[++i], sizeof(tmp)-1);
			tmp[sizeof(tmp)-1] = '\0';
			char *tok = strtok(tmp, ",");
			while (tok) {
				if (n_ext == cap_ext) {
					cap_ext = cap_ext ? cap_ext * 2 : 8;
					exts = realloc(exts, cap_ext * sizeof(char *));
				}
				exts[n_ext++] = strdup(tok);
				tok = strtok(NULL, ",");
			}
		} else if (strcmp(argv[i], "--max-file") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			max_file = strtol(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "--all") == 0) {
			text_only = 0;
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
	if (budget <= 0) {
		usage(argv[0]);
		return 2;
	}

	magic_cookie = magic_open(MAGIC_MIME_TYPE);
	if (magic_cookie) magic_load(magic_cookie, NULL);

	char realdir[PATH_BUF];
	if (!realpath(directory, realdir)) {
		fprintf(stderr, "error: cannot resolve %s: %s\n", directory, strerror(errno));
		return 1;
	}
	collect(realdir, exts, n_ext, text_only, max_file);
	qsort(entries, n_entries, sizeof(Entry), cmp_size);

	long used = 0;
	for (size_t i = 0; i < n_entries; i++) {
		if (used + entries[i].size + 64 > budget) continue;
		/* print section */
		char header[PATH_BUF + 16];
		int hl = snprintf(header, sizeof(header), "===== %s =====\n", entries[i].path);
		if (hl < 0) continue;
		if (used + hl + entries[i].size + 1 > budget) continue;
		fputs(header, stdout);
		used += hl;
		FILE *f = fopen(entries[i].path, "rb");
		if (f) {
			char buf[65536];
			size_t n;
			while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
				fwrite(buf, 1, n, stdout);
				used += (long)n;
			}
			fclose(f);
		}
		fputc('\n', stdout);
		used += 1;
	}
	if (magic_cookie) magic_close(magic_cookie);
	return 0;
}
