#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include "pmtiles.hpp"
#include <protozero/varint.hpp>
#include <iostream>
#include "mvt.hpp"

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
	outfile->entries.emplace_back(pmtilesv3_entry(zxy_to_tileid(z,tx,ty),outfile->offset,size,1));
	outfile->ostream.write(data,size);
	outfile->offset += size;
}

void pmtilesv3_finalize(pmtilesv3 *outfile) {
	fprintf(stderr, "offset: %llu\n", outfile->offset);
	fprintf(stderr, "entries: %lu\n", outfile->entries.size());

	outfile->ostream.close();
	delete outfile;
};

inline void rotate(int64_t n, int64_t &x, int64_t &y, int64_t rx, int64_t ry) {
  if (ry == 0) {
    if (rx == 1) {
        x = n-1 - x;
        y = n-1 - y;
    }
    int64_t t = x;
    x = y;
    y = t;
  }
}

pmtiles_zxy t_on_level(uint8_t z, uint64_t pos) {
    int64_t n = 1 << z;
    int64_t rx, ry, s, t = pos;
    int64_t tx = 0;
    int64_t ty = 0;

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

uint64_t zxy_to_tileid(uint8_t z, uint32_t x, uint32_t y) {
	uint64_t acc = 0;
  for (uint8_t t_z = 0; t_z < z; t_z++) acc += (0x1 << t_z) * (0x1 << t_z);
  int64_t n = 1 << z;
  int64_t rx, ry, s, d=0;
  int64_t tx = x;
  int64_t ty = y;
  for (s=n/2; s>0; s/=2) {
    rx = (tx & s) > 0;
    ry = (ty & s) > 0;
    d += s * s * ((3 * rx) ^ ry);
    rotate(s, tx, ty, rx, ry);
  }
  return acc + d;
}

// precondition: entries is sorted by tile_id
std::string serialize_entries(const std::vector<pmtilesv3_entry>& entries) {
	std::string data;

	protozero::write_varint(std::back_inserter(data), entries.size());

	uint64_t last_id = 0;
	for (auto const &entry : entries) {
		protozero::write_varint(std::back_inserter(data), entry.tile_id - last_id);
		last_id = entry.tile_id;
	}

	for (auto const &entry : entries) {
		protozero::write_varint(std::back_inserter(data), entry.run_length);
	}

	for (auto const &entry : entries) {
		protozero::write_varint(std::back_inserter(data), entry.length);
	}

	for (size_t i = 0; i < entries.size(); i++) {
		if (i > 0 && entries[i].offset == entries[i-1].offset + entries[i-1].length) {
			protozero::write_varint(std::back_inserter(data), 0);
		} else {
			protozero::write_varint(std::back_inserter(data), entries[i].offset+1);
		}
	}

	std::string compressed;
	compress(data,compressed);

	return compressed;
}

std::vector<pmtilesv3_entry> deserialize_entries(const std::string &data) {
	std::string decompressed;
	decompress(data,decompressed);

	const char *t = decompressed.data();
	const char *end = t + decompressed.size();

	uint64_t num_entries = protozero::decode_varint(&t,end);

	std::vector<pmtilesv3_entry> result;
	result.resize(num_entries);

	uint64_t last_id = 0;
	for (size_t i = 0; i < num_entries; i++) {
		uint64_t tile_id = last_id + protozero::decode_varint(&t,end);
		result[i].tile_id = tile_id;
		last_id = tile_id;
	}

	for (size_t i = 0; i < num_entries; i++) {
		result[i].run_length = protozero::decode_varint(&t,end);
	}

	for (size_t i = 0; i < num_entries; i++) {
		result[i].length = protozero::decode_varint(&t,end);
	}

	for (size_t i = 0; i < num_entries; i++) {
		uint64_t tmp = protozero::decode_varint(&t,end);

		if (i > 0 && tmp == 0) {
			result[i].offset = result[i-1].offset + result[i-1].length;
		} else {
			result[i].offset = tmp - 1;
		}
	}

	// assert the directory has been fully consumed
	if (t != end) {
		fprintf(stderr, "Error: malformed pmtiles directory\n");
		exit(EXIT_FAILURE);
  }

	return result;
}