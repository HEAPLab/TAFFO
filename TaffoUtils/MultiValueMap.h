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
#include <type_traits>
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


template <typename BaseT, bool isConst = false>
class MultiValueMapIterator :
    public std::iterator<std::forward_iterator_tag,
                         std::pair<typename BaseT::key_type,
                                   typename BaseT::mapped_type>> {
  using KeyT = typename BaseT::key_type;
  using MappedT = typename BaseT::mapped_type;
  using InnerListT = typename std::conditional<
      isConst, const typename BaseT::ValueListKeyT,
      typename BaseT::ValueListKeyT>::type;
  using InnerListItT = typename std::conditional<
      isConst, typename BaseT::ValueListKeyT::const_iterator,
      typename BaseT::ValueListKeyT::iterator>::type;
  using OuterListT = typename std::conditional<
      isConst, const typename BaseT::ValueListKeyStorageT,
      typename BaseT::ValueListKeyStorageT>::type;
  using OuterListItT = typename std::conditional<
      isConst, typename BaseT::ValueListKeyStorageT::const_iterator,
      typename BaseT::ValueListKeyStorageT::iterator>::type;
  using ValueT = typename std::conditional<
      isConst, const std::pair<KeyT, MappedT>, std::pair<KeyT, MappedT>>::type;
  
  BaseT *Parent = nullptr;
  // IOuterList can be nullptr if not known. At end of list it must point to
  // the end of the list.
  OuterListItT IOuterList = nullptr;
  // nullptr only when at end of list
  //   Would be fully redundant with IOuterList if not for the unique_ptr
  // that stores each list.
  InnerListT *InnerList = nullptr;
  // Can be nullptr if not known or if at end of list.
  InnerListItT IInnerList = nullptr;
  // nullptr only when at end of list
  KeyT Key = nullptr;
  
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
    IInnerList = InnerList->begin();
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
  MultiValueMapIterator(BaseT& Parent, InnerListItT IInnerList,
                        OuterListItT IOuterList) :
      Parent(&Parent), IOuterList(IOuterList), IInnerList(IInnerList) {
    if (IOuterList != Parent.ValueListsStorage.end()) {
      InnerList = IOuterList->get();
      assert(IInnerList != InnerList->end());
      Key = *IInnerList;
    } else {
      InnerList = nullptr;
      Key = nullptr;
    }
  }
  // convenience initializer. Points at first value in list pointed to by
  // IOuterList (of course if it is not at the end of the list).
  MultiValueMapIterator(BaseT& Parent, OuterListItT IOuterList) :
      Parent(&Parent), IOuterList(IOuterList) {
    if (IOuterList != Parent.ValueListsStorage.end()) {
      InnerList = IOuterList->get();
      IInnerList = InnerList->begin();
      Key = *IInnerList;
    } else {
      InnerList = nullptr;
      Key = nullptr;
    }
  }
  /// Lazy initializer
  MultiValueMapIterator(BaseT& Parent, KeyT Key, InnerListT *InnerList) :
      Parent(&Parent), IOuterList(nullptr), InnerList(InnerList),
      IInnerList(nullptr), Key(Key) {}
  MultiValueMapIterator(MultiValueMapIterator const &Other) = default;
  MultiValueMapIterator(MultiValueMapIterator&&) = default;
  MultiValueMapIterator() = default;
  
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
  template <typename T, bool C> friend class MultiValueMapIterator;
  
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
  using const_iterator = MultiValueMapIterator<const MultiValueMap<KeyT, ValueT>, true>;
  
  inline iterator begin() {
    if (this->ValueListsStorage.empty())
      return end();
    return iterator(*this,
        this->ValueListsStorage.begin()->get()->begin(),
        this->ValueListsStorage.begin());
  }
  inline iterator end() {
    return iterator(*this,
        nullptr,
        this->ValueListsStorage.end());
  }
  inline const_iterator begin() const {
    if (this->ValueListsStorage.empty())
      return end();
    return const_iterator(*this,
        this->ValueListsStorage.begin()->get()->begin(),
        this->ValueListsStorage.begin());
  }
  inline const_iterator end() const {
    return const_iterator(*this,
        nullptr,
        this->ValueListsStorage.end());
  }
  
  iterator find(const KeyT& K) {
    auto VListIt = this->Index.find(K);
    if (VListIt == this->Index.end())
      return end();
    return iterator(*this, K, VListIt->second);
  }
  ValueT lookup(const KeyT& K) {
    auto VListIt = this->Index.find(K);
    if (VListIt == this->Index.end())
      return ValueT();
    return *(this->ValueListsToValues.find(*VListIt)->get());
  }
  ValueT& operator[](const KeyT& K) {
    auto VListIt = this->Index.find(K);
    assert(VListIt != this->Index.end());
    return *(this->ValueListsToValues.find(*VListIt)->get());
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
    iterator ResIt = iterator(*this, NewVL->begin(), NewI);
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
  
  template<typename InputIt>
  iterator insert(iterator P, InputIt I, InputIt E) {
    std::pair<iterator, bool> State{P, true};
    for (; I != E; ++I, ++State.first)
      State = insert(State.first, *I);
    return P;
  }
  template<typename InputIt>
  iterator insert(iterator P, InputIt I, InputIt E, ValueT MV) {
    std::pair<iterator, bool> State;
    State = insert(P, std::make_pair(*I, MV));
    for (++I, ++State.first; I != E; ++I, ++State.first)
      State = insertLeft(State.first, *I);
    return P;
  }
  
  iterator eraseAll(iterator I) {
    I.makeOuterListIt();
    for (auto K: *(I.InnerList)) {
      this->Index.erase(K);
    }
    auto NewIOuter = this->ValueListsStorage.erase(I.IOuterList);
    this->ValueListsToValues.erase(I.InnerList);
    return iterator(*this, NewIOuter);
  }
  bool eraseAll(const KeyT& K) {
    iterator KI = find(K);
    if (KI == end())
      return false;
    eraseAll(KI);
    return true;
  }
  
  iterator erase(iterator I) {
    I.makeInnerListIt();
    if (I.InnerList->size() == 1) {
      // obliterate the entire list
      I.makeOuterListIt();
      auto NewIOuter = this->ValueListsStorage.erase(I.IOuterList);
      this->ValueListsToValues.erase(I.InnerList);
      this->Index.erase(I.Key);
      return iterator(*this, NewIOuter);
    }
    // just remove the value
    I.IInnerList = I.InnerList->erase(I.IInnerList);
    return I;
  }
  bool erase(const KeyT& K) {
    iterator KI = find(K);
    if (KI == end())
      return false;
    erase(KI);
    return true;
  }
};


}

#endif
