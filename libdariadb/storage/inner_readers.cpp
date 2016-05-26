#include "inner_readers.h"
#include "../flags.h"
#include <cassert>

using namespace dariadb;
using namespace dariadb::compression;
using namespace dariadb::storage;

class CursorReader : public dariadb::storage::Cursor::Callback {
public:
  Chunk_Ptr readed;
  CursorReader() { readed = nullptr; }
  void call(dariadb::storage::Chunk_Ptr &ptr) override { readed = ptr; }
};

InnerReader::InnerReader(dariadb::Flag flag, dariadb::Time from, dariadb::Time to)
    : _cursors{}, _flag(flag), _from(from), _to(to), _tp_readed(false) {
  is_time_point_reader = false;
  end = false;
}

void InnerReader::add(Cursor_ptr c) {
  std::lock_guard<std::mutex> lg(_locker);
  this->_cursors.push_back(c);
}

void InnerReader::add_tp(Chunk_Ptr c) {
  std::lock_guard<std::mutex> lg(_locker);
  this->_tp_chunks[c->info->first.id].push_back(c);
}

bool InnerReader::isEnd() const {
  return this->end && this->_tp_readed;
}

dariadb::IdArray InnerReader::getIds() const {
  return _ids;
}

void InnerReader::readNext(storage::ReaderClb *clb) {
  std::lock_guard<std::mutex> lg(_locker);

  if (!_tp_readed) {
    this->readTimePoint(clb);
  }
  std::shared_ptr<CursorReader> reader_clbk{new CursorReader};
  for (auto id : this->_ids) {
    for (auto ch : _cursors) {
      while (!ch->is_end()) {
        ch->readNext(reader_clbk.get());
        if (reader_clbk->readed == nullptr) {
          continue;
        }
        auto cur_ch = reader_clbk->readed;
        reader_clbk->readed = nullptr;
        auto ch_reader = cur_ch->get_reader();
        size_t read_count = 0;
        while (!ch_reader->is_end()) {
          auto sub = ch_reader->readNext();
          read_count++;
          if (sub.id == id) {
            if (check_meas(sub)) {
              clb->call(sub);
            }
          }
        }
      }
    }
  }
  end = true;
}

void InnerReader::readTimePoint(storage::ReaderClb *clb) {
  std::lock_guard<std::mutex> lg(_locker_tp);
  std::list<Chunk_Ptr> to_read_chunks{};
  for (auto cand_ch : _tp_chunks) {
    auto candidate = cand_ch.second.front();

    for (auto cur_chunk : cand_ch.second) {
      if (candidate->info->first.time < cur_chunk->info->first.time) {
        candidate = cur_chunk;
      }
    }
    to_read_chunks.push_back(candidate);
  }

  for (auto ch : to_read_chunks) {

    auto bw = std::make_shared<BinaryBuffer>(ch->bw->get_range());
    bw->reset_pos();
    CopmressedReader crr(bw, ch->info->first);

    Meas candidate = Meas::empty();
    bool found = false;
    // candidate = ch->info->first;
    ch->lock();
    for (size_t i = 0; i < ch->info->count; i++) {

      auto sub = crr.read();
      sub.id = ch->info->first.id;
      if ((sub.time <= _from) && (sub.time >= candidate.time) &&
          (sub.id == _ids.front())) {
        candidate = sub;
        found = true;
      }
      if (sub.time > _from) {
        break;
      }
    }
    ch->unlock();
    if (found && candidate.time <= _from) {
      // TODO make as options
      candidate.time = _from;

      clb->call(candidate);
      _tp_readed_times.insert(std::make_tuple(candidate.id, candidate.time));
    }
  }
  auto m = dariadb::Meas::empty();
  m.time = _from;
  m.flag = dariadb::Flags::_NO_DATA;
  for (auto id : _not_exist) {
    m.id = id;
    clb->call(m);
  }
  _tp_readed = true;
}

bool InnerReader::check_meas(const Meas &m) const {
  auto tmp = std::make_tuple(m.id, m.time);
  if (this->_tp_readed_times.find(tmp) != _tp_readed_times.end()) {
    return false;
  }
  using utils::inInterval;

  if (m.inFlag(_flag) && (m.inInterval(_from, _to))) {
    return true;
  }
  return false;
}

Reader_ptr InnerReader::clone() const {
  auto res = std::make_shared<InnerReader>(_flag, _from, _to);
  res->_cursors = _cursors;
  res->_tp_chunks = _tp_chunks;
  res->_flag = _flag;
  res->_from = _from;
  res->_to = _to;
  res->_tp_readed = _tp_readed;
  res->end = end;
  res->_not_exist = _not_exist;
  res->_tp_readed_times = _tp_readed_times;
  return res;
}
void InnerReader::reset() {
  end = false;
  _tp_readed = false;
  _tp_readed_times.clear();
  for (auto ch : _cursors) {
    ch->reset_pos();
  }
}

TP_Reader::TP_Reader() {
  _values_iterator = this->_values.end();
}

TP_Reader::~TP_Reader() {}

bool TP_Reader::isEnd() const {
  return _values_iterator == _values.end();
}

dariadb::IdArray TP_Reader::getIds() const {
  return _ids;
}

void TP_Reader::readNext(dariadb::storage::ReaderClb *clb) {
  if (_values_iterator != _values.end()) {
    clb->call(*_values_iterator);
    ++_values_iterator;
    return;
  }
}

Reader_ptr TP_Reader::clone() const {
  TP_Reader *raw = new TP_Reader;
  raw->_values = _values;
  raw->reset();
  return Reader_ptr(raw);
}

void TP_Reader::reset() {
  _values_iterator = _values.begin();
}
