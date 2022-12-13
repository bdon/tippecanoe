#include <unordered_map>
#include <vector>
#include <fstream>
#include <string.h>
#include <algorithm>
#include <unistd.h>
#include <sqlite3.h>
#include "errors.hpp"
#include "pmtiles_file.hpp"
#include "mvt.hpp"
#include "write_json.hpp"

bool pmtiles_has_suffix(const char *filename) {
	if (filename == nullptr) {
		return false;
	}
	size_t lenstr = strlen(filename);
	if (lenstr < 8) {
		return false;
	}
	if (strncmp(filename + (lenstr - 8), ".pmtiles", 8) == 0) {
		return true;
	}
	return false;
}

static void out(json_writer &state, std::string k, std::string v) {
	state.json_comma_newline();
	state.json_write_string(k);
	state.json_write_string(v);
}

std::string metadata_to_pmtiles_json(metadata m) {
	std::string buf;
	json_writer state(&buf);

	state.json_write_hash();
	state.json_write_newline();

	out(state, "name", m.name);
	out(state, "description", m.description);
	if (m.attribution.size() > 0) {
		out(state, "attribution", m.attribution);
	}
	if (m.strategies_json.size() > 0) {
		state.json_comma_newline();
		state.json_write_string("strategies");
		state.json_write_json(m.strategies_json);
	}
	out(state, "generator", m.generator);
	out(state, "generator_options", m.generator_options);

	if (m.vector_layers_json.size() > 0) {
		state.json_comma_newline();
		state.json_write_string("vector_layers");
		state.json_write_json(m.vector_layers_json);
	}

	if (m.tilestats_json.size() > 0) {
		state.json_comma_newline();
		state.json_write_string("tilestats");
		state.json_write_json(m.tilestats_json);
	}

	state.json_write_newline();
	state.json_end_hash();
	state.json_write_newline();
	std::string compressed;
	compress(buf, compressed);
	return compressed;
}

std::tuple<std::string, std::string, int> build_root_leaves(const std::vector<pmtiles::entryv3> &entries, int leaf_size) {
	std::vector<pmtiles::entryv3> root_entries;
	std::string leaves_bytes;
	int num_leaves = 0;
	for (size_t i = 0; i <= entries.size(); i += leaf_size) {
		num_leaves++;
		int end = i + leaf_size;
		if (i + leaf_size > entries.size()) {
			end = entries.size();
		}
		std::vector<pmtiles::entryv3> subentries = {entries.begin() + i, entries.begin() + end};
		auto uncompressed_leaf = pmtiles::serialize_directory(subentries);
		std::string compressed_leaf;
		compress(uncompressed_leaf, compressed_leaf);
		root_entries.emplace_back(entries[i].tile_id, leaves_bytes.size(), compressed_leaf.size(), 0);
		leaves_bytes += compressed_leaf;
	}
	auto uncompressed_root = pmtiles::serialize_directory(root_entries);
	std::string compressed_root;
	compress(uncompressed_root, compressed_root);
	return std::make_tuple(compressed_root, leaves_bytes, num_leaves);
}

std::tuple<std::string, std::string, int> make_root_leaves(const std::vector<pmtiles::entryv3> &entries) {
	auto test_bytes = pmtiles::serialize_directory(entries);
	std::string compressed;
	compress(test_bytes, compressed);
	if (compressed.size() <= 16384 - 127) {
		return std::make_tuple(compressed, "", 0);
	}
	int leaf_size = 4096;
	while (true) {
		std::string root_bytes;
		std::string leaves_bytes;
		int num_leaves;
		std::tie(root_bytes, leaves_bytes, num_leaves) = build_root_leaves(entries, leaf_size);
		if (root_bytes.length() < 16384 - 127) {
			return std::make_tuple(root_bytes, leaves_bytes, num_leaves);
		}
		leaf_size *= 2;
	}
}

