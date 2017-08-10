#pragma unmanaged
#include "roaring64map.hh"

#pragma managed
#include <vcclr.h> 
#include <msclr/marshal.h>

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

namespace RoaringCLI {

	public ref class RoarCLI
	{
	private:
		Roaring64Map *r64;

		static RoarCLI() { }

	protected:
		__clrcall !RoarCLI() { delete r64; }

	public:
		__clrcall ~RoarCLI() { delete r64; }

		// Managed iterator passed directly to native code
		delegate bool ManagedIterator(uint64_t value, uint64_t %param);
		virtual bool __clrcall iterate(ManagedIterator^ iterator, size_t %high_bits) sealed {

			GCHandle gch = GCHandle::Alloc(iterator);
			IntPtr stubPointer = Marshal::GetFunctionPointerForDelegate(iterator);
			roaring_iterator64 functionPointer = static_cast<roaring_iterator64>(stubPointer.ToPointer());

			size_t counter = high_bits;

			r64->iterate(functionPointer, &counter);
			
			gch.Free();
			
			high_bits = counter;
			return true;
		}

		// constructors
		__clrcall RoarCLI() {
			r64 = new Roaring64Map();
		}

		__clrcall RoarCLI(RoarCLI^ other) {
			r64 = new Roaring64Map(*other->r64);
		}
		
		__clrcall RoarCLI(RoarCLI% other) : r64(other.r64) {}

		__clrcall RoarCLI(Roaring64Map&& other) {
			r64 = new Roaring64Map(other);
		}

		__clrcall RoarCLI(Roaring64Map& other) {
			r64 = new Roaring64Map(other);
		}

		__clrcall RoarCLI(Roaring &other) {
			r64 = new Roaring64Map(other);
		}

		__clrcall RoarCLI(roaring_bitmap_t* other) {
			r64 = new Roaring64Map(other);
		}

		__clrcall RoarCLI(... array<uint32_t>^ arr) {
			uint32_t len = arr->GetLength(0);
			pin_ptr<uint32_t> buf = &arr[0];

			r64 = new Roaring64Map(len, buf);

			buf = nullptr;
		}

		__clrcall RoarCLI(... array<size_t>^ arr) {
			size_t len = arr->GetLongLength(0);
			pin_ptr<size_t> buf = &arr[0];
			
			r64 = new Roaring64Map(len, buf);
			
			buf = nullptr;
		}

		__clrcall RoarCLI(List<uint32_t>^ vars) {
			array<uint32_t>^ vArr = vars->ToArray();
			pin_ptr<uint32_t> buf = &vArr[0];

			r64 = new Roaring64Map(vArr->Length, buf);

			// TODO: it would be more ideal to do something with the underlying API directly
			// the Roaring64 map is private
			// using CLI map and providing some sort of sync back and forth may also be more ideal
			// in the mean time live with the less than ideal perf
			// or rewrite this while thing for native CLI use
			//roaring_bitmap_add_many(&r64->roarings[0].roaring, vArr->Length, buf);

			buf = nullptr;
		}

		__clrcall RoarCLI(List<size_t>^ vars) {
			r64 = new Roaring64Map();
			for each(size_t v in vars)
				r64->add(v);
		}

		__clrcall RoarCLI(uint32_t Count, IntPtr intArr) {
			r64 = new Roaring64Map();

			r64 = new Roaring64Map(Count, (uint32_t *)intArr.ToPointer());
			//roaring_bitmap_add_many(&r64->roarings[0].roaring, Count, (uint32_t *)intArr.ToPointer());
		}

		__clrcall RoarCLI(size_t Count, IntPtr intArr) {
			r64 = new Roaring64Map(Count, (size_t *) intArr.ToPointer());
		}

		// static constructors 
		static RoarCLI^ __clrcall read(array<Byte>^ Data) { return read(Data, true); }
		static RoarCLI^ __clrcall read(array<Byte>^ Data, bool portable) {
			pin_ptr<Byte> buf = &Data[0];

			RoarCLI^ rv = gcnew RoarCLI(Roaring64Map::read((char *)buf, portable));

			buf = nullptr;
			return rv;
		}

		static RoarCLI^ __clrcall fastunion(size_t n, array<RoarCLI ^>^ inputs) {
			RoarCLI^ rv = gcnew RoarCLI();
			for each(RoarCLI^ input in inputs)
				rv |= input;

			return rv;
		}

		// static operators
		static bool    __clrcall operator ==(RoarCLI^ lval, RoarCLI^ rval) {
			if (!Object::ReferenceEquals(nullptr, rval) && !Object::ReferenceEquals(nullptr, lval))
				return *(lval->r64) == *(rval->r64);

			return Object::ReferenceEquals(lval, rval);
		}

		static RoarCLI^ __clrcall operator &(RoarCLI^ lval, RoarCLI^ rval) { return gcnew RoarCLI(*lval->r64 & *rval->r64); }
		static RoarCLI^ __clrcall operator -(RoarCLI^ lval, RoarCLI^ rval) { return gcnew RoarCLI(*lval->r64 - *rval->r64); }
		static RoarCLI^ __clrcall operator |(RoarCLI^ lval, RoarCLI^ rval) { return gcnew RoarCLI(*lval->r64 | *rval->r64); }
		static RoarCLI^ __clrcall operator ^(RoarCLI^ lval, RoarCLI^ rval) { return gcnew RoarCLI(*lval->r64 ^ *rval->r64); }

		// instance operators
		virtual RoarCLI^ __clrcall operator =(const RoarCLI% rval) sealed { return gcnew RoarCLI(*(r64 = rval.r64)); }
		virtual RoarCLI^ __clrcall operator =(const RoarCLI^ rval) sealed { return gcnew RoarCLI(*rval->r64); }
		virtual RoarCLI^ __clrcall operator &=(RoarCLI^ rval) sealed { return gcnew RoarCLI(*r64 &= *rval->r64); }
		virtual RoarCLI^ __clrcall operator -=(RoarCLI^ rval) sealed { return gcnew RoarCLI(*r64 -= *rval->r64); }
		virtual RoarCLI^ __clrcall operator |=(RoarCLI^ rval) sealed { return gcnew RoarCLI(*r64 |= *rval->r64); }
		virtual RoarCLI^ __clrcall operator ^=(RoarCLI^ rval) sealed { return gcnew RoarCLI(*r64 ^= *rval->r64); }

		// properties
		virtual property bool copyOnWrite { bool get() sealed { return r64->getCopyOnWrite(); } void __clrcall  set(bool val) sealed { r64->setCopyOnWrite(val); } }
		virtual property bool isEmpty { bool get() sealed { return r64->isEmpty(); }}
		virtual property bool isFull { bool get() sealed { return r64->isFull(); }}
		
		virtual property size_t cardinality { size_t get() sealed { return r64->cardinality(); }}
		virtual property size_t sizeInBytes { size_t get() sealed { return r64->getSizeInBytes(); }}
		virtual property size_t maximum { size_t get() sealed { return r64->maximum(); }}
		virtual property size_t minimum { size_t get() sealed { return r64->minimum(); }}

		// methods
		virtual array<size_t>^ __clrcall ToArray() sealed {
			array<size_t>^ rv = gcnew array<size_t>(cardinality);
			pin_ptr<size_t> buf = &rv[0];
			
			r64->toUint64Array(buf);

			buf = nullptr;
			return rv;
		}

		virtual void __clrcall add(uint32_t x) sealed {
			r64->add(x);
		}

		virtual void __clrcall add(size_t x) sealed {
			r64->add(x);
		}

		virtual void __clrcall addMany(... array<uint32_t>^ arr) {
			uint32_t len = arr->GetLength(0);
			pin_ptr<uint32_t> buf = &arr[0];
			
			r64->addMany(len, buf);

			buf = nullptr;
		}

		virtual void __clrcall addMany(... array<size_t>^ arr) {
			size_t len = arr->GetLongLength(0);
			pin_ptr<size_t> buf = &arr[0];

			r64->addMany(len, buf);

			buf = nullptr;
		}

		virtual bool __clrcall contains(uint32_t x) sealed {
			return r64->contains(x);
		}

		virtual bool __clrcall contains(size_t x) sealed {
			return r64->contains(x);
		}

		virtual void __clrcall remove(uint32_t x) sealed {
			r64->remove(x);
		}

		virtual void __clrcall remove(size_t x) sealed {
			r64->remove(x);
		}

		virtual bool __clrcall select(size_t rnk, size_t% element) sealed {
			size_t e = element;

			bool rv = r64->select(rnk, &e);
			element = e;

			return rv;
		}

		virtual bool __clrcall isSubset(RoarCLI^ r) sealed {
			Roaring64Map* other = r->r64;

			return r64->isSubset(*other);
		}

		virtual void __clrcall swap(RoarCLI^ r) sealed {
			Roaring64Map* other = r->r64;

			r64->swap(*other);
		}

		virtual bool __clrcall isStrictSubset(RoarCLI^ r) sealed {
			return isSubset(r) && (cardinality != r->cardinality);
		}

		// there's no default parameter in C++/CLI so this is simply to mimic the C++ API
		virtual size_t __clrcall getSizeInBytes() sealed { return r64->getSizeInBytes(true); }
		virtual size_t __clrcall getSizeInBytes(bool portable) sealed {
			return r64->getSizeInBytes(portable);
		}

		virtual void __clrcall setCopyOnWrite(bool val) sealed {
			r64->setCopyOnWrite(val);
		}

		virtual bool __clrcall runOptimize() sealed {
			return r64->runOptimize();
		}

		virtual size_t __clrcall shrinkToFit() sealed {
			return r64->shrinkToFit();
		}

		virtual bool __clrcall removeRunCompression() sealed {
			return r64->removeRunCompression();
		}

		virtual void __clrcall flip(size_t range_start, size_t range_end) sealed {
			r64->flip(range_start, range_end);
		}

		virtual size_t __clrcall write(array<Byte>^ Data) sealed { return write(Data, true); }
		virtual size_t __clrcall write(array<Byte>^ Data, bool portable) sealed {
			pin_ptr<Byte> buf = &Data[0];

			size_t rv = r64->write((char *)buf, portable);

			buf = nullptr;
			return rv;
		}

		virtual size_t __clrcall rank(size_t x) sealed {
			return r64->rank(x);
		}

		virtual void __clrcall printf() sealed {
			r64->printf();
		}

		virtual String^ __clrcall ToString() sealed override {
			return gcnew String(r64->toString().c_str());
		}

		// since we overloaded ==.  Maybe should do gethashcode also
		virtual bool __clrcall Equals(Object^ other) sealed override {
			if (Object::ReferenceEquals(nullptr, other)) return false;
			
			RoarCLI^ o = safe_cast<RoarCLI^>(other);

			if (Object::ReferenceEquals(nullptr, o)) return false;

			return r64 == o->r64;
		}
	};
};
#pragma unmanaged