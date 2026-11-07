# Watcom DOS far-heap inspection correction

`heapwalk.c` and its private layout header `heap.h` come from Open Watcom v2
(`bld/clib/heap/c/heapwalk.c`, `bld/clib/heap/h/heap.h`), fetched 2026-11-04.
Their Sybase Open Watcom Public License is included in LICENSE.

The 16-bit large-model runtime's `__HeapWalk` compares a segment-based pointer
with NULL after converting the caller's far NULL to a based offset. A zero
offset in a nonzero heap segment is not a null far pointer. Consequently a
minimal `malloc(100); _heapchk();` reports `_HEAPBADNODE` and `_fheapwalk` reports
`_HEAPBADBEGIN` on an intact heap, including without any NETFTP or scene code.

Two checks are corrected: test the caller's original far pointer at entry, and
test the based offset as an unsigned offset when selecting the first block.
The includes are adapted to compile outside the Watcom runtime source tree.
No allocator is replaced. Linking this object before the CRT replaces only
`__HeapWalk`; the existing `_heapchk` still checks free-list links and sizes.

`tools/test_heap.py` checks valid allocations, multiple heap segments, frees,
and deliberate corruption followed by restoration. The scene lifecycle test
also checks the heap before entry, during rendering, and after exit.

Upstream input SHA-256:

- `heapwalk.c`: `cbf6869c000b80d69548ed7dc0eedcf757d4206510c870b39ef8633b61135c80`
- `heap.h`: `8d3e0b06e8bcec763cd6feec0e1251370c08e6688eb25cf04f9cade1cbfaf669`
