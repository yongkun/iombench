iobench
========

A micro benchmark system for storage device such as hard disk and flash SSD. It uses direct IO to by-pass operating system cache and file system buffer, to obtain real performance of the storage device. It simulates multiple outstanding IOs by multi-threading with various access sequences.

I benchmarked several SSDs from Mtron, Intel, and OCZ with this program. Results are here: 
http://www.tkl.iis.u-tokyo.ac.jp/~yongkun/paper/ieice-yongkun-wang-final.pdf
