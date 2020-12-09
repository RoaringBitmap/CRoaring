/*
 * memory_unit.C
 *
 */

#include <roaring/roaring.h>  // access to pure C exported API for testing

#include <roaring/containers/containers.h>
using namespace roaring::internal;

#include "roaring.hh"
using roaring::Roaring;  // the C++ wrapper class

#include "roaring64map.hh"
using roaring::Roaring64Map;  // C++ class extended for 64-bit numbers

#include "test.h"

#define ASSERT_HEAP_SIZE(n) assert_int_equal(tracker.getAllocatedTotal(), n)
#define ASSERT_HEAP_SIZE_COW(n1, n2) assert_int_equal(tracker.getAllocatedTotal(), (copy_on_write ? n1 : n2))
#define ASSERT_USE_COUNT(n) assert_int_equal(use_count, n)
#define ASSERT_VALID_C_BITMAP(b) assert_true(copy_on_write || isValidC32Bitmap(b))
#define ASSERT_VALID_CPP_BITMAP(b) assert_true(copy_on_write || isValidCPP32Bitmap(b))
#define ASSERT_VALID_CPP64_BITMAP(b) assert_true(copy_on_write || isValidCPP64Bitmap(b))

/**
 * These functions ensure that all sub-objects in a bitmap have consistant pointers to the options
 *and memory structs.
 **/
bool isValidC32Bitmap(const roaring_bitmap_t* b)
{
   roaring_options_t* opts = b->options;
   const roaring_array_t* ra = &b->high_low_container;

   // check that array object is consistant
   if (ra->options != opts)
   {
      fprintf(stderr, "%d %p %p\n", __LINE__, opts, ra->options);
      return false;
   }

   // check that containers are consistant
   for (int32_t i = 0; i < ra->size; ++i)
   {
      switch (ra->typecodes[i])
      {
      case BITSET_CONTAINER_TYPE:
         if (((bitset_container_t*)ra->containers[i])->options != opts)
         {
            fprintf(stderr,
               "%d %p %p\n",
               __LINE__,
               opts,
               ((bitset_container_t*)ra->containers[i])->options);
            return false;
         }
         break;
      case ARRAY_CONTAINER_TYPE:
         if (((array_container_t*)ra->containers[i])->options != opts)
         {
            fprintf(stderr,
               "%d %p %p\n",
               __LINE__,
               opts,
               ((array_container_t*)ra->containers[i])->options);
            return false;
         }
         break;
      case RUN_CONTAINER_TYPE:
         if (((run_container_t*)ra->containers[i])->options != opts)
         {
            fprintf(stderr,
               "%d %p %p\n",
               __LINE__,
               opts,
               ((run_container_t*)ra->containers[i])->options);
            return false;
         }
         break;
      case SHARED_CONTAINER_TYPE:
         // shared containers not currently checked as they have complex ownership
         break;
      default:
         fprintf(stderr, "%d default\n", __LINE__);
         return false;
      }
   }

   // if all else passes, this bitmap is valid
   return true;
}

bool isValidCPP32Bitmap(const Roaring& b) { return isValidC32Bitmap(&b.roaring); }

bool isValidCPP64Bitmap(const Roaring64Map& b)
{
   for (const auto& x : b.getBitmaps())
   {
      // validate the 32 bitmap independently
      if (!isValidCPP32Bitmap(x.second))
      {
         return false;
      }
   }
   return true;
}

class CRoaringMemoryTracker {
   std::map<void*, int> allocated;

public:
   CRoaringMemoryTracker() {}

   void* malloc(size_t n)
   {
      void* p = ::malloc(n);
      if (allocated.find(p) == allocated.end())
      {
         allocated[p] = n;
         return p;
      }
      ::free(p);
      return NULL;
   }

   void* realloc(void* p, size_t old_sz, size_t new_sz)
   {
       (void)old_sz;
      void* new_p = ::realloc(p, new_sz);
      if (new_sz == 0)
      {
         allocated.erase(p);
         return NULL;
      }
      else if (p == new_p)
      {
         allocated[p] = new_sz;
         return p;
      }
      else
      {
         allocated.erase(p);
         allocated[new_p] = new_sz;
         return new_p;
      }
   }

