#ifndef PMTILES_FILE_HPP
#define PMTILES_FILE_HPP

#include "pmtiles/pmtiles.hpp"

bool pmtiles_has_suffix(const char *filename);

void mbtiles_map_image_to_pmtiles(char *dbname);

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

#endif