void mbtiles_map_image_to_pmtiles(char *fname, metadata m) {
	sqlite3 *db;

	if (sqlite3_open(fname, &db) != SQLITE_OK) {
		fprintf(stderr, "%s: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_SQLITE);
	}

	char *err = NULL;
	if (sqlite3_exec(db, "PRAGMA integrity_check;", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: integrity_check: %s\n", fname, err);
		exit(EXIT_SQLITE);
	}

	// materialize list of all tile IDs
	std::vector<uint64_t> tile_ids;

	{
		const char *sql = "SELECT zoom_level, tile_column, tile_row FROM map";
		sqlite3_stmt *stmt;

		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
			fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_SQLITE);
		}

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			int zoom = sqlite3_column_int(stmt, 0);
			int x = sqlite3_column_int(stmt, 1);
			int sorty = sqlite3_column_int(stmt, 2);
			int y = (1LL << zoom) - 1 - sorty;
			uint64_t res = pmtiles::zxy_to_tileid(zoom, x, y);
			tile_ids.push_back(res);
		}

		sqlite3_finalize(stmt);
	}

	std::sort(tile_ids.begin(), tile_ids.end());

	std::unordered_map<std::string, std::pair<unsigned long long, unsigned long>> hash_to_offset_len;
	std::vector<pmtiles::entryv3> entries;
	unsigned long long offset = 0;

	std::string tmpname = (std::string(fname) + ".tmp");

	// write tile data to tempfile in clustered order
	{
		const char *map_sql = "SELECT tile_id FROM map WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?";
		sqlite3_stmt *map_stmt;

		if (sqlite3_prepare_v2(db, map_sql, -1, &map_stmt, NULL) != SQLITE_OK) {
			fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_SQLITE);
		}

		const char *image_sql = "SELECT tile_data FROM images WHERE tile_id = ?";
		sqlite3_stmt *image_stmt;

		if (sqlite3_prepare_v2(db, image_sql, -1, &image_stmt, NULL) != SQLITE_OK) {
			fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_SQLITE);
		}

		std::ofstream tmp_ostream;

		tmp_ostream.open(tmpname.c_str(), std::ios::out | std::ios::binary);

		for (auto const &tile_id : tile_ids) {
			pmtiles::zxy zxy = pmtiles::tileid_to_zxy(tile_id);
			sqlite3_bind_int(map_stmt, 1, zxy.z);
			sqlite3_bind_int(map_stmt, 2, zxy.x);
			sqlite3_bind_int(map_stmt, 3, (1LL << zxy.z) - 1 - zxy.y);

			if (sqlite3_step(map_stmt) != SQLITE_ROW) {
				fprintf(stderr, "Corrupt mbtiles file: null entry in map table\n");
				exit(EXIT_SQLITE);
			}

			std::string hsh{reinterpret_cast<const char *>(sqlite3_column_text(map_stmt, 0))};

			if (hash_to_offset_len.count(hsh) > 0) {
				auto offset_len = hash_to_offset_len.at(hsh);
				if (entries.size() > 0 && tile_id == entries[entries.size() - 1].tile_id + 1 && entries[entries.size() - 1].offset == std::get<0>(offset_len)) {
					entries[entries.size() - 1].run_length++;
				} else {
					entries.emplace_back(tile_id, std::get<0>(offset_len), std::get<1>(offset_len), 1);
				}
			} else {
				sqlite3_bind_text(image_stmt, 1, hsh.data(), hsh.size(), SQLITE_STATIC);
				if (sqlite3_step(image_stmt) != SQLITE_ROW) {
					fprintf(stderr, "Corrupt mbtiles file: null entry in image table\n");
					exit(EXIT_SQLITE);
				}

				int len = sqlite3_column_bytes(image_stmt, 0);
				const char *blob = (const char *) sqlite3_column_blob(image_stmt, 0);

				tmp_ostream.write(blob, len);

				entries.emplace_back(tile_id, offset, len, 1);
				hash_to_offset_len.emplace(hsh, std::make_pair(offset, len));
				offset += len;

				sqlite3_reset(image_stmt);
				sqlite3_clear_bindings(image_stmt);
			}

			sqlite3_reset(map_stmt);
			sqlite3_clear_bindings(map_stmt);
		}
		tmp_ostream.close();
		sqlite3_finalize(map_stmt);
		sqlite3_finalize(image_stmt);
	}

	// finalize PMTiles archive.
	{
		std::sort(entries.begin(), entries.end(), pmtiles::entryv3_cmp);

		std::string root_bytes;
		std::string leaves_bytes;
		int num_leaves;
		std::tie(root_bytes, leaves_bytes, num_leaves) = make_root_leaves(entries);

		pmtiles::headerv3 header;

		header.min_zoom = m.minzoom;
		header.max_zoom = m.maxzoom;
		header.min_lon_e7 = m.minlon * 10000000;
		header.min_lat_e7 = m.minlat * 10000000;
		header.max_lon_e7 = m.maxlon * 10000000;
		header.max_lat_e7 = m.maxlat * 10000000;
		header.center_zoom = m.center_z;
		header.center_lon_e7 = m.center_lon * 10000000;
		header.center_lat_e7 = m.center_lat * 10000000;

		std::string json_metadata = metadata_to_pmtiles_json(m);

		sqlite3_close(db);

		header.clustered = 0x1;
		header.internal_compression = 0x2;  // gzip
		header.tile_compression = 0x2;	    // gzip

		if (m.format == "pbf") {
			header.tile_type = 0x1;
		} else if (m.format == "png") {
			header.tile_type = 0x2;
		} else {
			header.tile_type = 0x0;
		}

		header.root_dir_offset = 127;
		header.root_dir_bytes = root_bytes.size();

		header.json_metadata_offset = header.root_dir_offset + header.root_dir_bytes;
		header.json_metadata_bytes = json_metadata.size();
		header.leaf_dirs_offset = header.json_metadata_offset + header.json_metadata_bytes;
		header.leaf_dirs_bytes = leaves_bytes.size();
		header.tile_data_offset = header.leaf_dirs_offset + header.leaf_dirs_bytes;
		header.tile_data_bytes = offset;

		header.addressed_tiles_count = tile_ids.size();
		header.tile_entries_count = entries.size();
		header.tile_contents_count = hash_to_offset_len.size();

		std::ifstream tmp_istream(tmpname.c_str(), std::ios::in | std::ios_base::binary);

		std::ofstream ostream;
		ostream.open(fname, std::ios::out | std::ios::binary);

		auto header_str = header.serialize();
		ostream.write(header_str.data(), header_str.length());
		ostream.write(root_bytes.data(), root_bytes.length());
		ostream.write(json_metadata.data(), json_metadata.size());
		ostream.write(leaves_bytes.data(), leaves_bytes.length());
		ostream << tmp_istream.rdbuf();

		tmp_istream.close();
		unlink(tmpname.c_str());
		ostream.close();
	}
}

