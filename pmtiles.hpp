#ifndef PMTILES_HPP
#define PMTILES_HPP

#include <vector>
#include <fstream>

struct pmtilesv3 {
	std::vector<std::tuple<uint8_t,uint32_t,uint32_t,uint64_t,uint32_t>> entries{};
	uint64_t offset = 0;
	std::ofstream ostream;
};

pmtilesv3 *pmtilesv3_open(const char *filename, char **argv, int force);

void pmtilesv3_write_tile(pmtilesv3 *outfile, int z, int tx, int ty, const char *data, int size);

void pmtilesv3_finalize(pmtilesv3 *outfile);

#endif