   void* calloc(size_t n_elements, size_t element_size)
   {
      void* p = ::calloc(n_elements, element_size);
      if (allocated.find(p) == allocated.end())
      {
         allocated[p] = n_elements * element_size;
         return p;
      }
      ::free(p);
      return NULL;
   }

   void free(void* p)
   {
      ::free(p);
      allocated.erase(p);
   }

   int getAllocatedTotal()
   {
      int tot = 0;
      for (auto const& e : allocated)
      {
         tot += e.second;
      }
      return tot;
   }

   void reset() { allocated.clear(); }
};

CRoaringMemoryTracker tracker;

void* my_malloc(size_t n, void* payload)
{
   if (payload != NULL)
   {
      *((int*)payload) += 1;
   }
   return tracker.malloc(n);
}

void* my_realloc(void* p, size_t old_sz, size_t new_sz, void* payload)
{
   if (payload != NULL)
   {
      *((int*)payload) += 1;
   }
   return tracker.realloc(p, old_sz, new_sz);
}

void* my_calloc(size_t n_elements, size_t element_size, void* payload)
{
   if (payload != NULL)
   {
      *((int*)payload) += 1;
   }
   return tracker.calloc(n_elements, element_size);
}

void my_free(void* p, void* payload)
{
   if (payload != NULL)
   {
      *((int*)payload) += 1;
   }
   tracker.free(p);
}

int use_count;
roaring_memory_t mem;
roaring_options_t opt;

void init_settings()
{
   use_count = 0;

   mem.malloc = &my_malloc;
   mem.realloc = &my_realloc;
   mem.calloc = &my_calloc;
   mem.free = &my_free;
   mem.payload = &use_count;

   opt.memory = &mem;

   tracker.reset();
}

void test_roaring_memory_meta(void**)
{
   void* p = tracker.malloc(100);
   ASSERT_HEAP_SIZE(100);

   p = tracker.realloc(p, 100, 200);
   ASSERT_HEAP_SIZE(200);

   tracker.free(p);
   p = tracker.calloc(16, 64);
   ASSERT_HEAP_SIZE(1024);

   tracker.free(p);
   ASSERT_HEAP_SIZE(0);
}

/*
 * Basic sanity check. These memory values have been validated with valgrind.
 * Also verifies that the opaque void pointer payload works.
 */
void test_roaring_memory_basic(void**)
{
   init_settings();
   bool copy_on_write = false;
   ASSERT_USE_COUNT(0);

   // empty bitmap
   roaring_bitmap_t* b = roaring_bitmap_create_with_opts(&opt);
   assert_ptr_not_equal(b, nullptr);
   ASSERT_HEAP_SIZE(120);
   ASSERT_USE_COUNT(3);

   ASSERT_VALID_C_BITMAP(b);
   roaring_bitmap_free(b);
   ASSERT_HEAP_SIZE(0);
   ASSERT_USE_COUNT(7);

   // simple bitmap
   b = roaring_bitmap_create_with_opts(&opt);
   assert_ptr_not_equal(b, nullptr);
   ASSERT_HEAP_SIZE(120);
   ASSERT_USE_COUNT(10);

   for (uint32_t i = 100; i < 1000; i++)
   {
      roaring_bitmap_add(b, i);
   }
   ASSERT_HEAP_SIZE(2352);
   ASSERT_USE_COUNT(27);
   assert_true(roaring_bitmap_contains(b, 500));
   ASSERT_VALID_C_BITMAP(b);

   roaring_bitmap_free(b);
   ASSERT_HEAP_SIZE(0);
   ASSERT_USE_COUNT(33);
}

