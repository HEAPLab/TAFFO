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
// This data structure guarantees that the mapped values will never change
// allocation address during the lifetime of the object, no matter how it is
// mutated.
//
// All operations invalidate all item-wise standard iterators.
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
  
  BaseT *Parent;
  // IOuterList can be nullptr if not known. At end of list it must point to
  // the end of the list.
  typename OuterListT::iterator IOuterList;
  // nullptr only when at end of list
  //   Would be fully redundant with IOuterList if not for the unique_ptr
  // that stores each list.
  InnerListT *InnerList;
  // Can be nullptr if not known or if at end of list.
  typename InnerListT::iterator IInnerList;
  // nullptr only when at end of list
  KeyT Key;
  
  // In summary there are 3 types of lazy iterators, each of which allows
  // O(1) iteration of a different subset of the entire collection.
  // Converting from one lazy mode to the other is typically a O(n) process.
  // (1) IOuterList == null, IInnerList == null
  //     O(1) access just to one key
  // (2) IOuterList == null
  //     O(1) iteration to the list of Value* keys associated to the same value
  // (3) IOuterList != null
  //     O(1) iteration through the entire collection
  
  // give MultiValueMap access to isLazy()
  template <typename T, typename U> friend class MultiValueMap;
  
private:
  inline bool hasInnerListIt() const {
    return hasOuterListIt() || Key == nullptr || IInnerList != nullptr;
  }
  inline bool hasOuterListIt() const {
    return IOuterList != nullptr;
  }
  
  void makeInnerListIt() {
    if (hasInnerListIt())
      return;
    auto IInnerList = InnerList->begin();
    for (; IInnerList != InnerList->end(); IInnerList++) {
      if (*IInnerList == Key)
        break;
    }
  }
  void makeOuterListIt() {
    if (hasOuterListIt())
      return;
    makeInnerListIt();
    if (Key == nullptr) {
      IOuterList = Parent->ValueListsStorage.end();
    } else {
      IOuterList = Parent->ValueListsStorage.begin();
      for (; IOuterList != Parent->ValueListsStorage.end(); IOuterList++) {
        if (IOuterList->get() == InnerList)
          break;
      }
    }
  }
  
