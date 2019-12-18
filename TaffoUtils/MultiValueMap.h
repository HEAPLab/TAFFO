//===- MultiValueMap.h - Ordered Map from Values to Data --------*- C++ -*-===//
//
//   TAFFO - Tuning Assistant for Floating Point to Fixed Point Optimization
//
//===----------------------------------------------------------------------===//
//
// MultiValueMap is intended for mapping small lists of Value* to an arbitrary
// other type.  Any Value* V1 from the list can be used to get the associated
// V2, or the complete list of associated values.
// Every Value* can be contained in at most one list at a time.
// The map itself is ordered, and insertion of new associations can happen
// in any position.
//
//===----------------------------------------------------------------------===//

#ifndef MULTI_VALUE_MAP_H
#define MULTI_VALUE_MAP_H

#include <utility>
#include <memory>
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/ValueHandle.h"

namespace taffo {


template <typename KeyT, typename ValueT>
class MultiValueMapBase {
protected:
  // Type of the list of values associated to the same key (inner lists)
  using ValueListKeyT = llvm::SmallVector<llvm::AssertingVH<llvm::Value>, 2>;
  // Type of the list of the groups of values associated to each key
  using ValueListKeyStorageT = llvm::SmallVector<std::unique_ptr<ValueListKeyT>, 0>;
  // Index that maps each value to the list where it is contained
  using SingleValueIndexT = llvm::ValueMap<KeyT, ValueListKeyT *>;
  // Map from each value list to the mapped objects
  using ValueListToValueMapT = llvm::DenseMap<ValueListKeyT *, std::unique_ptr<ValueT>>;

  SingleValueIndexT Index;
  ValueListKeyStorageT ValueListsStorage;  // outer list
  ValueListToValueMapT ValueListsToValues;
};


template <typename BaseT>
class MultiValueMapIterator :
    public std::iterator<std::forward_iterator_tag,
                         std::pair<typename BaseT::key_type,
                                   typename BaseT::mapped_type>> {
  using KeyT = typename BaseT::key_type;
  using MappedT = typename BaseT::mapped_type;
  using InnerListT = typename BaseT::ValueListKeyT;
  using OuterListT = typename BaseT::ValueListKeyStorageT;
  using ValueT = typename std::pair<KeyT, MappedT>;
  
  BaseT& Parent;
  // if IOuterList is nullptr, lazy case
  typename OuterListT::iterator IOuterList;
  union {
    struct {
      typename InnerListT::iterator IInnerList;
      typename InnerListT::iterator InnerListEnd;
    } Full;
    struct {
      KeyT Key;
      InnerListT *IInnerList;
    } Lazy;
  } Data;
  
  // give MultiValueMap access to isLazy()
  template <typename T, typename U> friend class MultiValueMap;
  
private:
  inline bool isLazy() const {
    return IOuterList == nullptr;
  }
  
  /// *slowly* transform the lazy case in the full (non-lazy) case
  void makeFull() {
    if (!isLazy())
      return;
    InnerListT *InnerList = Data.Lazy.IInnerList;
    auto IInnerList = InnerList->begin();
    for (; IInnerList != InnerList->end(); IInnerList++) {
      if (*IInnerList == Data.Lazy.Key)
        break;
    }
    IOuterList = Parent.ValueListsStorage.begin();
    for (; IOuterList != Parent.ValueListsStorage.end(); IOuterList++) {
      if (IOuterList->get() == InnerList)
        break;
    }
    Data.Full.InnerListEnd = InnerList->end();
    Data.Full.IInnerList = IInnerList;
  }

public:
  MultiValueMapIterator(BaseT& Parent,
                        typename InnerListT::iterator IInnerList,
                        typename InnerListT::iterator InnerListEnd,
                        typename OuterListT::iterator IOuterList) :
      Parent(Parent), IOuterList(IOuterList),
      Data{.Full={IInnerList, InnerListEnd}} {}
      
  /// Lazy initializer
  MultiValueMapIterator(BaseT& Parent, KeyT key, InnerListT *IInnerList) :
      Parent(Parent), Data{.Lazy={key, IInnerList}} {}
      
  struct ValueTypeProxy {
    const KeyT first;
    MappedT& second;
    ValueTypeProxy *operator->() { return this; }
    operator std::pair<KeyT, MappedT>() const {
      return std::make_pair(first, second);
    }
  };
                        
  MultiValueMapIterator& operator++() {
    makeFull();
    Data.Full.IInnerList++;
    if (Data.Full.IInnerList == Data.Full.InnerListEnd) {
      IOuterList++;
      if (IOuterList != Parent.ValueListsStorage.end()) {
        Data.Full.IInnerList = IOuterList->begin();
        Data.Full.IInnerListEnd = IOuterList->begin()->end();
      } else {
        Data.Full.IInnerList = Data.Full.IInnerListEnd = nullptr;
      }
    }
    return *this;
  }
  MultiValueMapIterator operator++(int) {
    MultiValueMapIterator Res = *this;
    ++(*this);
    return Res;
  }
  bool operator==(const MultiValueMapIterator &RHS) const {
    if (!RHS.isLazy() && !isLazy()) {
      return Data.Lazy.Key == RHS.Data.Lazy.Key &&
             Data.Lazy.IInnerList == RHS.Data.Lazy.IInnerList;
    } else if (RHS.isLazy() != isLazy()) {
      // lazy status mismatched between RHS and LHS
      // since end iterators are never lazy, we optimize this case
      if (!isLazy()) {
        if (Data.Full.IInnerList == nullptr) // i'm the end() iterator
          return false;
      } else if (!RHS.isLazy()) {
        if (RHS.Data.Full.IInnerList == nullptr) // the RHS is the end()
          return false;
      }
      // not the easy case; un-lazy both iterators and then try again
      MultiValueMapIterator ThisCopy = *this;
      MultiValueMapIterator RHSCopy = RHS;
      ThisCopy.makeFull();
      RHSCopy.makeFull();
      return ThisCopy == RHSCopy;
    }
    return Data.Full.IInnerList == RHS.Data.Full.IInnerList &&
           IOuterList == IOuterList;
  }
  bool operator!=(const MultiValueMapIterator &RHS) const {
    return !(*this == RHS);
  }
  ValueTypeProxy operator*() const {
    InnerListT *VLTVKey;
    KeyT Key;
    if (isLazy()) {
      VLTVKey = Data.Lazy.IInnerList;
      Key = Data.Lazy.Key;
    } else {
      VLTVKey = IOuterList->get();
      Key = *(Data.Full.IInnerList);
    }
    MappedT& V = *((Parent.ValueListsToValues.find(VLTVKey))->second);
    return ValueTypeProxy{Key, V};
  }
  ValueTypeProxy operator->() const {
    return operator*();
  }
};


template <typename KeyT, typename ValueT>
class MultiValueMap : public MultiValueMapBase<KeyT, ValueT> {
  template <typename T> friend class MultiValueMapIterator;
  