void test_roaring_memory_struct_ownership(void**)
{
   init_settings();
   bool copy_on_write = false;
   ASSERT_USE_COUNT(0);

   // create a copy of our test options locally
   roaring_options_t* opt_local = (roaring_options_t*)malloc(sizeof(roaring_options_t));
   roaring_memory_t* mem_local = (roaring_memory_t*)malloc(sizeof(roaring_memory_t));
   memcpy(mem_local, &mem, sizeof(roaring_memory_t));
   opt_local->memory = mem_local;

   // scoping for objects
   {
      // create empty bitmaps which should copy in the option struct
      roaring_bitmap_t* b1 = roaring_bitmap_create_with_opts(opt_local);
      assert_ptr_not_equal(b1, nullptr);
      ASSERT_USE_COUNT(3);

      Roaring b2(opt_local);
      ASSERT_USE_COUNT(5);

      Roaring64Map b3(opt_local);
      ASSERT_USE_COUNT(7);

      // free both structs, invalidating any pointers to this memory
      free(mem_local);
      free(opt_local);

      // ensure we are still using to originally provided options
      for (uint32_t i = 100; i < 1000; i++)
      {
         roaring_bitmap_add(b1, i);
      }
      ASSERT_USE_COUNT(24);

      for (uint32_t i = 100; i < 1000; i++)
      {
         b2.add(i);
      }
      ASSERT_USE_COUNT(41);

      for (uint32_t i = 100; i < 1000; i++)
      {
         b3.add(i);
      }
      ASSERT_USE_COUNT(60);

      ASSERT_VALID_C_BITMAP(b1);
      ASSERT_VALID_CPP_BITMAP(b2);
      ASSERT_VALID_CPP64_BITMAP(b3);
      roaring_bitmap_free(b1);
   }
   ASSERT_HEAP_SIZE(0);
   ASSERT_USE_COUNT(78);
}

bool test_roaring_memory_sumall(uint32_t value, void* param)
{
   *(uint32_t*)param += value;
   return true; // we always process all values
}

bool test_roaring_memory_sumall64(uint64_t value, void* param)
{
   *(uint64_t*)param += value;
   return true; // we always process all values
}

/*
 * A near direct copy of the example code provided for C. Offers a good range of external tests.
 * These memory values have been validated with valgrind.
 */
