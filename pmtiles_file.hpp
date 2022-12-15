#ifndef PMTILES_FILE_HPP
#define PMTILES_FILE_HPP

#include "pmtiles/pmtiles.hpp"
#include "mbtiles.hpp"

bool pmtiles_has_suffix(const char *filename);
void check_pmtiles(const char *filename, char **argv);

void mbtiles_map_image_to_pmtiles(char *dbname, metadata m, bool tile_compression, bool quiet, bool quiet_progress);

struct pmtiles_zxy_entry {
	long long z;
	long long x;
	long long y;
	uint64_t offset;
	uint32_t length;

	pmtiles_zxy_entry(long long _z, long long _x, long long _y, uint64_t _offset, uint32_t _length)
	    : z(_z), x(_x), y(_y), offset(_offset), length(_length) {
	}
};

std::vector<pmtiles_zxy_entry> pmtiles_entries_colmajor(const char *pmtiles_map);
sqlite3 *pmtilesmeta2tmp(const char *fname, const char *pmtiles_map);

#endif