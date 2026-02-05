Memory Fragmentation Test
=========================

Heap stress test for memory fragmentation measurement.
Allocates and frees memory blocks with realistic size distribution
to simulate real-world memory usage patterns.

Configuration
-------------

Enable the example in menuconfig:
  Application Configuration -> Examples -> Example to intentionally fragment the heap memory

For detailed fragmentation information, also enable:
  Debug Options -> Memory Manager Debug Options -> Enable Heap Info
  Debug Options -> Memory Manager Debug Options -> Check heap fragmentation

TASH Commands
-------------

  memfrag init [seed]              Initialize tracking table (optional: set PRNG seed)
  memfrag seed <seed>              Set PRNG seed for reproducible patterns
  memfrag malloc <total_bytes>     Allocate memory blocks up to total bytes
  memfrag free <count>             Free random blocks (count = number of blocks)
  memfrag loop <bytes> <ratio> <count>
                                   Repeat malloc/free cycles
                                   - bytes: bytes to allocate per iteration
                                   - ratio: fraction to free (e.g., 1/2, 2/3)
                                   - count: number of iterations
  memfrag info                     Show current status and heap info
  memfrag help                     Show usage

Size Distribution
-----------------

Memory allocations follow a realistic size distribution:
  - Small (85%):  8-64 bytes (dominated by 36-byte allocations)
  - Medium (12%): 108-1024 bytes
  - Large (3%):   1796-512000 bytes

Example Usage
-------------

1. Initialize the test:
   TASH>> memfrag init 12345

2. Allocate 100KB of memory:
   TASH>> memfrag malloc 102400

3. Free 50 random blocks:
   TASH>> memfrag free 50

4. Run stress loop (allocate 50KB, free half, repeat 10 times):
   TASH>> memfrag loop 51200 1/2 10

5. Check fragmentation status:
   TASH>> memfrag info
   TASH>> heapinfo   (system command for detailed heap analysis)