void test_roaring_memory_c_example(bool copy_on_write, roaring_options_t* options)
{
   // create a new empty bitmap
   roaring_bitmap_t* r1 = roaring_bitmap_create_with_opts(options);
   roaring_bitmap_set_copy_on_write(r1, copy_on_write);
   assert_ptr_not_equal(r1, nullptr);
   ASSERT_HEAP_SIZE(120);

   // then we can add values
   for (uint32_t i = 100; i < 1000; i++)
   {
      roaring_bitmap_add(r1, i);
   }
   ASSERT_HEAP_SIZE(2352);

   // check whether a value is contained
   assert_true(roaring_bitmap_contains(r1, 500));

   // compute how many bits there are:
   uint64_t cardinality = roaring_bitmap_get_cardinality(r1);
   assert_int_equal(900, cardinality);

   // if your bitmaps have long runs, you can compress them by calling
   // run_optimize
   roaring_bitmap_run_optimize(r1);
   ASSERT_HEAP_SIZE(170);

   // create a new bitmap with varargs
   roaring_bitmap_t* r2 = roaring_bitmap_of_with_opts(5, options, 1, 2, 3, 5, 6);
   assert_ptr_not_equal(r2, nullptr);
   ASSERT_HEAP_SIZE(352);

   // we can also create a bitmap from a pointer to 32-bit integers
   const uint32_t values[] = { 2, 3, 4 };
   roaring_bitmap_t* r3 = roaring_bitmap_of_ptr_with_opts(3, values, options);
   roaring_bitmap_set_copy_on_write(r3, copy_on_write);
   ASSERT_HEAP_SIZE(526);

   // we can also go in reverse and go from arrays to bitmaps
   uint64_t card1 = roaring_bitmap_get_cardinality(r1);
   uint32_t* arr1 = new uint32_t[card1];
   assert_ptr_not_equal(arr1, nullptr);
   roaring_bitmap_to_uint32_array(r1, arr1);

   roaring_bitmap_t* r1f = roaring_bitmap_of_ptr_with_opts(card1, arr1, options);
   delete[] arr1;
   assert_ptr_not_equal(r1f, nullptr);
   ASSERT_HEAP_SIZE(2878);

   // bitmaps shall be equal
   assert_true(roaring_bitmap_equals(r1, r1f));
   ASSERT_VALID_C_BITMAP(r1f);
   roaring_bitmap_free(r1f);
   ASSERT_HEAP_SIZE(526);

   // we can copy and compare bitmaps
   roaring_bitmap_t* z = roaring_bitmap_copy_with_opts(r3, options);
   roaring_bitmap_set_copy_on_write(z, copy_on_write);
   ASSERT_HEAP_SIZE_COW(681, 689);

   assert_true(roaring_bitmap_equals(r3, z));
   ASSERT_VALID_C_BITMAP(z);
   roaring_bitmap_free(z);
   ASSERT_HEAP_SIZE_COW(550, 526);

   // we can compute union two-by-two
   roaring_bitmap_t* r1_2_3 = roaring_bitmap_or_with_opts(r1, r2, options);
   roaring_bitmap_set_copy_on_write(r1_2_3, copy_on_write);
   ASSERT_HEAP_SIZE_COW(764, 740);

   roaring_bitmap_or_inplace(r1_2_3, r3);
   ASSERT_HEAP_SIZE_COW(764, 740);

   // we can compute a big union
   const roaring_bitmap_t* allmybitmaps[] = { r1, r2, r3 };
   roaring_bitmap_t* bigunion = roaring_bitmap_or_many_with_opts(3, allmybitmaps, options);
   assert_true(roaring_bitmap_equals(r1_2_3, bigunion));
   ASSERT_HEAP_SIZE_COW(2742, 2718);

   roaring_bitmap_t* bigunionheap = roaring_bitmap_or_many_heap(3, allmybitmaps);
   assert_ptr_equal(bigunionheap, nullptr);
   ASSERT_HEAP_SIZE_COW(2742, 2718); // heap algorithm cannot use options struct

   ASSERT_VALID_C_BITMAP(r1_2_3);
   ASSERT_VALID_C_BITMAP(bigunion);
   roaring_bitmap_free(r1_2_3);
   roaring_bitmap_free(bigunion);
   ASSERT_HEAP_SIZE_COW(550, 526);

   // we can compute intersection two-by-two
   roaring_bitmap_t* i1_2 = roaring_bitmap_and_with_opts(r1, r2, options);
   ASSERT_HEAP_SIZE_COW(681, 657);
   ASSERT_VALID_C_BITMAP(i1_2);
   roaring_bitmap_free(i1_2);
   ASSERT_HEAP_SIZE_COW(550, 526);

   // we can write a bitmap to a pointer and recover it later
   size_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
   char* serializedbytes = (char*)malloc(expectedsize);
   roaring_bitmap_portable_serialize(r1, serializedbytes);
   roaring_bitmap_t* t = roaring_bitmap_portable_deserialize(serializedbytes, options);
   free(serializedbytes);
   ASSERT_HEAP_SIZE_COW(709, 685);
   assert_true(expectedsize == roaring_bitmap_portable_size_in_bytes(t));
   assert_true(roaring_bitmap_equals(r1, t));
   ASSERT_VALID_C_BITMAP(t);
   roaring_bitmap_free(t);
   ASSERT_HEAP_SIZE_COW(550, 526);

   // we can iterate over all values using custom functions
   uint32_t counter = 0;
   roaring_iterate(r1, test_roaring_memory_sumall, &counter);

   ASSERT_VALID_C_BITMAP(r1);
   ASSERT_VALID_C_BITMAP(r2);
   ASSERT_VALID_C_BITMAP(r3);
   roaring_bitmap_free(r1);
   roaring_bitmap_free(r2);
   roaring_bitmap_free(r3);

   ASSERT_HEAP_SIZE(0);
}