  using ValueListKeyT =
      typename MultiValueMapBase<KeyT, ValueT>::ValueListKeyT;
  using ValueListKeyStorageT =
      typename MultiValueMapBase<KeyT, ValueT>::ValueListKeyStorageT;
  using SingleValueIndexT =
      typename MultiValueMapBase<KeyT, ValueT>::SingleValueIndexT;
  using ValueListToValueMapT =
      typename MultiValueMapBase<KeyT, ValueT>::ValueListToValueMapT;
  
public:
  using key_type = KeyT;
  using mapped_type = ValueT;
  using value_type = std::pair<KeyT, ValueT>;
  using size_type = unsigned;
  
  MultiValueMap() = default;
  
  // Disable copy & move because they're not implemented yet, and are currently
  // not worth implementing
  MultiValueMap(const MultiValueMap &) = delete;
  MultiValueMap(MultiValueMap &&) = delete;
  MultiValueMap &operator=(const MultiValueMap &) = delete;
  MultiValueMap &operator=(MultiValueMap &&) = delete;
  
  bool empty() const { return !this->ValueListsToValues.size(); }
  unsigned size() const { return this->Index.size(); }
  
  void clear() {
    this->ValueListsToValues.clear();
    this->Index.clear();
    this->ValueListsStorage.clear();
  }
  
  size_type count(const KeyT& K) const {
    return this->Index.find(K) != this->Index.end();
  }
  
  using iterator = MultiValueMapIterator<MultiValueMap<KeyT, ValueT>>;
  
  inline iterator begin() {
    if (this->ValueListsStorage.empty())
      return end();
    return iterator(*this,
        this->ValueListsStorage.begin()->get()->begin(),
        this->ValueListsStorage.begin()->get()->end(),
        this->ValueListsStorage.begin());
  }
  inline iterator end() {
    return iterator(*this,
        nullptr,
        nullptr,
        this->ValueListsStorage.end());
  }
  
  iterator find(const KeyT& K) {
    auto VListIt = this->Index.find(K);
    if (VListIt == this->Index.end())
      return end();
    return iterator(*this, K, *VListIt);
  }
  ValueT lookup(const KeyT& K) {
    auto VListIt = this->Index.find(K);
    if (VListIt == this->Index.end())
      return ValueT();
    return *(this->ValueListsToValues.find(*VListIt)->get());
  }
  
  // Inserts key,value pair into the map if the key isn't already in the map.
  // If the key is already in the map, it returns false and doesn't update the
  // value.
  // Note that this method will always create a new Value* group, regardless
  // of the mapped value.
  std::pair<iterator, bool> insert(iterator P,
      const std::pair<KeyT, ValueT> &KV) {
    if (this->Index.find(KV.first) != this->Index.end())
      return std::make_pair(P, false);
    P.makeFull();
    ValueListKeyT *NewVL = new ValueListKeyT{KV.first};
    auto NewI = this->ValueListsStorage.insert(P.IOuterList,
        std::unique_ptr<ValueListKeyT>(NewVL));
    this->Index[KV.first] = NewVL;
    this->ValueListsToValues[NewVL] =
      std::unique_ptr<ValueT>(new ValueT(KV.second));
    iterator ResIt = iterator(*this, NewVL->begin(), NewVL->end(), NewI);
    return std::make_pair(ResIt, true);
  }
};


}

#endif
