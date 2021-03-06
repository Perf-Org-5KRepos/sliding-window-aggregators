#ifndef __TIMESTAMPED_DYNAMIC_FLATFIT_H__
#define __TIMESTAMPED_DYNAMIC_FLATFIT_H__

#include<vector>
#include<stack>
#include<iterator>
#include<algorithm>
#include<cassert>

#ifdef DEBUG
#define _IFDEBUG(x) x
#else
#define _IFDEBUG(x)
#endif

namespace timestamped_dynamic_flatfit {
  // grow and shrink factor: grow by a factor of THRES and
  // shrink by THRES when the load drops below a factor of 2*THRES
  const int THRES = 2;
  const int LOW_CAP = 2;
  template<typename valT, typename ptrT, typename timeT>
  class __AggT {
  public:
    valT _val;
    ptrT _next;
    timeT _timestamp;
    __AggT() {}
    __AggT(valT val_)
      : _val(val_) {}
    __AggT(valT val_, ptrT next_)
      : _val(val_), _next(next_) {}
    __AggT(valT val_, ptrT next_, timeT time_)
      : _val(val_), _next(next_), _timestamp(time_) {}
  };

  template<typename binOpFunc, typename Timestamp>
  class Aggregate {
  public:
    typedef uint32_t ptrT;
    typedef typename binOpFunc::In inT;
    typedef typename binOpFunc::Partial aggT;
    typedef typename binOpFunc::Out outT;
    typedef Timestamp timeT;
    typedef __AggT<aggT, ptrT, timeT> AggT;

    Aggregate(binOpFunc binOp_, aggT identE_)
      : _size(0), _buffer(LOW_CAP),
        _ever_evicted(false),
        _front(0), _back(-1),
        _binOp(binOp_), _identE(identE_) {}

    size_t size() { return _size; }

    void insert(timeT const& time, inT const& v) {
      if (_size + 1 > (int) _buffer.size())
        rescale_to(_buffer.size()*THRES, _size + 1);

      int prev = (_size > 0)?_back:-1;
      AggT vAgg = AggT(_binOp.lift(v), 0, time);
      ++_back; ++_size;
      _back %= _buffer.size();
      _buffer[_back] = vAgg;

      if (prev>=0)
        _buffer[prev]._next = _back;
    }

    void evict() {
      _front = (_front + 1)%_buffer.size();
      --_size;

      if (_size < (int) _buffer.size()/(2*THRES))
        rescale_to(_buffer.size()/THRES, _size);
    }


    outT query() {
      if (_size == 0)
        return _binOp.lower(_identE);

      // for non-empty cases
      std::stack<int> tracing_indices;
      for (int cur=_front;cur!=_back;cur=_buffer[cur]._next)
        tracing_indices.push(cur);

      aggT theSum = _identE;
      while (!tracing_indices.empty()) {
        int index = tracing_indices.top();
        theSum = _binOp.combine(_buffer[index]._val, theSum);
        _buffer[index] = AggT(theSum, _back, _buffer[index]._timestamp);
        tracing_indices.pop();
      }

      return _binOp.lower(_binOp.combine(theSum, _buffer[_back]._val));
    }
    timeT oldest() { return _buffer[_front]._timestamp; }

    timeT youngest() { return _buffer[_back]._timestamp; }

    friend inline std::ostream& operator<<(std::ostream& os, Aggregate const& t) {
      os << "(f=" << t._front << ",b=" << t._back << ",s=" << t._size << ") -- ";
      for (int i=0;i<t._buffer.size();i++) {
        os << "[" << t._buffer[i]._val << "; " << t._buffer[i]._next << "]";
      }
      return os;
    }
  private:
    int _size;
    std::vector<AggT> _buffer;
    bool _ever_evicted;
    int32_t _front, _back;

    // the binary operator deck
    binOpFunc _binOp;
    aggT _identE;

    void rescale_to(size_t new_size, size_t ensure_size) {
      new_size = std::max(new_size, (size_t) LOW_CAP);

      if (ensure_size > new_size) {
        std::cerr << "Baka!!" << std::endl;
        throw 1; // should never happen
      }

      std::vector<AggT> rescaled_buffer(new_size);

      // pack and reindex the next pointers
      size_t old_cap = _buffer.size();
      for (int index=0;index<_size;++index) {
        AggT & elt = _buffer[(_front + index)%old_cap];
        rescaled_buffer[index] = elt;
        int offset = (elt._next + old_cap - _front) % old_cap;
        rescaled_buffer[index]._next = offset;
      }

      // swap
      _buffer.swap(rescaled_buffer);

      // reset front and back
      // when _size == 0, we want _back = -1, so this has the right effect.
      _front = 0;
      _back = _size - 1;
    }
  };

  template <typename timeT, class BinaryFunction, class T>
  Aggregate<BinaryFunction, timeT> make_aggregate(BinaryFunction f, T elem) {
    return Aggregate<BinaryFunction, timeT>(f, elem);
  }

  template <typename BinaryFunction, typename timeT>
  struct MakeAggregate {
    template <typename T>
    Aggregate<BinaryFunction, timeT> operator()(T elem) {
      BinaryFunction f;
      return make_aggregate<timeT, BinaryFunction>(f, elem);
    }
  };
}
#endif
