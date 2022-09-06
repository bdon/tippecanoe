#ifndef PMTILES_HPP
#define PMTILES_HPP

#include <vector>
#include <fstream>
#include <map>
#include "mbtiles.hpp"

struct pmtilesv3_entry {
	uint64_t tile_id;
	uint64_t offset;
	uint32_t length;
	uint32_t run_length;

	pmtilesv3_entry() : tile_id(0), offset(0), length(0), run_length(0) {
	}

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

struct pmtilesv3_header {
	uint32_t root_dir_bytes;
	uint32_t json_metadata_bytes;
	uint64_t leaf_dirs_bytes;
	uint64_t leaf_dirs_offset; // the absolute offset of the leaf directory section.
	uint64_t tile_data_offset; // the absolute offset of the tile data section.
	uint64_t addressed_tiles_count; // sum(runlength) over all entries
	uint64_t tile_entries_count; // # of entries where runlength > 0
	uint64_t unique_tile_contents_count; // unique # of offsets where runlength > 0 (indicates unique)
	bool clustered;
	std::string directory_compression;
	std::string tile_compression;
	std::string tile_format;
	uint8_t min_zoom;
	uint8_t max_zoom;
	float min_lon;
	float min_lat;
	float max_lon;
	float max_lat;
	uint8_t center_zoom;
	float center_lon;
	float center_lat;

	std::string serialize();
};

std::string serialize_entries(const std::vector<pmtilesv3_entry>& entries);

std::vector<pmtilesv3_entry> deserialize_entries(const std::string &data);

pmtilesv3 *pmtilesv3_open(const char *filename, char **argv, int force);

void pmtilesv3_write_tile(pmtilesv3 *outfile, int z, int tx, int ty, const char *data, int size);

void pmtilesv3_write_metadata(pmtilesv3 *outfile, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector, const char *description, bool do_tilestats, std::map<std::string, std::string> const &attribute_descriptions, std::string const &program, std::string const &commandline);

void pmtilesv3_finalize(pmtilesv3 *outfile);

uint64_t zxy_to_tileid(uint8_t z, uint32_t x, uint32_t y);

pmtiles_zxy tileid_to_zxy(uint64_t tile_id);

#endif