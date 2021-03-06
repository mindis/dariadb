#pragma once

#include "../storage.h"
#include "../utils/utils.h"
#include "chunk.h"
#include "chunk_container.h"
#include "cursor.h"

#include <vector>

namespace dariadb {
namespace storage {

const uint16_t OPENNED_PAGE_CACHE_SIZE = 10;
const size_t CHUNK_CACHE_SIZE = 1000;

class PageManager : public utils::NonCopy, public ChunkContainer, public MeasWriter {
public:
  struct Params {
    std::string path;
    uint32_t chunk_per_storage;
    uint32_t chunk_size;
    uint16_t openned_page_chache_size; /// max oppend pages in cache(readonly
                                       /// pages stored).
    size_t chunk_cache_size;
    Params(const std::string storage_path, size_t chunks_per_storage,
           size_t one_chunk_size) {
      path = storage_path;
      chunk_per_storage = uint32_t(chunks_per_storage);
      chunk_size = uint32_t(one_chunk_size);
      chunk_cache_size = CHUNK_CACHE_SIZE;
      openned_page_chache_size = OPENNED_PAGE_CACHE_SIZE;
    }
  };

protected:
  virtual ~PageManager();

  PageManager(const Params &param);

public:
  static void start(const Params &param);
  static void stop();
  void flush() override;
  static PageManager *instance();

  // bool append(const Chunk_Ptr &c) override;
  // bool append(const ChunksList &lst) override;

  // ChunkContainer
  bool minMaxTime(dariadb::Id id, dariadb::Time *minResult,
                  dariadb::Time *maxResult) override;
  ChunkLinkList chunksByIterval(const QueryInterval &query) override;
  Meas::Id2Meas valuesBeforeTimePoint(const QueryTimePoint &q) override;
  Cursor_ptr readLinks(const ChunkLinkList &links) override;

  // dariadb::storage::ChunksList get_open_chunks();
  size_t files_count() const;

  dariadb::Time minTime();
  dariadb::Time maxTime();

  append_result append(const Meas &value) override;

private:
  static PageManager *_instance;
  class Private;
  std::unique_ptr<Private> impl;
};

class ChunkCursor : public Cursor {
public:
  bool is_end() const override { return _chunk_iterator == chunks.end(); }
  void readNext(Callback *cbk) override {
    if (!is_end()) {
      cbk->call(*_chunk_iterator);
      ++_chunk_iterator;
    }
  }
  void reset_pos() override { _chunk_iterator = chunks.begin(); }

  ChunksList chunks;
  ChunksList::iterator _chunk_iterator;
};
}
}