void test_roaring_memory_cpp_example(bool copy_on_write, roaring_options_t* options)
{
   {
      // create a new empty bitmap
      Roaring r1(options);
      r1.setCopyOnWrite(copy_on_write);
      ASSERT_HEAP_SIZE(64);

      // then we can add values
      for (uint32_t i = 100; i < 1000; i++)
      {
         r1.add(i);
      }
      ASSERT_HEAP_SIZE(2296);

      // check whether a value is contained
      assert_true(r1.contains(500));

      // compute how many bits there are:
      uint64_t cardinality = r1.cardinality();
      assert_int_equal(900, cardinality);

      // if your bitmaps have long runs, you can compress them by calling run_optimize
      r1.runOptimize();
      ASSERT_HEAP_SIZE(114);

      // create a new bitmap with varargs
      Roaring r2 = Roaring::bitmapOfWithOpts(5, options, 1, 2, 3, 5, 6);
      ASSERT_HEAP_SIZE(240);

      // test select
      uint32_t element;
      r2.select(3, &element);
      assert_true(element == 5);
      assert_true(r2.minimum() == 1);
      assert_true(r2.maximum() == 6);
      assert_true(r2.rank(4) == 3);

      // we can also create a bitmap from a pointer to 32-bit integers
      const uint32_t values[] = { 2, 3, 4 };
      Roaring r3(3, values, options);
      r3.setCopyOnWrite(copy_on_write);
      ASSERT_HEAP_SIZE(358);

      {
         // we can also go in reverse and go from arrays to bitmaps
         uint64_t card1 = r1.cardinality();
         uint32_t* arr1 = new uint32_t[card1];
         assert_true(arr1 != NULL);
         r1.toUint32Array(arr1);
         Roaring r1f(card1, arr1, options);
         delete[] arr1;
         ASSERT_HEAP_SIZE(2654);

         // bitmaps shall be equal
         assert_true(r1 == r1f);
         ASSERT_VALID_CPP_BITMAP(r1f);
      }
      ASSERT_HEAP_SIZE(358);

      {
         // we can copy and compare bitmaps
         Roaring z(r3, options);
         z.setCopyOnWrite(copy_on_write);
         assert_true(r3 == z);
         ASSERT_HEAP_SIZE_COW(457, 465);
         ASSERT_VALID_CPP_BITMAP(z);
      }
      ASSERT_HEAP_SIZE_COW(382, 358);

      {
         // we can compute union two-by-two
         Roaring r1_2_3(options);
         ASSERT_HEAP_SIZE_COW(446, 422);

         r1_2_3 = r1 | r2;
         r1_2_3.setCopyOnWrite(copy_on_write);
         ASSERT_HEAP_SIZE_COW(382, 358); // move constructor does not use existing options

         r1_2_3 |= r3;
         ASSERT_HEAP_SIZE_COW(382, 358); // move constructor does not use existing options
         ASSERT_VALID_CPP_BITMAP(r1_2_3);
      }
      ASSERT_HEAP_SIZE_COW(382, 358);

      {
         // do it again with the copy constructor
         Roaring r1_2_3(r1, options);
         ASSERT_HEAP_SIZE_COW(481, 461); // now the memory has been copied
         r1_2_3 |= r2;
         ASSERT_HEAP_SIZE_COW(553, 485);
         r1_2_3 |= r3;
         ASSERT_HEAP_SIZE_COW(553, 513);

         // we can compute a big union
         const Roaring* allmybitmaps[] = { &r1, &r2, &r3 };
         Roaring bigunion = Roaring::fastunion(3, allmybitmaps, options);
         assert_true(r1_2_3 == bigunion);
         ASSERT_HEAP_SIZE_COW(2464, 2424);

         ASSERT_VALID_CPP_BITMAP(r1_2_3);
         ASSERT_VALID_CPP_BITMAP(bigunion);
      }
      ASSERT_HEAP_SIZE_COW(406, 358);

      // we can compute intersection two-by-two
      {
         Roaring i1_2(r1, options);
         ASSERT_HEAP_SIZE_COW(481, 461);
         i1_2 &= r2;
         ASSERT_HEAP_SIZE_COW(481, 433);

         ASSERT_VALID_CPP_BITMAP(i1_2);
      }
      ASSERT_HEAP_SIZE_COW(406, 358);

      {
         // we can write a bitmap to a pointer and recover it later
         size_t expectedsize = r1.getSizeInBytes(true);
         char* serializedbytes = new char[expectedsize];
         r1.write(serializedbytes, true);
         Roaring t1 = Roaring::read(serializedbytes, true, options);
         assert_true(expectedsize == t1.getSizeInBytes(true));
         assert_true(r1 == t1);
         ASSERT_HEAP_SIZE_COW(509, 461);
         delete[] serializedbytes;

         expectedsize = r1.getSizeInBytes(false);
         serializedbytes = new char[expectedsize];
         r1.write(serializedbytes, false);
         Roaring t2 = Roaring::read(serializedbytes, false, options);
         assert_true(expectedsize == t2.getSizeInBytes(false));
         assert_true(r1 == t2);
         ASSERT_HEAP_SIZE_COW(612, 564);
         delete[] serializedbytes;

         // we can iterate over all values using custom functions
         uint32_t counter = 0;
         r1.iterate(test_roaring_memory_sumall, &counter);

         // we can also iterate the C++ way
         counter = 0;
         for (Roaring::const_iterator i = t1.begin(); i != t1.end(); i++)
         {
            ++counter;
         }
         assert_true(counter == t1.cardinality());
         ASSERT_VALID_CPP_BITMAP(t1);
         ASSERT_VALID_CPP_BITMAP(t2);
      }
      ASSERT_HEAP_SIZE_COW(406, 358);

      // we can move iterators
      const uint32_t manyvalues[] = { 2, 3, 4, 7, 8 };
      Roaring rogue(5, manyvalues, options);
      Roaring::const_iterator j = rogue.begin();
      j.equalorlarger(4);
      assert_true(*j == 4);

      ASSERT_VALID_CPP_BITMAP(rogue);
      ASSERT_VALID_CPP_BITMAP(r1);
      ASSERT_VALID_CPP_BITMAP(r2);
      ASSERT_VALID_CPP_BITMAP(r3);
   }
   ASSERT_HEAP_SIZE(0);

   // test move constructor
   {
      Roaring b(options);
      ASSERT_HEAP_SIZE(64);

      b.add(10);
      b.add(20);
      ASSERT_HEAP_SIZE(114);

      Roaring a(std::move(b));
      assert_true(a.cardinality() == 2);
      assert_true(a.contains(10));
      assert_true(a.contains(20));

      // b should be destroyed without any errors
      assert_true(b.cardinality() == 0);

      ASSERT_HEAP_SIZE(114);
      ASSERT_VALID_CPP_BITMAP(a);
      ASSERT_VALID_CPP_BITMAP(b);
   }
   ASSERT_HEAP_SIZE(0);

   // test move operator
   {
      Roaring b(options);
      ASSERT_HEAP_SIZE(64);

      b.add(10);
      b.add(20);
      ASSERT_HEAP_SIZE(114);

      Roaring a(options);
      ASSERT_HEAP_SIZE(178);

      a = std::move(b);
      assert_int_equal(2, a.cardinality());
      assert_true(a.contains(10));
      assert_true(a.contains(20));

      // b should be destroyed without any errors
      assert_true(b.cardinality() == 0);

      ASSERT_HEAP_SIZE(114);
      ASSERT_VALID_CPP_BITMAP(a);
      ASSERT_VALID_CPP_BITMAP(b);
   }
   ASSERT_HEAP_SIZE(0);

   // test toString
   {
      Roaring a(options);
      ASSERT_HEAP_SIZE(64);

      a.add(1);
      a.add(2);
      a.add(3);
      a.add(4);
      ASSERT_HEAP_SIZE(118);

      assert_string_equal("{1,2,3,4}", a.toString().c_str());
      ASSERT_VALID_CPP_BITMAP(a);
   }
   ASSERT_HEAP_SIZE(0);
}

