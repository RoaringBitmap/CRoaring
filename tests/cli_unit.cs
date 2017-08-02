using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using RoaringCLI;
using System.IO;
using System.Diagnostics;
using static System.Console;
using System.Runtime.InteropServices;

namespace RoaringTestSharp
{
    class RoaringCLITest
    {
        static string aFile = "roaringTest.bin";

        static void Main(string[] args)
        {
            var test = new RoaringCLITest();
            test.Add();
            test.CPP(true);
            test.CPP(false);
            return;
        }

        public void CPP(bool copyParam)
        {
            RoarCLI r1 = new RoarCLI();
            ulong r1sum = 0;

            r1.setCopyOnWrite(copyParam);
            for (uint i = 100; i < 1000; i++)
            {
                r1.add(i);
                r1sum += i;
            }

            Debug.Assert(r1.contains(500));

            Save(r1, aFile);
            var rLoad = Load(aFile);
            Debug.Assert(r1 == rLoad);

            var rCopy = new RoarCLI(r1);

            RoarCLI rStack = rCopy;

            Debug.Assert(rStack == rCopy);

            var card = r1.cardinality;
            WriteLine($"Cardinality = {card}");
            r1.runOptimize();
            var compact = r1.getSizeInBytes();

            var varArr = new ulong[] { 5, 1, 2, 3, 5, 6 };
            RoarCLI r2 = new RoarCLI(new List<ulong>(varArr));
            r2.printf();

            RoarCLI r2arr = new RoarCLI(varArr);

            Debug.Assert(r2 == r2arr);

            ulong element = 0;
            r2.select(3, ref element);
            Debug.Assert(5 == element);
            Debug.Assert(r2.minimum == 1);
            Debug.Assert(r2.maximum == 6);
            Debug.Assert(r2.rank(4) == 3);

            var arrx = new ulong []{ 2, 3, 4 };
            var r3 = new RoarCLI(arrx.ToList());
            r3.setCopyOnWrite(copyParam);

            var card1 = r1.cardinality;
            var arrR1 = r1.ToArray();
            var arr1 = new ulong[card1];

            int size = Marshal.SizeOf(arrR1[0]) * arrR1.Length;
            IntPtr pnt = Marshal.AllocHGlobal(size);
            Marshal.Copy(arrR1.Select(l => (long)l).ToArray(), 0, pnt, arr1.Length);
            RoarCLI r1f = new RoarCLI(card1, pnt);
            Marshal.FreeHGlobal(pnt);

            Debug.Assert(r1f == r1);

            RoarCLI z = new RoarCLI(r3);
            z.setCopyOnWrite(copyParam);
            Debug.Assert(r3 == z);

            RoarCLI r1_2_3 = r1 | r2;
            r1_2_3.setCopyOnWrite(copyParam);
            r1_2_3 |= r3;

            RoarCLI[] all = new RoarCLI[] { r1, r2, r3 };
            RoarCLI bigunion = RoarCLI.fastunion(3, all);
            Debug.Assert(r1_2_3 == bigunion);

            RoarCLI i1_2 = r1 & r2;
            var expected = r1.getSizeInBytes();
            var bytes = new byte[expected];
            r1.write(bytes);
            var t = RoarCLI.read(bytes);

            Debug.Assert(expected == t.getSizeInBytes());
            Debug.Assert(r1 == t);

            ulong counter = 0;
            r1.iterate(count, ref counter);
            Debug.Assert(r1.cardinality == counter);

            counter = 0;
            r1.iterate(sum, ref counter);
            Debug.Assert(counter == r1sum);
        }

        // these managed callbacks get passed directly to native code =)
        bool sum(ulong value, ref ulong param)
        {
            param += value;
            return true;
        }
        bool count(ulong value, ref ulong param)
        {
            param++;
            return true;
        }


        public void Add()
        {
            RoarCLI r1 = new RoarCLI();

            for (ulong i = 100; i < 1000; i++)
                r1.add(i);
            
            Debug.Assert(r1.contains(500));
            Debug.Assert(900 == r1.cardinality);
            r1.setCopyOnWrite(true);

            var size = r1.getSizeInBytes(true);
            r1.runOptimize();
            var compact_size = r1.getSizeInBytes(true);

            WriteLine($"size before run optimize {size} bytes, and after {compact_size} bytes");

            RoarCLI r2 = new RoarCLI(new ulong[] {1,2,4,5,6,7,8,224,52,25252,2442});
            Debug.Assert(r2 != null);
            r2.printf();

            ulong[] values = { 2, 3, 4 };
            RoarCLI r3 = new RoarCLI(values.ToList());
            r3.setCopyOnWrite(true);

            var card1 = r1.cardinality;
            ulong[] arr1 = new ulong[card1];
            Debug.Assert(arr1 != null);

            var arr2 = r1.ToArray();

            var r1f = new RoarCLI(arr2.ToList());
            Debug.Assert(r1f.ToString() == r1.ToString());
            Debug.Assert(r1f == r1);
        }

        public RoarCLI Load(string aFile)
        {
            if (File.Exists(aFile))
            {
                var bdbytes = File.ReadAllBytes(aFile);
                return RoarCLI.read(bdbytes, false);
            }
            return null;
        }
        public void Save(RoarCLI r, string aFile)
        {
            var sizeNeeded = r.getSizeInBytes(false);
            var buff = new byte[sizeNeeded];
            r.write(buff, false);
            File.WriteAllBytes(aFile, buff);
        }
    }
}
