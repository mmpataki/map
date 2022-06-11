#pragma once

#include <stdio.h>
#include <limits.h>

#define MAGIC 			(0xfafafafa)
#define MAX_FNAME_LEN 	64
#define MODE_READ 		(1)
#define MODE_WRITE 		(2)

struct dirent_t {
	char isdir;
	long start, len;
	char name[MAX_FNAME_LEN];
};

struct head_t {
	int magic;
	int n_roots;			// number of files under root
	long n_files;			// number of files
	long d_offset;			// data offset
};

struct archive_t {
	struct head_t head;
	FILE *file;
	int mode;
	long w_pos;
};

typedef void (*callback_t)(char *path, struct dirent_t *ent, FILE *fp);
typedef void (*dissect_callback_t)(void *ptr);

#define min(x, y) (x < y ? x : y)

// API
struct archive_t *a_open(char *path, char *mode);

void a_close(struct archive_t *arch);

struct archive_t *a_build(char *target, int num_files, char **paths);

void a_dissect(char *path, dissect_callback_t h, dissect_callback_t d);

void a_traverse(char *src_file, char *prefix, callback_t cb);

// util API
void fcopy(FILE *tgt, FILE *src, long nbytes);

void copy(FILE *tgt, char *src);