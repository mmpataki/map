#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h> 
#include <string.h>
#include <libgen.h>
#include <stdlib.h>

#include "lib.h"

#define pattr(x, fmt, key) printf("\t" #key ": %" #fmt "", (x)->key);

void file_creator(char *path, struct dirent_t *ent, FILE *fp) {
	printf("creating %s : %s\n", ent->isdir ? "dir " : "file", path);

	if(ent->isdir) {
		struct stat st = {0};
		if (stat(path, &st) == -1) {
			mkdir(path, 0700);
		}
	} else {
		FILE *tgt = fopen(path, "w");
		fcopy(tgt, fp, ent->len);
	}
}

void lister(char *path, struct dirent_t *ent, FILE *fp) {
    printf("%c %-6ld %s\n", ent->isdir ? 'd' : ' ', ent->isdir ? 0 : ent->len, path);
}

void print_head(void *ptr) {
    struct head_t *a = (struct head_t*)ptr;
	printf("struct head_t (%ld) {", sizeof(struct head_t));
	pattr(a, x, magic);
	pattr(a, d, n_roots);
	pattr(a, ld, n_files);
	pattr(a, ld, d_offset);
	printf("}\n");
}

void print_dirent(void *ptr) {
    struct dirent_t *d = (struct dirent_t*)ptr;
	printf("struct dirent_t (%ld) {", sizeof(struct dirent_t));
	pattr(d, d, isdir);
	pattr(d, ld, start);
	pattr(d, ld, len);
	pattr(d, s, name);
	printf("}\n");
}

int main(int argc, char **argv) {

    if(argc < 2) {
        printf("Usage: %s [-l | -d] file.mzip\n", argv[0]);
        return 1;
    }
    
    int list = 0, debug = 0;
    char *file;

    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            list |= (!strcmp(argv[i], "-l"));
            debug |= (!strcmp(argv[i], "-d"));
        } else {
            file = argv[i];
        }
    }

    if(!file) {
        printf("Pass a .mzip file to unmap\n");
        return 1;
    }

    if(debug) {
        a_dissect(file, print_head, print_dirent);
    } else {
        callback_t cb = list ? lister : file_creator;
        char *prefix = list ? "" : ".";
        a_traverse(file, prefix, cb);
    }
}