void test_roaring_memory_cpp64_example(bool copy_on_write, roaring_options_t* options)
{
   {
      // create a new empty bitmap
      Roaring64Map r1(options);
      r1.setCopyOnWrite(copy_on_write);
      ASSERT_HEAP_SIZE(64);

      // then we can add values
      for (uint64_t i = 100; i < 1000; i++)
      {
         r1.add(i);
      }
      ASSERT_HEAP_SIZE(2360);
      for (uint64_t i = 14000000000000000100ull; i < 14000000000000001000ull; i++)
      {
         r1.add(i);
      }
      ASSERT_HEAP_SIZE(4656);

      // check whether a value is contained
      assert_true(r1.contains((uint64_t)14000000000000000500ull));

      // compute how many bits there are:
      uint64_t cardinality = r1.cardinality();
      assert_int_equal(1800, cardinality);

      // if your bitmaps have long runs, you can compress them by calling run_optimize
      r1.runOptimize();
      ASSERT_HEAP_SIZE(292);

      // create a new bitmap with varargs
      Roaring64Map r2 = Roaring64Map::bitmapOfWithOpts(
         5, options, 1ull, 2ull, 234294967296ull, 195839473298ull, 14000000000000000100ull);
      ASSERT_HEAP_SIZE(806);

      // test select
      uint64_t element;
      r2.select(4, &element);
      assert_true(element == 14000000000000000100ull);
      assert_true(r2.minimum() == 1ull);
      assert_true(r2.maximum() == 14000000000000000100ull);
      assert_true(r2.rank(234294967296ull) == 4ull);

      // we can also create a bitmap from a pointer to 32-bit integers
      const uint32_t values[] = { 2, 3, 4 };
      Roaring64Map r3(3, values, options);
      r3.setCopyOnWrite(copy_on_write);
      ASSERT_HEAP_SIZE(988);

      {
         // we can also go in reverse and go from arrays to bitmaps
         uint64_t card1 = r1.cardinality();
         uint64_t* arr1 = new uint64_t[card1];
         assert_true(arr1 != NULL);
         r1.toUint64Array(arr1);
         Roaring64Map r1f(card1, arr1, options);
         delete[] arr1;
         ASSERT_HEAP_SIZE_COW(5692, 5644);

         // bitmaps shall be equal
         assert_true(r1 == r1f);
         ASSERT_VALID_CPP64_BITMAP(r1f);
      }
      ASSERT_HEAP_SIZE_COW(1036, 988);

      {
         // we can copy and compare bitmaps
         Roaring64Map z(r3, options);
         z.setCopyOnWrite(copy_on_write);
         assert_true(r3 == z);
         ASSERT_HEAP_SIZE_COW(1199, 1159);
         ASSERT_VALID_CPP64_BITMAP(z);
      }
      ASSERT_HEAP_SIZE_COW(1060, 988);

      {
         // we can compute union two-by-two
         Roaring64Map r1_2_3(r1, options);
         ASSERT_HEAP_SIZE_COW(1274, 1258);
         r1_2_3 |= r2;
         ASSERT_HEAP_SIZE_COW(1362, 1278);
         r1_2_3.setCopyOnWrite(copy_on_write);
         r1_2_3 |= r3;
         ASSERT_HEAP_SIZE_COW(1386, 1294);

         // we can compute a big union
         const Roaring64Map* allmybitmaps[] = { &r1, &r2, &r3 };
         Roaring64Map bigunion = Roaring64Map::fastunion(3, allmybitmaps, options);
         assert_true(r1_2_3 == bigunion);
         ASSERT_HEAP_SIZE_COW(1450, 1358);

         ASSERT_VALID_CPP64_BITMAP(r1_2_3);
         ASSERT_VALID_CPP64_BITMAP(bigunion);
      }
      ASSERT_HEAP_SIZE_COW(1060, 988);

      {
         // we can compute intersection two-by-two
         Roaring64Map i1_2(options);
         ASSERT_HEAP_SIZE_COW(1124, 1052);
         i1_2 = r1 & r2;
         ASSERT_HEAP_SIZE_COW(1124, 1052);

         ASSERT_VALID_CPP64_BITMAP(i1_2);
      }
      ASSERT_HEAP_SIZE_COW(1060, 988);

      {
         // we can write a bitmap to a pointer and recover it later
         size_t expectedsize = r1.getSizeInBytes(true);
         char* serializedbytes = new char[expectedsize];
         r1.write(serializedbytes, true);
         Roaring64Map t1 = Roaring64Map::read(serializedbytes, true, options);
         assert_true(expectedsize == t1.getSizeInBytes(true));
         assert_true(r1 == t1);
         ASSERT_HEAP_SIZE_COW(1124, 1052);
         delete[] serializedbytes;

         expectedsize = r1.getSizeInBytes(false);
         serializedbytes = new char[expectedsize];
         r1.write(serializedbytes, false);
         Roaring64Map t2 = Roaring64Map::read(serializedbytes, false, options);
         assert_true(expectedsize == t2.getSizeInBytes(false));
         assert_true(r1 == t2);
         ASSERT_HEAP_SIZE_COW(1188, 1116);
         delete[] serializedbytes;

         // we can iterate over all values using custom functions
         uint64_t counter = 0;
         r1.iterate(test_roaring_memory_sumall64, &counter);

         // we can also iterate the C++ way
         counter = 0;
         for (Roaring64Map::const_iterator i = t1.begin(); i != t1.end(); i++)
         {
            ++counter;
         }
         assert_true(counter == t1.cardinality());
         ASSERT_VALID_CPP64_BITMAP(t1);
         ASSERT_VALID_CPP64_BITMAP(t2);
      }
      ASSERT_HEAP_SIZE_COW(1060, 988);
      ASSERT_VALID_CPP64_BITMAP(r1);
      ASSERT_VALID_CPP64_BITMAP(r2);
      ASSERT_VALID_CPP64_BITMAP(r3);
   }
   ASSERT_HEAP_SIZE(0);

   {
      Roaring64Map b(options);
      ASSERT_HEAP_SIZE(64);

      b.add(1u);
      b.add(2u);
      b.add(3u);
      assert_int_equal(3, b.cardinality());
      ASSERT_HEAP_SIZE(182);

      Roaring64Map a(std::move(b));
      assert_int_equal(3, a.cardinality());

      ASSERT_HEAP_SIZE(246);
      ASSERT_VALID_CPP64_BITMAP(a);
      ASSERT_VALID_CPP64_BITMAP(b);
   }
   ASSERT_HEAP_SIZE(0);
}

