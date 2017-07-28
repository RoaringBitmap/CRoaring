#include "roaring64map.hh"
#pragma managed

#include <msclr\marshal.h>

using namespace System;

namespace RoaringCLI {
	public ref class Roar
	{
	private:
		Roaring64Map *r64;
		static Roar() { }

	protected:
		__clrcall !Roar() { delete r64; }

	public: 
		__clrcall Roar() {
			r64 = new Roaring64Map();
		}

		__clrcall Roar(Roaring64Map& other) {
			r64 = new Roaring64Map(other);
		}

		__clrcall ~Roar() { this->!Roar(); }

		static Roar^ __clrcall read(array<Byte>^ Data, bool portable) {
			pin_ptr<Byte> buf = &Data[0];

			Roar^ rv = gcnew Roar(Roaring64Map::read((char *)buf, portable));

			buf = nullptr;
			return rv;
		}

		virtual property bool maximum { bool get() sealed { return r64->maximum(); }}
		virtual property bool minimum { bool get() sealed { return r64->minimum(); }}
		virtual property bool isEmpty { bool get() sealed { return r64->isEmpty(); }}
		virtual property bool isFull { bool get() sealed { return r64->isFull(); }}
		virtual property size_t cardinality { size_t get() sealed { return r64->cardinality(); }}
		virtual property bool copyOnWrite { bool get() sealed { return r64->getCopyOnWrite(); } void set(bool val) sealed { r64->setCopyOnWrite(val); } }

		virtual void __clrcall add(unsigned int x) sealed  {
			r64->add(x);
		}

		virtual void __clrcall add(size_t x) sealed {
			r64->add(x);
		}

		virtual bool __clrcall contains(unsigned int x) sealed {
			return r64->contains(x);
		}

		virtual bool __clrcall contains(size_t x) sealed {
			return r64->contains(x);
		}

		virtual void __clrcall remove(unsigned int x) sealed {
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

		virtual bool isSubset(Roar^ r) sealed {
			Roaring64Map* other = r->r64;

			return r64->isSubset(*other);
		}

		virtual void swap(Roar^ r) sealed {
			Roaring64Map* other = r->r64;

			r64->swap(*other);
		}

		virtual bool isStrictSubset(Roar^ r) sealed {
			return isSubset(r) && (cardinality != r->cardinality);
		}

		virtual size_t __clrcall getSizeInBytes(bool portable) sealed {
			return r64->getSizeInBytes(portable);
		}

		virtual void __clrcall setCopyOnWrite(bool val) sealed  {
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
		
		virtual size_t __clrcall write(array<Byte>^ Data, bool portable) sealed {
			pin_ptr<Byte> buf = &Data[0];
			
			size_t rv = r64->write((char *)buf, portable);

			buf = nullptr;
			return rv;
		}

		virtual size_t __clrcall range(size_t x) sealed {
			return r64->rank(x);
		}
	};
};

#pragma unmanaged
