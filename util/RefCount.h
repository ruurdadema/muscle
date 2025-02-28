/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleRefCount_h
#define MuscleRefCount_h

#include "util/Cloneable.h"
#include "util/ObjectPool.h"
#include "util/PointerAndBool.h"
#include "system/AtomicCounter.h"

namespace muscle {

class RefCountable;

#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
class String;
extern void UpdateAllocationStackTrace(bool isAllocation, String * & s);  // implemented in SysLog.cpp
#endif

/** This macro declares typedefs for a given RefCountable type that follow the standard naming convention.
  * Given a RefCountable class Named XXX it will create typedefs named XXXRef and ConstXXXRef as
  * more readable synonyms for Ref<XXX> and ConstRef<XXX>, respectively.
  */
#define DECLARE_REFTYPES(RefCountableClassName)                                            \
   typedef muscle::ConstRef<RefCountableClassName>      Const##RefCountableClassName##Ref; \
   typedef muscle::Ref<RefCountableClassName>           RefCountableClassName##Ref;        \
   typedef muscle::DummyRef<RefCountableClassName>      Dummy##RefCountableClassName##Ref; \
   typedef muscle::DummyConstRef<RefCountableClassName> DummyConst##RefCountableClassName##Ref

/** This class represents objects that can be reference-counted using the Ref class.
  * Note that any object that can be reference-counted can also be cached and recycled via an ObjectPool.
  */
class RefCountable
{
public:
   /** Default constructor.  Refcount begins at zero. */
   RefCountable() : _manager(NULL)
#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
      , _allocatedAtStackTrace(NULL)
#endif
   {/* empty */}

   /** Copy constructor -- ref count and manager settings are deliberately not copied over! */
   RefCountable(const RefCountable &) : _refCount(), _manager(NULL)
#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
      , _allocatedAtStackTrace(NULL)
#endif
   {/* empty */}

   /** Virtual destructor, to keep C++ honest.  Don't remove this unless you like crashing */
   virtual ~RefCountable()
   {
#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
      UpdateAllocationStackTrace(false, _allocatedAtStackTrace);  // delete the allocation-location string, if any
#endif
   }

   /** Assigment operator.  deliberately implemented as a no-op! */
   inline RefCountable &operator=(const RefCountable &) {return *this;}

   /** Increments the counter.  Thread safe. */
   inline void IncrementRefCount() const {_refCount.AtomicIncrement();}

   /** Decrements the counter and returns true iff the new value is zero.  Thread safe. */
   inline bool DecrementRefCount() const {return _refCount.AtomicDecrement();}

   /** Sets the recycle-pointer for this object.  If set to non-NULL, this pointer
     * is used by the ObjectPool class to recycle this object when it is no longer
     * in use, so as to avoid the overhead of having to delete it and re-create it
     * later on.  The RefCountable class itself does nothing with this pointer.
     * Default value is NULL.
     * @param manager Pointer to the new manager object to use, or NULL to use no manager.
     */
   void SetManager(AbstractObjectManager * manager)
   {
      _manager = manager;
#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
      UpdateAllocationStackTrace((_manager!=NULL), _allocatedAtStackTrace);
#endif
   }

   /** Returns this object's current recyler pointer. */
   AbstractObjectManager * GetManager() const {return _manager;}

