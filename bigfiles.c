#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
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
#define SNIFF 8192

static magic_t magic_cookie = NULL;

enum file_kind { KIND_ALL = 0, KIND_TEXT = 1, KIND_BINARY = 2, KIND_CODE = 3 };

static int classify_file(const char *path)
{
	if (magic_cookie) {
		const char *mime = magic_file(magic_cookie, path);
		if (mime) {
			if (strncmp(mime, "text/", 5) != 0) {
				if (strstr(mime, "json") || strstr(mime, "xml") ||
				    strstr(mime, "javascript") || strstr(mime, "script"))
					return KIND_CODE;
				return KIND_BINARY;
			}
			if (strstr(mime, "x-") || strstr(mime, "script") ||
			    strstr(mime, "python") || strstr(mime, "javascript") ||
			    strstr(mime, "json") || strstr(mime, "xml"))
				return KIND_CODE;
			return KIND_TEXT;
		}
	}
	FILE *f = fopen(path, "rb");
	if (!f) return KIND_BINARY;
	unsigned char buf[SNIFF];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	if (n == 0) return KIND_TEXT;
	for (size_t i = 0; i < n; i++) {
		if (buf[i] == 0) return KIND_BINARY;
	}
	int bad = 0;
	for (size_t i = 0; i < n; i++) {
		unsigned char c = buf[i];
		if (c < 32 && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v')
			bad++;
	}
	if (bad * 10 > (int)n) return KIND_BINARY;
	return KIND_TEXT;
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
	if (ferror(f)) { fclose(f); return -1; }
	fclose(f);
	if (total > 0 && last != '\n') lines++;
	return lines;
}

#define MAX_EXT 64
static char *ext_filters[MAX_EXT];
static int n_ext = 0;

#define MAX_MAGIC 32
static unsigned char magic_filters[MAX_MAGIC][64];
static size_t magic_lens[MAX_MAGIC];
static int n_magic = 0;

static int hexval(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static void add_magic(const char *s) {
	char tmp[1024];
	strncpy(tmp, s, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';
	char *tok = strtok(tmp, ",");
	while (tok && n_magic < MAX_MAGIC) {
		while (*tok == ' ') tok++;
		size_t L = strlen(tok);
		if (L == 0) { tok = strtok(NULL, ","); continue; }
		if (L % 2 != 0) {
			fprintf(stderr, "error: odd hex length: %s\n", tok);
			exit(2);
		}
		size_t blen = L / 2;
		if (blen > 64) { fprintf(stderr, "error: magic too long: %s\n", tok); exit(2); }
		for (size_t i = 0; i < blen; i++) {
			int hi = hexval(tok[i*2]);
			int lo = hexval(tok[i*2+1]);
			if (hi < 0 || lo < 0) {
				fprintf(stderr, "error: invalid hex: %s\n", tok);
				exit(2);
			}
			magic_filters[n_magic][i] = (unsigned char)((hi << 4) | lo);
		}
		magic_lens[n_magic] = blen;
		n_magic++;
		tok = strtok(NULL, ",");
	}
}

static int magic_matches(const char *path) {
	if (n_magic == 0) return 1;
	size_t maxlen = 0;
	for (int i = 0; i < n_magic; i++) if (magic_lens[i] > maxlen) maxlen = magic_lens[i];
	if (maxlen == 0) return 1;
	unsigned char buf[64];
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	size_t got = fread(buf, 1, maxlen, f);
	fclose(f);
	for (int i = 0; i < n_magic; i++) {
		if (magic_lens[i] <= got && memcmp(buf, magic_filters[i], magic_lens[i]) == 0)
			return 1;
	}
	return 0;
}

static void add_ext(const char *s)
{
	char tmp[256];
	strncpy(tmp, s, sizeof(tmp) - 1);
	tmp[sizeof(tmp) - 1] = '\0';
	char *tok = strtok(tmp, ",");
	while (tok && n_ext < MAX_EXT) {
		while (*tok == ' ' || *tok == '.') tok++;
		/* lowercase */
		for (char *p = tok; *p; p++) *p = (char)tolower((unsigned char)*p);
		if (*tok) {
			ext_filters[n_ext] = strdup(tok);
			n_ext++;
		}
		tok = strtok(NULL, ",");
	}
}

static int ext_matches(const char *path)
{
	if (n_ext == 0) return 1;
	const char *dot = strrchr(path, '.');
	if (!dot || dot == path) return 0;
	char ext[256];
	strncpy(ext, dot + 1, sizeof(ext) - 1);
	ext[sizeof(ext) - 1] = '\0';
	for (char *p = ext; *p; p++) *p = (char)tolower((unsigned char)*p);
	for (int i = 0; i < n_ext; i++) {
		if (strcmp(ext, ext_filters[i]) == 0) return 1;
	}
	return 0;
}

static void walk(const char *dir, long max_lines, int type_filter)
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
		size_t plen = strlen(dir) + strlen(ent->d_name) + 2;
		char *path = malloc(plen);
		if (!path) { fprintf(stderr, "error: out of memory\n"); continue; }
		snprintf(path, plen, "%s/%s", dir, ent->d_name);
		struct stat st;
		if (lstat(path, &st) != 0) {
			fprintf(stderr, "error: stat %s: %s\n", path, strerror(errno));
			free(path);
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			walk(path, max_lines, type_filter);
			free(path);
		} else if (S_ISREG(st.st_mode)) {
			if (!ext_matches(path)) { free(path); continue; }
			int kind = classify_file(path);
			if (type_filter != KIND_ALL && kind != type_filter) { free(path); continue; }
			if (!magic_matches(path)) { free(path); continue; }
			long lines = count_lines(path);
			if (lines >= 0 && lines > max_lines) {
				printf("%ld\t%s\n", lines, path);
			}
			free(path);
		}
	}
	closedir(d);
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [DIRECTORY] --max LINES [--type text|binary|code] [--ext EXT[,EXT...]] [--magic HEX[,HEX...]]\n", prog);
	fprintf(stderr, "  --max N        threshold of lines\n");
	fprintf(stderr, "  --type KIND    filter by kind: text, binary, code (default: all)\n");
	fprintf(stderr, "  --ext LIST     only files with these extensions (comma-separated)\n");
	fprintf(stderr, "  --magic LIST   only files whose prefix matches hex bytes (comma-separated)\n");
	fprintf(stderr, "Output: LINES<TAB>PATH\n");
}

static int parse_type(const char *s)
{
	if (strcmp(s, "text") == 0) return KIND_TEXT;
	if (strcmp(s, "binary") == 0) return KIND_BINARY;
	if (strcmp(s, "code") == 0) return KIND_CODE;
	if (strcmp(s, "all") == 0) return KIND_ALL;
	return -1;
}

int main(int argc, char **argv)
{
	long max_lines = -1;
	const char *directory = ".";
	int have_max = 0;
	int type_filter = KIND_ALL;
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
		} else if (strcmp(argv[i], "--type") == 0 || strcmp(argv[i], "-t") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			int k = parse_type(argv[++i]);
			if (k < 0) { fprintf(stderr, "error: invalid type: %s\n", argv[i]); return 2; }
			type_filter = k;
		} else if (strcmp(argv[i], "--ext") == 0 || strcmp(argv[i], "--extensions") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			add_ext(argv[++i]);
		} else if (strcmp(argv[i], "--magic") == 0 || strcmp(argv[i], "--magic-number") == 0) {
			if (i + 1 >= argc) { usage(argv[0]); return 2; }
			add_magic(argv[++i]);
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

	magic_cookie = magic_open(MAGIC_MIME_TYPE);
	if (magic_cookie) magic_load(magic_cookie, NULL);

	char *realdir = realpath(directory, NULL);
	if (!realdir) {
		fprintf(stderr, "error: cannot resolve %s: %s\n", directory, strerror(errno));
		return 1;
	}
	walk(realdir, max_lines, type_filter);
	free(realdir);
	if (magic_cookie) magic_close(magic_cookie);
	/* free ext filters */
	for (int i = 0; i < n_ext; i++) free(ext_filters[i]);
	return 0;
}
