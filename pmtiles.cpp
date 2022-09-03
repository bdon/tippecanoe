#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pmtiles.hpp"

pmtilesv3 *pmtilesv3_open(const char *filename, char **argv, int force) {
	pmtilesv3 *outfile = new pmtilesv3;

	struct stat st;
	if (force) {
		unlink(filename);
	} else {
		if (stat(filename, &st) == 0) {
			fprintf(stderr, "%s: %s: file exists\n", argv[0], filename);
			exit(EXIT_FAILURE);
		}
	}
	outfile->ostream.open(filename,std::ios::out | std::ios::binary);
	outfile->offset = 512000;
	for (uint64_t i = 0; i < outfile->offset; ++i) {
		char zero = 0;
		outfile->ostream.write(&zero,sizeof(char));
	}
	return outfile;
}

void pmtilesv3_write_tile(pmtilesv3 *outfile, int z, int tx, int ty, const char *data, int size) {
	outfile->entries.emplace_back(z,tx,ty,outfile->offset,size);
	outfile->ostream.write(data,size);
	outfile->offset += size;
}

void pmtilesv3_finalize(pmtilesv3 *outfile) {
	fprintf(stderr, "offset: %llu\n", outfile->offset);
	fprintf(stderr, "entries: %lu\n", outfile->entries.size());
	outfile->ostream.close();
	delete outfile;
};