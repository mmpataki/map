#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h> 
#include <string.h>
#include <libgen.h>
#include <stdlib.h>

#include "lib.h"

#define xprintf printf

struct archive_t *a_open(char *path, char *mode) {
	struct archive_t *arch = (struct archive_t*)malloc(sizeof(struct archive_t));
	arch->file = fopen(path, mode);
	arch->mode = mode[0] == 'r' ? MODE_READ : MODE_WRITE;
	arch->w_pos = 0;
	if(arch->file == NULL) {
		perror("ERR");
		return NULL;
	}
	if(arch->mode == MODE_READ){
		fread(&arch->head, sizeof(struct head_t), 1, arch->file);
	} else {
		arch->head.magic = MAGIC,
		arch->head.n_roots= 0, 
		arch->head.n_files= 0,
		arch->head.d_offset= 0;
	}
	return arch;
}

void a_close(struct archive_t *arch) {
	fclose(arch->file);
	free(arch);
}

// https://stackoverflow.com/a/57398580/4617334
long countfiles(char *path) {
    DIR *dir_ptr = NULL;
    struct dirent *direntp;
    char *npath;
    if( (dir_ptr = opendir(path)) == NULL ) return 0;

    long count=0;
    while( (direntp = readdir(dir_ptr))) {
        if(!strcmp(direntp->d_name,".") || !strcmp(direntp->d_name,".."))
			continue;
        switch (direntp->d_type) {
            case DT_DIR:
                npath = malloc(strlen(path) + strlen(direntp->d_name) + 2);
                sprintf(npath, "%s/%s", path, direntp->d_name);
                count += countfiles(npath);
                free(npath);
			case DT_REG:
                ++count;
        }
    }
    closedir(dir_ptr);
    return count;
}

long count_files(int num_files, char **paths) {
	long ret = num_files;
	for(int i=0; i<num_files; i++)
		ret += countfiles(paths[i]);
	return ret;
}

void a_dissect(char *path, dissect_callback_t h, dissect_callback_t d) {
	struct archive_t *arch = a_open(path, "r");
	h(&arch->head);
	struct dirent_t ent;
	for(long i=0; i<arch->head.n_files; i++) {
		fread(&ent, sizeof(struct dirent_t), 1, arch->file);
		d(&ent);
	}
	a_close(arch);
}

void dfs_and_write(struct archive_t *arch, char *path) {

    if (!path) return;

	struct stat path_stat;
	stat(path, &path_stat);
	
	if(!S_ISDIR(path_stat.st_mode) && !S_ISREG(path_stat.st_mode))
		return;

	struct dirent_t ent;
	ent.isdir = S_ISDIR(path_stat.st_mode);
	ent.start = arch->w_pos;
	ent.len = ent.isdir ? 0 : path_stat.st_size;
	strcpy(ent.name, basename(path));

	xprintf("adding %s : %s\n", ent.isdir ? "dir " : "file", path);

	if(!ent.isdir) {
		long meta = ftell(arch->file);
		fseek(arch->file, arch->w_pos, SEEK_SET);
		copy(arch->file, path);
		arch->w_pos += ent.len;
		fseek(arch->file, meta, SEEK_SET);
		fwrite(&ent, sizeof(struct dirent_t), 1, arch->file);
	} else {
		DIR *dir_ptr = NULL;
		struct dirent *direntp;
		char *npath;
		if( (dir_ptr = opendir(path)) == NULL ) return;

		long pos = ftell(arch->file);
		fseek(arch->file, sizeof(struct dirent_t), SEEK_CUR);

		while( (direntp = readdir(dir_ptr))) {
			if (strcmp(direntp->d_name,".")==0 || strcmp(direntp->d_name,"..")==0)
				continue;
			npath = malloc(strlen(path) + strlen(direntp->d_name) + 2);
			sprintf(npath, "%s/%s", path, direntp->d_name);
			dfs_and_write(arch, npath);
			ent.len++;
			free(npath);
		}

		long npos = ftell(arch->file);
		fseek(arch->file, pos, SEEK_SET);
		fwrite(&ent, sizeof(struct dirent_t), 1, arch->file);
		fseek(arch->file, npos, SEEK_SET);

		closedir(dir_ptr);
	}
}


struct archive_t *a_build(char *target, int num_files, char **paths) {
	
	long total_files = count_files(num_files, paths);

	struct archive_t *arch = a_open(target, "w");
	arch->head.magic = MAGIC;
	arch->head.n_roots = num_files; 
	arch->head.n_files = total_files;
	arch->head.d_offset = arch->w_pos = sizeof(struct head_t) + sizeof(struct dirent_t) * total_files;

	// write the header
	fwrite(&arch->head, sizeof(struct head_t), 1, arch->file);

	// write the rest
	for(int i = 0; i < num_files; i++)
		dfs_and_write(arch, paths[i]);

	a_close(arch);
}

static void dfs(struct archive_t *arch, int siblings, char *path, callback_t cb) {
	struct dirent_t ent;
	for(int i = 0; i < siblings; i++) {
		fread(&ent, sizeof(struct dirent_t), 1, arch->file);

		char *npath = (char *)malloc(strlen(path) + 2 + strlen(ent.name));
		sprintf(npath, "%s/%s", path, ent.name);

		FILE *fp = NULL;
		if(!ent.isdir) {
			fp = fdopen(dup(fileno(arch->file)), "r");
			fseek(fp, ent.start, SEEK_SET);
		}
		cb(npath, &ent, fp);
		if(fp)
			fclose(fp);
		if(ent.isdir)
			dfs(arch, ent.len, npath, cb);
		free(npath);
	}
}

void a_traverse(char *src_file, char *out_prefix, callback_t callback) {
	struct archive_t *arch = a_open(src_file, "r");
	dfs(arch, arch->head.n_roots, out_prefix, callback);
	a_close(arch);
}

void fcopy(FILE *tgt, FILE *src, long nbytes) {
	char buf[1024];
	long n_read;
	do {
		n_read = fread(buf, 1, min(nbytes, 1024), src);
		fwrite(buf, n_read, 1, tgt);
		nbytes -= n_read;
	} while(n_read == 1024 && nbytes);
}

void copy(FILE *tgt, char *src) {
	FILE *sfp = fopen(src, "r");
	fcopy(tgt, sfp, LONG_MAX);
	fclose(sfp);
}