   /** Returns this object's current reference count.  Note that
     * the value returned by this method is volatile in multithreaded
     * environments, so it may already be wrong by the time it is returned.
     * Be careful!
     */
   uint32 GetRefCount() const {return _refCount.GetCount();}

#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
   /** If -DMUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS was specified on the compile line,
     * and this RefCountable is currently being managed by an ObjectPool, this will return
     * a string containing the stack trace of where the allocation occurred.  This is useful
     * for tracking down the cause of on-exit assertion failures from the ObjectPool class
     * destructors which are very picky about making sure all objects have been returned
     * to the pool before they deallocate the ObjectSlabs.
     *
     * Note that enabling this feature uses up gobs of extra memory and CPU, so don't leave
     * it enabled after you are done debugging.
     */
   const String * GetAllocationLocation() const {return _allocatedAtStackTrace;}
#endif

private:
   mutable AtomicCounter _refCount;
   AbstractObjectManager * _manager;

#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
   String * _allocatedAtStackTrace;
#endif
};
template <class Item> class Ref;           // forward reference
template <class Item> class ConstRef;      // forward reference
template <class Item> class DummyRef;      // forward reference
template <class Item> class DummyConstRef; // forward reference
DECLARE_REFTYPES(RefCountable);

/**
 *  This is a ref-count token object that works with any instantiation
 *  type that is a subclass of RefCountable.  The RefCountable object
 *  that this ConstRef item holds is considered read-only; if you want
 *  it to be modfiable, you should use the Ref class instead.
 */
template <class Item> class ConstRef
{
public:
   typedef ObjectPool<Item> ItemPool;        /**< type of an ObjectPool of user data structures */
   typedef Item ItemType;                    /**< the type we are specialized to */

   /**
    *  Default constructor.
    *  Creates a NULL reference (suitable for later initialization with SetRef(), or the assignment operator)
    */
   ConstRef() : _item(NULL, true) {/* empty */}

   /**
     * Explicit constructor.  Increases the reference-count of the specified item.
     * Once referenced, (item) will be automatically deleted (or recycled) when the last ConstRef that references it goes away.
     * @param item A dynamically allocated object that the ConstRef class will assume responsibility for deleting.  May be NULL.
     */
   explicit ConstRef(const Item * item) : _item(item, true) {RefItem();}

   /** Copy constructor.  Creates an additional reference to the object referenced by (rhs).
    *  The referenced object won't be deleted until ALL Refs that reference it are gone.
    *  @param rhs the object to make this a copy of.  Note that the data pointed to by (rhs) is not duplicated, only double-referenced.
    */
   ConstRef(const ConstRef & rhs) : _item(NULL, true) {*this = rhs;}

   /** This constructor is useful for automatic upcasting (e.g. creating an ConstAbstractReflectSessionRef from a ConstStorageReflectSessionRef)
     * @param refItem A Ref or ConstRef to copy our state from.
     */
   template<typename T> ConstRef(const ConstRef<T> & refItem) : _item(refItem(), refItem.IsRefCounting()) {RefItem();}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   ConstRef(ConstRef && rhs) {this->SwapContents(rhs);}
#endif

   /** Attempts to set this reference by downcasting the reference in the provided ConstRefCountableRef.
     * If the downcast cannot be done (via dynamic_cast) then we become a NULL reference.
     * @param ref The read-only RefCountable reference to set ourselves from.
     * @param junk This parameter is ignored; it is just here to disambiguate constructors.
     */
   ConstRef(const ConstRefCountableRef & ref, bool junk) : _item(NULL, true)
   {
      (void) junk;
      (void) SetFromRefCountableRef(ref);
   }

   /** Unreferences the held data item.  If this is the last ConstRef that
    *  references the held data item, the data item will be deleted or recycled at this time.
    */
   ~ConstRef() {UnrefItem();}

   /** Convenience method; unreferences any item this ConstRef is currently referencing; and
     * causes this ConstRef to reference the given Item instead.  Very similar to the assignment operator.
     * (in fact the assignment operator just calls this method)
     * @param item A dynamically allocated object that this ConstRef object will assume responsibility for deleting.
     *             May be NULL, in which case the effect is just to unreference the current item.
     * @param doRefCount If set false, then this ConstRef will not do any ref-counting on (item); rather it
     *                   just acts as a fancy version of a normal C++ pointer.  Specifically, it will not
     *                   modify the object's reference count, nor will it ever delete the object.
     *                   Setting this argument to false can be useful if you want to supply a
     *                   ConstRef to an item that wasn't dynamically allocated from the heap.
     *                   But if you do that, it allows the possibility of the object going away while
     *                   other Refs are still using it, so be careful!
     */
   void SetRef(const Item * item, bool doRefCount = true)
   {
      if (item == this->GetItemPointer())
      {
         if (doRefCount != this->IsRefCounting())
         {
            if (doRefCount)
            {
               // We weren't ref-counting before, but the user wants us to start
               SetRefCounting(true);
               RefItem();
            }
            else
            {
               // We were ref-counting before, but the user wants us to drop it
               UnrefItem();
               SetRefCounting(false);
            }
         }
      }
      else
      {
         // switch items
         UnrefItem();
         _item.SetPointerAndBool(item, doRefCount);
         RefItem();
      }
   }

   /** Assigment operator.
    *  Unreferences the previous held data item, and adds a reference to the data item of (rhs).
    *  @param rhs Item to become a copy of.
    */
   inline ConstRef &operator=(const ConstRef & rhs) {this->SetRef(rhs.GetItemPointer(), rhs.IsRefCounting()); return *this;}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   inline ConstRef &operator=(ConstRef && rhs) {this->SwapContents(rhs); return *this;}
#endif

   /** Similar to the == operator, except that this version will also call the comparison operator
     * on the objects themselves if necessary, to determine exact equality.  (This is different than
     * the behavior of the == operator, which only compares the pointers, not the objects themselves)
     * @param rhs the ConstRef whose referenced object we will compare to our own referenced object.
     */
   bool IsDeeplyEqualTo(const ConstRef & rhs) const
   {
      const Item * myItem  = this->GetItemPointer();
      const Item * hisItem = rhs.GetItemPointer();
      if (myItem == hisItem) return true;
      if ((myItem != NULL) != (hisItem != NULL)) return false;
      return ((myItem == NULL)||(*myItem == *hisItem));
   }

   /** Returns true iff both ConstRefs are referencing the same data.
     * @param rhs the ConstRef to compare pointers with
     */
   bool operator ==(const ConstRef &rhs) const {return this->GetItemPointer() == rhs.GetItemPointer();}

   /** Returns true iff both Refs are not referencing the same data.
     * @param rhs the ConstRef to compare pointers with
     */
   bool operator !=(const ConstRef &rhs) const {return this->GetItemPointer() != rhs.GetItemPointer();}

   /** Compares the pointers the two Refs are referencing.
     * @param rhs the ConstRef to compare pointers with
     */
   bool operator < (const ConstRef &rhs) const {return this->GetItemPointer() < rhs.GetItemPointer();}

   /** Compares the pointers the two Refs are referencing.
     * @param rhs the ConstRef to compare pointers with
     */
   bool operator > (const ConstRef &rhs) const {return this->GetItemPointer() > rhs.GetItemPointer();}

   /** Returns the ref-counted data item.  The returned data item
    *  is only guaranteed valid for as long as this RefCount object exists.
    */
   const Item * GetItemPointer() const {return _item.GetPointer();}

   /** Convenience synonym for GetItemPointer(). */
   const Item * operator()() const {return this->GetItemPointer();}

   /** Unreferences our held data item (if any), and turns this object back into a NULL reference.
    *  (equivalent to *this = ConstRef();)
    */
   void Reset() {UnrefItem();}

   /** Equivalent to Reset(), except that this method will not delete or recycle
     * the held object under any circumstances.  Use with caution, as use of this
     * method can result in memory leaks.
     */
   void Neutralize()
   {
      const Item * item = this->IsRefCounting() ? this->GetItemPointer() : NULL;
      if (item) (void) item->DecrementRefCount();  // remove our ref-count from the item but deliberately never delete the item
      _item.SetPointer(NULL);
   }

   /** Swaps this ConstRef's contents with those of the specified ConstRef.
     * @param swapWith ConstRef to swap state with.
     */
   void SwapContents(ConstRef & swapWith) {this->_item.SwapContents(swapWith._item);}

   /** Returns true iff we are refcounting our held object, or false
     * if we are merely pointing to it (see constructor documentation for details)
     */
   bool IsRefCounting() const {return _item.GetBool();}

   /** Convenience method:  Returns a ConstRefCountableRef object referencing the same RefCountable as this typed ref. */
   ConstRefCountableRef GetRefCountableRef() const {ConstRefCountableRef ret; ret.SetRef(this->GetItemPointer(), this->IsRefCounting()); return ret;}

   /** Convenience method; attempts to set this typed ConstRef to be referencing the same item as the given ConstRefCountableRef.
     * If the conversion cannot be done, our state will remain unchanged.
     * @param refCountableRef The ConstRefCountableRef to set ourselves from.
     * @returns B_NO_ERROR if the conversion was successful, or B_BAD_ARGUMENT if the ConstRefCountableRef's item
     *                     type is incompatible with our own item type (as dictated by dynamic_cast)
     */
   status_t SetFromRefCountableRef(const ConstRefCountableRef & refCountableRef)
   {
      const RefCountable * refCountableItem = refCountableRef();
      if (refCountableItem)
      {
         const Item * typedItem = dynamic_cast<const Item *>(refCountableItem);
         if (typedItem == NULL) return B_BAD_ARGUMENT;
         SetRef(typedItem, refCountableRef.IsRefCounting());
      }
      else Reset();

      return B_NO_ERROR;
   }

   /** Same as SetFromRefCountableRef(), but uses a static_cast instead of a dynamic_cast to
     * do the type conversion.  This is faster that SetFromRefCountableRef(), but it is up to
     * the user's code to guarantee that the conversion is valid.  If (refCountableRef)
     * references an object of the wrong type, undefined behaviour (read: crashing)
     * will likely occur.
     * @param refCountableRef the ConstRefCountableRef object we should become equal to
     */
   inline void SetFromRefCountableRefUnchecked(const ConstRefCountableRef & refCountableRef)
   {
      SetRef(static_cast<const Item *>(refCountableRef()), refCountableRef.IsRefCounting());
   }

   /** Returns true iff we are pointing to a valid item (i.e. if (GetItemPointer() != NULL)) */
   bool IsValid() const {return (this->GetItemPointer() != NULL);}

   /** Returns true iff we are not pointing to a valid item (i.e. if (GetItemPointer() == NULL)) */
   bool IsNull() const {return (this->GetItemPointer() == NULL);}

   /** Returns true only if we are certain that no other Refs are pointing
     * at the same RefCountable object that we are.  If this Ref's do-reference-counting
     * flag is false, then this method will always return false, since we can't
     * be sure about sharing unless we are reference counting.  If this ConstRef is
     * a NULL Ref, then this method will return true.
     */
   bool IsRefPrivate() const
   {
      const Item * item = this->GetItemPointer();
      return ((item == NULL)||((this->IsRefCounting())&&(item->GetRefCount() == 1)));
   }

   /** This method will check our referenced object to see if there is any
     * chance that it is shared by other ConstRef objects.  If it is, it will
     * make a copy of the referenced object and set this ConstRef to reference
     * the copy instead of the original.  The upshot of this is that once
     * this method returns B_NO_ERROR, you can safely modify the referenced
     * object without having to worry about race conditions caused by sharing
     * data with other threads.  This method is thread safe -- it may occasionally
     * make a copy that wasn't strictly necessary, but it will never fail to
     * make a copy when making a copy is necessary.
     * @returns B_NO_ERROR on success (i.e. the object was successfully copied,
     *                     or a copy turned out to be unnecessary), or B_OUT_OF_MEMORY
     *                     on failure (i.e. out of memory)
     */
   status_t EnsureRefIsPrivate()
   {
      if (IsRefPrivate() == false)
      {
         Ref<Item> copyRef = this->Clone();
         if (copyRef() == NULL) return B_OUT_OF_MEMORY;
         *this = copyRef;
      }
      return B_NO_ERROR;
   }

   /** Makes a copy of our held Item and returns it in a non-const Ref object,
     * or returns a NULL Ref on out-of-memory (or if we aren't holding an Item).
     */
   Ref<Item> Clone() const
   {
      const Item * item = this->GetItemPointer();
      if (item)
      {
         AbstractObjectManager * m = item->GetManager();
         Item * newItem;
         if (m)
         {
            newItem = static_cast<Item *>(m->ObtainObjectGeneric());
            if (newItem) *newItem = *item;
         }
         else newItem = CloneObject(*item);

         if (newItem) return Ref<Item>(newItem);
                 else MWARN_OUT_OF_MEMORY;
      }
      return Ref<Item>();
   }

   /** This method allows Refs to be keys in Hashtables.  Node that we hash on the pointer's value, not the object it points to! */
   uint32 HashCode() const {return CalculateHashCode(this->GetItemPointer());}

private:
   friend class DummyConstRef<Item>;
   friend class Ref<Item>;

   ConstRef(const Item * item, bool doRefCount) : _item(item, doRefCount) {RefItem();}
   void SetRefCounting(bool rc) {_item.SetBool(rc);}

   void RefItem()
   {
      const Item * item = this->IsRefCounting() ? this->GetItemPointer() : NULL;
      if (item) item->IncrementRefCount();
   }

   void UnrefItem()
   {
      const Item * item = this->GetItemPointer();
      if (item)
      {
         const bool isRefCounting = this->IsRefCounting();
         if ((isRefCounting)&&(item->DecrementRefCount()))
         {
            AbstractObjectManager * m = item->GetManager();
            if (m) m->RecycleObject(const_cast<Item *>(item));
              else delete item;
         }
         _item.SetPointer(NULL);
      }
   }

   PointerAndBool<const Item> _item;
};

/** This class is just syntactic sugar for more clearly declaring a
  * ConstRef that doesn't actually do any reference-counting of the object
  * that it refers to (e.g. if you need a ConstRef to a stack-based object and
  * are willing to take responsibility for manually managing object-lifetime issues
  * yourself).  It will behave similarly to a raw const-pointer
  */
template <class Item> class DummyConstRef : public ConstRef<Item>
{
public:
   /** Default constructor.  Creates a NULL reference. */
   DummyConstRef() {/* empty */}

   /** Creates a dummy-const-reference to the specified item
     * @param item reference to the item to point to
     */
   explicit DummyConstRef(const Item & item) : ConstRef<Item>(&item, false) {/* empty */}

   /** Creates a dummy-const-reference to the specified item
     * @param item pointer to the item to point to, or a NULL pointer to create a NULL DummyConstRef.
     */
   explicit DummyConstRef(const Item * item) : ConstRef<Item>(item, false) {/* empty */}

   /** This constructor is useful for automatic upcasting (e.g. creating a
     * DummyConstAbstractReflectSessionRef from a ConstStorageReflectSessionRef)
     * @param refItem A Ref to copy our state from.
     * @note this constructor will use the state of (refItem)'s IsRefCounting() flag verbatim, our Dummy status notwithstanding.
     */
   template<typename T> DummyConstRef(const ConstRef<T> & refItem) : ConstRef<Item>(refItem(), refItem.IsRefCounting()) { /* empty */}
};


/** When we compare references, we really want to be comparing what those references point to */
template <typename ItemType> class CompareFunctor<ConstRef<ItemType> >
{
public:
   /** Compares the two ConstRef's strcmp() style, returning zero if they are equal, a negative value if (item1) comes first, or a positive value if (item2) comes first.
     * @param item1 the first item to compare
     * @param item2 the first item to compare
     * @param cookie arbitrary user-specific value
     */
   int Compare(const ConstRef<ItemType> & item1, const ConstRef<ItemType> & item2, void * cookie) const {return CompareFunctor<const ItemType *>().Compare(item1(), item2(), cookie);}
};

/**
 *  This is a ref-count token object that works with any instantiation
 *  type that is a subclass of RefCountable.  The RefCountable object
 *  that this ConstRef item holds is considered modifiable; if you want
 *  it to be read-only, you should use the ConstRef class instead.
 */
template <class Item> class Ref : public ConstRef<Item>
{
public:
   /**
    *  Default constructor.
    *  Creates a NULL reference (suitable for later initialization with SetRef(), or the assignment operator)
    */
   Ref() : ConstRef<Item>() {/* empty */}

   /**
     * Explicit constructor.  Increases the reference-count of the specified item.
     * Once referenced, (item) will be automatically deleted (or recycled) when the last ConstRef that references it goes away.
     * @param item A dynamically allocated object that the ConstRef class will assume responsibility for deleting.  May be NULL.
     */
   explicit Ref(Item * item) : ConstRef<Item>(item, true) {/* empty */}

   /** Copy constructor.  Creates an additional reference to the object referenced by (rhs).
    *  The referenced object won't be deleted until ALL Refs that reference it are gone.
    *  @param rhs the object to make this object a duplicate of
    */
   Ref(const Ref & rhs) : ConstRef<Item>(rhs) {/* empty */}

   /** This constructor is useful for automatic upcasting (e.g. creating an AbstractReflectSessionRef from a StorageReflectSessionRef)
     * @param refItem A Ref to copy our state from.
     */
   template<typename T> Ref(const Ref<T> & refItem) : ConstRef<Item>(refItem(), refItem.IsRefCounting()) {/* empty */}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   Ref(Ref && rhs) {this->SwapContents(rhs);}
#endif

   /** Attempts to set this reference by downcasting the reference in the provided RefCountableRef.
     * If the downcast cannot be done (via dynamic_cast) then we become a NULL reference.
     * @param ref The RefCountable reference to set ourselves from.
     * @param junk This parameter is ignored; it is just here to disambiguate constructors.
     */
   Ref(const RefCountableRef & ref, bool junk) : ConstRef<Item>(ref, junk) {/* empty */}

   /** Returns the ref-counted data item.  The returned data item
    *  is only guaranteed valid for as long as this RefCount object exists.
    */
   Item * GetItemPointer() const {return const_cast<Item *>((static_cast<const ConstRef<Item> *>(this))->GetItemPointer());}

   /** Read-write implementation of the convenience synonym for GetItemPointer(). */
   Item * operator()() const {return GetItemPointer();}

   /** Convenience method:  Returns a read/write RefCountableRef object referencing the same RefCountable as this typed ref. */
   RefCountableRef GetRefCountableRef() const {RefCountableRef r; r.SetRef(GetItemPointer(), this->IsRefCounting()); return r;}

   /** Redeclared here so that the AutoChooseHashFunctor code will see it */
   uint32 HashCode() const {return this->ConstRef<Item>::HashCode();}

   /** Assigment operator.
    *  Unreferences the previous held data item, and adds a reference to the data item of (rhs).
    *  @param rhs Item to become a copy of.
    */
   inline Ref &operator=(const Ref & rhs) {this->SetRef(rhs.GetItemPointer(), rhs.IsRefCounting()); return *this;}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   inline Ref &operator=(Ref && rhs) {this->SwapContents(rhs); return *this;}
#endif

private:
   friend class DummyRef<Item>;

   Ref(Item * item, bool doRefCount) : ConstRef<Item>(item, doRefCount) {/* empty */}
};

/** This class is just syntactic sugar for more clearly declaring a
  * Ref that doesn't actually do any reference-counting of the object
  * that it refers to (e.g. if you need a Ref to a stack-based object and
  * are willing to take responsibility for manually managing object-lifetime issues
  * yourself).  It will behave similarly to a raw pointer.
  */
template <class Item> class DummyRef : public Ref<Item>
{
public:
   /** Default constructor.  Creates a NULL reference. */
   DummyRef() {/* empty */}

   /** Creates a dummy-const-reference to the specified item
     * @param item reference to the item to point to
     */
   explicit DummyRef(Item & item) : Ref<Item>(&item, false) {/* empty */}

   /** Creates a dummy-const-reference to the specified item
     * @param item pointer to the item to point to, or NULL to create a NULL DummyRef.
     */
   explicit DummyRef(Item * item) : Ref<Item>(item, false) {/* empty */}

   /** This constructor is useful for automatic upcasting (e.g. creating a
     * DummyAbstractReflectSessionRef from a StorageReflectSessionRef)
     * @param refItem A Ref to copy our state from.
     * @note this constructor will use the state of (refItem)'s IsRefCounting() flag verbatim, our Dummy status notwithstanding.
     */
   template<typename T> DummyRef(const Ref<T> & refItem) : Ref<Item>(refItem(), refItem.IsRefCounting()) {/* empty */}
};

/** This function works similarly to ConstRefCount::GetItemPointer(), except that this function
 *  can be safely called with a NULL ConstRef object as an argument.
 *  @param rt Pointer to a const reference object, or NULL.
 *  @returns If rt is NULL, this function returns NULL.  Otherwise it returns rt->GetItemPointer().
 */
template <class Item> inline const Item * CheckedGetItemPointer(const ConstRef<Item> * rt) {return rt ? rt->GetItemPointer() : NULL;}

/** This function works similarly to RefCount::GetItemPointer(), except that this function
 *  can be safely called with a NULL Ref object as an argument.
 *  @param rt Pointer to a const reference object, or NULL.
 *  @returns If rt is NULL, this function returns NULL.  Otherwise it returns rt->GetItemPointer().
 */
template <class Item> inline Item * CheckedGetItemPointer(const Ref<Item> * rt) {return rt ? rt->GetItemPointer() : NULL;}

/** Convenience method for converting a ConstRef into a non-const Ref.  Only call this method if you are sure you know what you are doing,
  * because usually the original ConstRef was declared as a ConstRef for a good reason!
  * @param constItem the ConstRef we want to return a Ref equivalent of.
  * @returns a non-const Ref that is pointing to the same object that the passed-in ConstRef was pointing to.
  */
template <class Item> inline Ref<Item> CastAwayConstFromRef(const ConstRef<Item> & constItem) {Ref<Item> ret; ret.SetRef(const_cast<Item *>(constItem()), constItem.IsRefCounting()); return ret;}

/** Convenience method for converting a non-const Ref into a ConstRef.  This method isn't strictly necessary, since
  * you can also just use the assignment-operator, but I'm including it for completeness, and because some compilers
  * want to warn you about object-slicing if you just use the assignment operator to do this.
  * @param nonConstItem the Ref we want to return a ConstRef equivalent of.
  * @returns a ConstRef that is pointing to the same object that the passed-in non-const Ref was pointing to.
  */
template <class Item> inline ConstRef<Item> AddConstToRef(const Ref<Item> & nonConstItem) {ConstRef<Item> ret; ret.SetRef(nonConstItem(), nonConstItem.IsRefCounting()); return ret;}

} // end namespace muscle

#endif
