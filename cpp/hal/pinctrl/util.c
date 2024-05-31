#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

// We're actually going to cheat and cast the pointers, but define
// a structure to keep the compiler happy.

struct dt_subnode_iter
{
    DIR *dh;
};

const char *dtpath;

static void *do_read_file(const char *fname, const char *mode, size_t *plen)
{
    FILE *fp = fopen(fname, mode);
    void *buf;
    long len;

    if (fp == NULL)
        return NULL;

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    if (plen)
        *plen = len;
    buf = malloc(len);
    fseek(fp, 0, SEEK_SET);
    if (buf)
    {
        if (fread(buf, 1, len, fp) != (size_t)len)
        {
            free(buf);
            buf = NULL;
        }
    }
    fclose(fp);
    return (char *)buf;
}

char *read_text_file(const char *fname, size_t *plen)
{
    return do_read_file(fname, "rt", plen);
}

void *read_file(const char *fname, size_t *plen)
{
    return do_read_file(fname, "rb", plen);
}

void dt_set_path(const char *path)
{
    dtpath = path;
}

char *dt_read_prop(const char *node, const char *prop, size_t *plen)
{
    char filename[FILENAME_MAX];
    size_t len;

    len = snprintf(filename, sizeof(filename), "%s%s/%s", dtpath, node, prop);
    if (len >= sizeof(filename))
    {
        assert(0);
        return NULL;
    }

    filename[sizeof(filename) - 1] = '\0';

    return read_file(filename, plen);
}

uint32_t *dt_read_cells(const char *node, const char *prop, unsigned *num_cells)
{
    uint8_t *buf;
    size_t len, i;

    buf = (uint8_t *)dt_read_prop(node, prop, &len);
    if (buf)
    {
        for (i = 0; i + 3 < len; i += 4)
        {
            *(uint32_t *)(buf + i) = (buf[i] << 24) + (buf[i + 1] << 16) +
                                     (buf[i + 2] << 8) + (buf[i + 3] << 0);
        }
        *num_cells = i >> 2;
    }
    return (uint32_t *)buf;
}

uint64_t dt_extract_num(const uint32_t *cells, int size)
{
    uint64_t val = 0;
    int i;

    /* PCIe uses 3 cells for an address, but we can ignore the first cell.
     * In this case, the big-endian representation makes it easy because
     * the unwanted portion is shifted off the top.
     */
    for (i = 0; i < size; i++)
    {
        val = (val << 32) | cells[i];
    }
    return val;
}

uint64_t dt_read_num(const char *node, const char *prop, size_t size)
{
    unsigned num_cells;
    uint32_t *cells = dt_read_cells(node, prop, &num_cells);
    uint64_t val = 0;
    if (cells)
    {
        if (size <= num_cells)
            val = dt_extract_num(cells, size);
        dt_free(cells);
    }
    return val;
}

uint32_t dt_read_u32(const char *node, const char *prop)
{
    return dt_read_num(node, prop, 1);
}

static uint64_t dt_translate_addr(const uint32_t *ranges, unsigned ranges_cells,
                                  int npa, int nps, int nca, uint64_t addr)
{
    /* Entries in the ranges table take the form:
     *   <child addr> <parent addr> <size>
     * where the elements are big-endian numbers of lengths nca, npa and nps
     * respectively.
     */
    unsigned pos = 0;

    while (pos + npa + nps + nca <= ranges_cells)
    {
        uint64_t ca, pa, ps;

        ca = dt_extract_num(ranges + pos, nca);
        pa = dt_extract_num(ranges + pos + nca, npa);
        ps = dt_extract_num(ranges + pos + nca + npa, nps);
        if (addr >= ca && addr <= ca + ps)
        {
            addr -= ca;
            addr += pa;
            break;
        }
        pos += npa + nps + nca;
    }

    return addr;
}

uint64_t dt_parse_addr(const char *node)
{
    char buf1[FILENAME_MAX], buf2[FILENAME_MAX];
    char *parent, *nextparent;
    uint32_t *ranges = NULL;
    unsigned ranges_cells = 0;
    uint64_t addr = INVALID_ADDRESS;
    unsigned npa, nps, nca = 0;

    parent = buf1;
    nextparent = buf2;

    while (1)
    {
        char *tmp, *p;
        strcpy(parent, node);
        p = strrchr(parent, '/');
        if (!p)
            return INVALID_ADDRESS;
        if (p == parent)
            p[1] = '\0';
        else
            p[0] = '\0';

        npa = dt_read_u32(parent, "#address-cells");
        nps = dt_read_u32(parent, "#size-cells");
        if (!npa || !nps)
        {
            addr = INVALID_ADDRESS;
            break;
        }

        if (addr == INVALID_ADDRESS)
        {
            addr = dt_read_num(node, "reg", npa);
        }
        else if (ranges)
        {
            addr = dt_translate_addr(ranges, ranges_cells, npa, nps, nca, addr);
            dt_free(ranges);
            ranges = NULL;
        }

        if (parent[1] == '\0')
            break;

        ranges = dt_read_cells(parent, "ranges", &ranges_cells);
        nca = npa;
        node = parent;
        /* Swap parent and nextparent */
        tmp = parent;
        parent = nextparent;
        nextparent = tmp;
    }

    dt_free(ranges);

    return addr;
}

void dt_free(void *value)
{
    free(value);
}

DT_SUBNODE_HANDLE dt_open_subnodes(const char *node)
{
    char dirpath[FILENAME_MAX];
    size_t len;

    len = snprintf(dirpath, sizeof(dirpath), "%s%s", dtpath, node);
    if (len >= sizeof(dirpath))
    {
        assert(0);
        return NULL;
    }
    return (DT_SUBNODE_HANDLE)opendir(dirpath);
}

const char *dt_next_subnode(DT_SUBNODE_HANDLE handle)
{
    DIR *dirh = (DIR *)handle;
    struct dirent *dent;

    dent = readdir(dirh);

    return dent ? dent->d_name : NULL;
}

void dt_close_subnodes(DT_SUBNODE_HANDLE handle)
{
    DIR *dirh = (DIR *)handle;

    closedir(dirh);
}
