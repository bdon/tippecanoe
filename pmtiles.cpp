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
	outfile->entries.emplace_back(1,outfile->offset,size,1);
	outfile->ostream.write(data,size);
	outfile->offset += size;
}

void pmtilesv3_finalize(pmtilesv3 *outfile) {
	fprintf(stderr, "offset: %llu\n", outfile->offset);
	fprintf(stderr, "entries: %lu\n", outfile->entries.size());
	outfile->ostream.close();
	delete outfile;
};

inline void rotate(int n, int32_t &x, int32_t &y, int rx, int ry) {
  if (ry == 0) {
    if (rx == 1) {
        x = n-1 - x;
        y = n-1 - y;
    }
    int t  = x;
    x = y;
    y = t;
  }
}

pmtiles_zxy t_on_level(uint8_t z, uint64_t pos) {
    int32_t n = 1 << z;
    int32_t rx, ry, s, t = pos;
    int32_t tx = 0;
    int32_t ty = 0;

    for (s=1; s<n; s*=2) {
      rx = 1 & (t/2);
      ry = 1 & (t ^ rx);
      rotate(s, tx, ty, rx, ry);
      tx += s * rx;
      ty += s * ry;
      t /= 4;
    }
    return pmtiles_zxy(z,tx,ty);
}

pmtiles_zxy tileid_to_zxy(uint64_t tile_id) {
	uint64_t acc = 0;
  uint8_t t_z = 0;
  while(true) {
    uint64_t num_tiles = (1 << t_z) * (1 << t_z);
    if (acc + num_tiles > tile_id) {
      return t_on_level(t_z, tile_id - acc);
    }
    acc += num_tiles;
    t_z++;
  }
}