io-bench
========

A micro benchmark system for storage device such as flash SSD, which using \emph{raw} IO to by-pass the operating system cache and file system buffer in the kernel, and obtain the real performance of the storage device. It supports multi-threads, so it can simulate single or multiple outstanding IOs with various access sequences. It can also be fed with real storage trace. The  synthetic access pattern can be generated such as the access addresses with different strides or access addresses following different distribution laws such as Poisson distribution and Zipf distribution.
