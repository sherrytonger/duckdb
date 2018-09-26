//===----------------------------------------------------------------------===//
//
//                         DuckDB
//
// storage/write_ahead_log.hpp
//
// Author: Mark Raasveldt
//
//===----------------------------------------------------------------------===//

#pragma once

#include <vector>

#include "common/exception.hpp"
#include "common/helper.hpp"
#include "common/types/data_chunk.hpp"

namespace duckdb {

class Catalog;
class DuckDB;
class SchemaCatalogEntry;
class TableCatalogEntry;
class Transaction;
class TransactionManager;

#define WAL_FILE "duckdb.wal"

//! The type of WAL entry
typedef uint8_t wal_type_t;

struct WALEntry {
	static constexpr wal_type_t INVALID = 0;
	static constexpr wal_type_t DROP_TABLE = 1;
	static constexpr wal_type_t CREATE_TABLE = 2;
	static constexpr wal_type_t DROP_SCHEMA = 3;
	static constexpr wal_type_t CREATE_SCHEMA = 4;
	static constexpr wal_type_t INSERT_TUPLE = 5;
	static constexpr wal_type_t QUERY = 6;
	static constexpr wal_type_t WAL_FLUSH = 100;

	static bool TypeIsValid(wal_type_t type) {
		return type == WALEntry::WAL_FLUSH ||
		       (type >= WALEntry::DROP_TABLE && type <= WALEntry::QUERY);
	}

	wal_type_t type;
	uint32_t size;
};

struct WALEntryData {
	WALEntry entry;
	std::unique_ptr<uint8_t[]> data;
};

//! The WriteAheadLog (WAL) is a log that is used to provide durability. Prior
//! to committing a transaction it writes the changes the transaction made to
//! the database to the log, which can then be replayed upon startup in case the
//! server crashes or is shut down.
class WriteAheadLog {
  public:
	WriteAheadLog(DuckDB &database)
	    : initialized(false), database(database), wal_file(nullptr) {}
	~WriteAheadLog();

	bool IsInitialized() { return initialized; }

	//! Replay the WAL
	void Replay(std::string &path);
	//! Initialize the WAL in the specified directory
	void Initialize(std::string &path);

	void WriteCreateTable(TableCatalogEntry *entry);
	void WriteDropTable(TableCatalogEntry *entry);

	void WriteCreateSchema(SchemaCatalogEntry *entry);
	void WriteDropSchema(SchemaCatalogEntry *entry);

	void WriteInsert(std::string &schema, std::string &table, DataChunk &chunk);
	void WriteQuery(std::string &query);

	void Flush();

  private:
	template <class T> size_t WriteSize();
	size_t WriteSize(std::string &val);

	template <class T> void Write(T val, size_t &sz);
	void WriteString(std::string &val, size_t &sz);

	void WriteEntry(wal_type_t type, uint32_t size) {
		size_t sz = sizeof(WALEntry);
		Write<wal_type_t>(type, sz);
		Write<uint32_t>(size, sz);
	}
	void WriteEntry(WALEntry entry) { WriteEntry(entry.type, entry.size); }
	void WriteData(uint8_t *dataptr, size_t data_size, size_t &sz);

	bool initialized;

	DuckDB &database;
	FILE *wal_file;
};

template <> void WriteAheadLog::Write(std::string &val, size_t &sz);

} // namespace duckdb