public:
  MultiValueMapIterator(BaseT& Parent,
                        typename InnerListT::iterator IInnerList,
                        InnerListT *InnerList,
                        typename OuterListT::iterator IOuterList) :
      Parent(&Parent), IOuterList(IOuterList), InnerList(InnerList),
      IInnerList(IInnerList) {
    assert(InnerList ? IInnerList != InnerList->end() : true);
    if (InnerList)
      Key = *IInnerList;
    else
      Key = nullptr;
  }
  /// Lazy initializer
  MultiValueMapIterator(BaseT& Parent, KeyT Key, InnerListT *InnerList) :
      Parent(&Parent), IOuterList(nullptr), InnerList(InnerList),
      IInnerList(IInnerList), Key(Key) {}
  MultiValueMapIterator(MultiValueMapIterator const &Other) = default;
  MultiValueMapIterator(MultiValueMapIterator&&) = default;
  
  MultiValueMapIterator& operator=(const MultiValueMapIterator& Other) {
    Parent = Other.Parent;
    IOuterList = Other.IOuterList;
    InnerList = Other.InnerList;
    IInnerList = Other.IInnerList;
    Key = Other.Key;
    return *this;
  };
      
  struct ValueTypeProxy {
    const KeyT first;
    MappedT& second;
    ValueTypeProxy *operator->() { return this; }
    operator std::pair<KeyT, MappedT>() const {
      return std::make_pair(first, second);
    }
  };
                        
  MultiValueMapIterator& operator++() {
    makeInnerListIt();
    IInnerList++;
    if (IInnerList == InnerList->end()) {
      makeOuterListIt();
      IOuterList++;
      if (IOuterList == Parent->ValueListsStorage.end()) {
        IInnerList = nullptr;
        InnerList = nullptr;
        Key = nullptr;
        return *this;
      }
      IInnerList = IOuterList->get()->begin();
      InnerList = IOuterList->get();
      Key = *IInnerList;
      return *this;
    }
    Key = *IInnerList;
    return *this;
  }
  MultiValueMapIterator operator++(int) {
    MultiValueMapIterator Res = *this;
    ++(*this);
    return Res;
  }
  
  MultiValueMapIterator& skip() {
    makeOuterListIt();
    IOuterList++;
    if (IOuterList != Parent->ValueListsStorage.end()) {
      IInnerList = IOuterList->get()->begin();
      InnerList = IOuterList->get();
      Key = *IInnerList;
      return *this;
    }
    IInnerList = nullptr;
    InnerList = nullptr;
    Key = nullptr;
    return *this;
  }
  
  bool operator==(const MultiValueMapIterator &RHS) const {
    if (this == &RHS)
      return true;
    return RHS.Key == Key;
  }
  bool operator!=(const MultiValueMapIterator &RHS) const {
    return !(*this == RHS);
  }
  ValueTypeProxy operator*() const {
    MappedT& V = *((Parent->ValueListsToValues.find(InnerList))->second);
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
        this->ValueListsStorage.begin()->get(),
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
  
  /// Inserts key,value pair into the map if the key isn't already in the map.
  /// Note that this method will always create a new Value* group, regardless
  /// of the mapped value.
  /// @returns pair(iterator pointing to the inserted pair, true) in case of
  ///   success; otherwise -- if the key is already in the map --
  ///   pair(the given iterator, false).
  std::pair<iterator, bool> insert(iterator P,
        const std::pair<KeyT, ValueT> &KV) {
    if (this->Index.find(KV.first) != this->Index.end())
      return std::make_pair(P, false);
    P.makeOuterListIt();
    ValueListKeyT *NewVL = new ValueListKeyT{KV.first};
    auto NewI = this->ValueListsStorage.insert(P.IOuterList,
        std::unique_ptr<ValueListKeyT>(NewVL));
    this->Index[KV.first] = NewVL;
    this->ValueListsToValues[NewVL] =
      std::unique_ptr<ValueT>(new ValueT(KV.second));
    iterator ResIt = iterator(*this, NewVL->begin(), NewVL, NewI);
    return std::make_pair(ResIt, true);
  }
  
  /// Adds a key to an existing key list / value association.
  /// If P points between two keys associated to the same value, K is associated
  /// to that value. If P points between a left key and a right key associated
  /// to different values, K will be added to the list of the right key.
  /// This method cannot be used at the end of the collection.
  /// @returns pair(iterator pointing to the inserted pair, true) in case of
  ///   success; otherwise -- if the key is already in the map or the iterator
  ///   is invalid -- pair(the given iterator, false).
  std::pair<iterator, bool> insertRight(iterator P, const KeyT& K) {
    if (P == end())
      return std::make_pair(P, false);
    if (this->Index.find(K) != this->Index.end())
      return std::make_pair(P, false);
    P.makeInnerListIt();
    P.IInnerList = P.InnerList->insert(P.IInnerList, K);
    P.Key = K;
    this->Index[K] = P.InnerList;
    return std::make_pair(P, true);
  }
  /// Adds a key to an existing key list / value association.
  /// If P points between two keys associated to the same value, K is associated
  /// to that value. If P points between a left key and a right key associated
  /// to different values, K will be added to the list of the left key.
  /// This method cannot be used at the beginning of the collection.
  /// @returns pair(iterator pointing to the inserted pair, true) in case of
  ///   success; otherwise -- if the key is already in the map or the iterator
  ///   is invalid -- pair(the given iterator, false).
  std::pair<iterator, bool> insertLeft(iterator P, const KeyT& K) {
    if (P == begin())
      return std::make_pair(P, false);
    if (this->Index.find(K) != this->Index.end())
      return std::make_pair(P, false);
    P.makeInnerListIt();
    if (!P.InnerList || P.IInnerList == P.InnerList->begin()) {
      // go back to the end of the previous list
      P.makeOuterListIt();
      P.IOuterList--;
      P.InnerList = P.IOuterList->get();
      P.IInnerList = P.InnerList->end();
    }
    P.IInnerList = P.InnerList->insert(P.IInnerList, K);
    P.Key = K;
    this->Index[K] = P.InnerList;
    return std::make_pair(P, true);
  }
  
  iterator eraseAll(iterator I) {
    I.makeOuterListIt();
    for (auto K: *(I.InnerList)) {
      this->Index.erase(K);
    }
    this->ValueListsStorage.erase(I.IOuterList);
    this->ValueListsToValues.erase(I.InnerList);
  }
  bool eraseAll(const KeyT& K) {
    iterator KI = find(K);
    if (KI == end())
      return false;
    eraseAll(KI);
    return true;
  }
  
  void erase(iterator I) {
    I.makeInnerListIt();
    if (I.InnerList->size() == 1) {
      // obliterate the entire list
      I.makeOuterListIt();
      this->ValueListsStorage.erase(I.IOuterList);
      this->ValueListsToValues.erase(I.InnerList);
    } else {
      // just remove the value
      I.InnerList->erase(I.IInnerList);
    }
    this->Index.erase(I.Key);
  }
  bool erase(const KeyT& K) {
    iterator KI = find(K);
    if (KI == end())
      return false;
    erase(KI);
    return true;
  }
  
  /// Get the list of keys associated to the same value as a given key.
  /// @returns false if the key is not in the map.
  bool getAssociatedValues(KeyT K, llvm::SmallVectorImpl<KeyT>& OutV) {
    auto ListI = this->Index.find(K);
    if (ListI == this->Index.end())
      return false;
    ValueListKeyT& List = *(ListI->second);
    for (KeyT K: List)
      OutV.push_back(K);
    return true;
  }
  
  
};


}

#endif