void test_roaring_memory_c_example_false(void**) {
   init_settings();
   test_roaring_memory_c_example(false, &opt);
}

void test_roaring_memory_c_example_true(void**) {
   init_settings();
   test_roaring_memory_c_example(true, &opt);
}

void test_roaring_memory_cpp_example_false(void**) {
   init_settings();
   test_roaring_memory_cpp_example(false, &opt);
}

void test_roaring_memory_cpp_example_true(void**) 
{
   init_settings();
   test_roaring_memory_cpp_example(true, &opt);
}

void test_roaring_memory_cpp64_example_false(void**)
{
   init_settings();
   test_roaring_memory_cpp64_example(false, &opt);
}

void test_roaring_memory_cpp64_example_true(void**)
{
   init_settings();
   test_roaring_memory_cpp64_example(true, &opt);
}

int main() {
#ifdef ENABLECMM
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_roaring_memory_meta),
        cmocka_unit_test(test_roaring_memory_basic),
        cmocka_unit_test(test_roaring_memory_struct_ownership),
        cmocka_unit_test(test_roaring_memory_c_example_false),
        cmocka_unit_test(test_roaring_memory_c_example_true),
        cmocka_unit_test(test_roaring_memory_cpp_example_false),
        cmocka_unit_test(test_roaring_memory_cpp_example_true),
        cmocka_unit_test(test_roaring_memory_cpp64_example_false),
        cmocka_unit_test(test_roaring_memory_cpp64_example_true),
        };

    return cmocka_run_group_tests(tests, NULL, NULL);
#endif
}