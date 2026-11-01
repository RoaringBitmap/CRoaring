# test data

These bitmaps were generated from Java : 
https://github.com/RoaringBitmap/RoaringBitmap/blob/master/examples/src/main/java/SerializeToDiskExample.java

The following files are shared with the Java (RoaringBitmap) and Go (roaring)
libraries:

- `crashproneinput*.bin`, `crashwithinvalidcookie.bin`, `crashwithoutcookie.bin`:
  malformed inputs that must be rejected (copied from Go's `testdata`).
- `bad-bitmap.bin`: an array container whose values are not sorted; it is
  accepted by the deserializer but fails validation (copied from Java).
- `testfrozendata/`: frozen and portable serializations of the same bitmaps,
  made with gocroaring (copied from Go's `testfrozendata`).
