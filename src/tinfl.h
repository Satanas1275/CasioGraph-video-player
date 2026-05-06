#ifndef TINFL_H
#define TINFL_H

#include <stddef.h>

#define TINFL_FLAG_PARSE_ZLIB_HEADER (1)

size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len,
                                    const void *pSrc_buf, size_t src_buf_len,
                                    int flags);

#endif /* TINFL_H */