void collect_tile_entries(std::vector<pmtiles_zxy_entry> &tile_entries, const char *pmtiles_map, uint64_t dir_offset, uint64_t dir_len, uint64_t leaf_offset, uint64_t tile_data_offset) {
	std::string dir_s{pmtiles_map + dir_offset, dir_len};
	std::string decompressed_dir;
	decompress(dir_s, decompressed_dir);
	auto dir_entries = pmtiles::deserialize_directory(decompressed_dir);
	for (auto const &entry : dir_entries) {
		if (entry.run_length == 0) {
			collect_tile_entries(tile_entries, pmtiles_map, leaf_offset + entry.offset, leaf_offset + entry.length, leaf_offset, tile_data_offset);
		} else {
			for (uint64_t i = entry.tile_id; i < entry.tile_id + entry.run_length; i++) {
				pmtiles::zxy zxy = pmtiles::tileid_to_zxy(entry.tile_id);
				tile_entries.emplace_back(zxy.z, zxy.x, zxy.y, tile_data_offset + entry.offset, entry.length);
			}
		}
	}
}

struct {
	bool operator()(pmtiles_zxy_entry a, pmtiles_zxy_entry b) const {
		if (a.z != b.z) {
			return a.z < b.z;
		}
		if (a.x != b.x) {
			return a.x < b.x;
		}
		return a.y < b.y;
	}
} colmajor_cmp;

std::vector<pmtiles_zxy_entry> pmtiles_entries_colmajor(const char *pmtiles_map) {
	std::string header_s{pmtiles_map, 127};
	auto header = pmtiles::deserialize_header(header_s);

	std::vector<pmtiles_zxy_entry> tile_entries;

	collect_tile_entries(tile_entries, pmtiles_map, header.root_dir_offset, header.root_dir_bytes, header.leaf_dirs_offset, header.tile_data_offset);

	std::sort(tile_entries.begin(), tile_entries.end(), colmajor_cmp);

	return tile_entries;
}
