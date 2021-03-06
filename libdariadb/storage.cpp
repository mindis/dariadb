#include "storage.h"
#include "flags.h"
#include "meas.h"
#include "storage/inner_readers.h"
#include <map>

using namespace dariadb;
using namespace dariadb::storage;

class InnerCallback : public dariadb::storage::ReaderClb {
public:
  InnerCallback(Meas::MeasList *output) { _output = output; }
  ~InnerCallback() {}
  void call(const Meas &m) { _output->push_back(m); }

  Meas::MeasList *_output;
};

class ByStepClbk : public dariadb::storage::ReaderClb {
public:
  ByStepClbk(dariadb::storage::ReaderClb *clb, dariadb::IdArray ids, dariadb::Time from,
             dariadb::Time to, dariadb::Time step) {
    _out_clbk = clb;
    _step = step;
    _from = from;
    _to = to;

    for (auto id : ids) {
      _last[id].id = id;
      _last[id].time = _from;
      _last[id].flag = dariadb::Flags::_NO_DATA;
      _last[id].src = dariadb::Flags::_NO_DATA;
      _last[id].value = 0;

      _isFirst[id] = true;

      _new_time_point[id] = from;
    }
  }
  ~ByStepClbk() {}

  void dump_first_value(const Meas &m) {
    if (m.time > _from) {
      auto cp = _last[m.id];
      for (dariadb::Time i = _from; i < m.time; i += _step) {
        _out_clbk->call(cp);
        cp.time = _last[m.id].time + _step;
        _last[m.id] = cp;
        _new_time_point[m.id] += _step;
      }
    }

    _last[m.id] = m;

    _out_clbk->call(_last[m.id]);
    _new_time_point[m.id] = (m.time + _step);
  }

  void call(const Meas &m) {
    if (_isFirst[m.id]) {
      _isFirst[m.id] = false;

      dump_first_value(m);
      return;
    }

    if (m.time < _new_time_point[m.id]) {
      _last[m.id] = m;
      return;
    }
    if (m.time == _new_time_point[m.id]) {
      _last[m.id] = m;
      dariadb::Meas cp{m};
      cp.time = _new_time_point[m.id];
      _new_time_point[m.id] = (m.time + _step);
      _out_clbk->call(cp);
      return;
    }
    if (m.time > _new_time_point[m.id]) {
      auto last_value = _last[m.id];
      auto new_time_point_value = _new_time_point[m.id];

      dariadb::Meas cp{last_value};
      // get all from _new_time_point to m.time  with step
      for (dariadb::Time i = new_time_point_value; i < m.time; i += _step) {
        cp.time = i;
        _out_clbk->call(cp);
        cp.time = last_value.time + _step;
        last_value = cp;
        new_time_point_value += _step;
      }

      _new_time_point[m.id] = new_time_point_value;

      if (m.time == new_time_point_value) {
        _out_clbk->call(m);
        _new_time_point[m.id] = (m.time + _step);
      }
      _last[m.id] = m;
      return;
    }
  }

  dariadb::storage::ReaderClb *_out_clbk;

  std::map<dariadb::Id, bool> _isFirst;
  std::map<dariadb::Id, dariadb::Meas> _last;
  std::map<dariadb::Id, dariadb::Time> _new_time_point;

  dariadb::Time _step;
  dariadb::Time _from;
  dariadb::Time _to;
};

void Reader::readAll(Meas::MeasList *output) {
  std::unique_ptr<InnerCallback> clb(new InnerCallback(output));
  this->readAll(clb.get());
}

void Reader::readAll(ReaderClb *clb) {
  while (!isEnd()) {
    readNext(clb);
  }
}

void Reader::readByStep(ReaderClb *clb, dariadb::Time from, dariadb::Time to,
                        dariadb::Time step) {
  std::unique_ptr<ByStepClbk> inner_clb(
      new ByStepClbk(clb, this->getIds(), from, to, step));

  this->reset();
  while (!isEnd()) {
    readNext(inner_clb.get());
  }
  for (auto kv : inner_clb->_new_time_point) {
    if (kv.second < to) {
      auto cp = inner_clb->_last[kv.first];
      cp.time = to;
      inner_clb->call(cp);
    }
  }
}

void Reader::readByStep(Meas::MeasList *output, dariadb::Time from, dariadb::Time to,
                        dariadb::Time step) {
  std::unique_ptr<InnerCallback> clb(new InnerCallback(output));
  this->readByStep(clb.get(), from, to, step);
}

append_result MeasWriter::append(const Meas::MeasArray &ma) {
  dariadb::append_result ar{};
  for (auto &m : ma) {
    ar = ar + this->append(m);
  }
  return ar;
}

append_result MeasWriter::append(const Meas::MeasList &ml) {
  dariadb::append_result ar{};
  for (auto &m : ml) {
    ar = ar + this->append(m);
  }
  return ar;
}
