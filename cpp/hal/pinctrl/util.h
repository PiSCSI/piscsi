#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>

#define INVALID_ADDRESS ((uint64_t)~0)
#define ROUND_UP(n, d) ((((n) + (d) - 1) / (d)) * (d))
#define UNUSED(x) (void)(x)

typedef struct dt_subnode_iter *DT_SUBNODE_HANDLE;
char *read_text_file(const char *fname, size_t *plen);

void *read_file(const char *fname, size_t *plen);

void dt_set_path(const char *path);

char *dt_read_prop(const char *node, const char *prop, size_t *len);

uint32_t *dt_read_cells(const char *node, const char *prop, unsigned *num_cells);

uint64_t dt_extract_num(const uint32_t *cells, int size);

uint64_t dt_read_num(const char *node, const char *prop, size_t size);

uint32_t dt_read_u32(const char *node, const char *prop);

uint64_t dt_parse_addr(const char *node);

void dt_free(void *value);

DT_SUBNODE_HANDLE dt_open_subnodes(const char *node);
const char *dt_next_subnode(DT_SUBNODE_HANDLE handle);
void dt_close_subnodes(DT_SUBNODE_HANDLE handle);

#endif
