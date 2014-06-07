#include "ipdb.h"
#include "util.h"
#include "cz_update.h"

extern const ipdb_handle qqwry_handle;
extern const ipdb_handle mon17_handle;
extern const ipdb_handle txtdb_handle;

bool qqwry_build(const ipdb *, const char *);

bool make_patch(const ipdb *, const ipdb *);

ipdb* apply_patch(const ipdb *, const uint8_t *, uint32_t);
