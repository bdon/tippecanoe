#ifndef PMTILES_HPP
#define PMTILES_HPP

#include <vector>
#include <fstream>

struct pmtilesv3_entry {
	uint64_t tile_id;
	uint64_t offset;
	uint32_t length;
	uint32_t run_length;

	pmtilesv3_entry(uint64_t _tile_id, uint64_t _offset, uint32_t _length, uint32_t _run_length)
	  : tile_id(_tile_id), offset(_offset), length(_length), run_length(_run_length) {
	}
};

struct pmtilesv3 {
	std::vector<pmtilesv3_entry> entries{};
	uint64_t offset = 0;
	std::ofstream ostream;
};

struct pmtiles_zxy {
	uint8_t z;
	uint32_t x;
	uint32_t y;

	pmtiles_zxy(int _z, int _x, int _y)
	  : z(_z), x(_x), y(_y) {
	}
};

pmtilesv3 *pmtilesv3_open(const char *filename, char **argv, int force);

void pmtilesv3_write_tile(pmtilesv3 *outfile, int z, int tx, int ty, const char *data, int size);

void pmtilesv3_finalize(pmtilesv3 *outfile);

uint64_t zxy_to_tileid(uint8_t z, uint32_t x, uint32_t y);

pmtiles_zxy tileid_to_zxy(uint64_t tile_id);

#endif