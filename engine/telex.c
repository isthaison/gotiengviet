#include "internal.h"
gboolean telex_transform(GArray *buf, gunichar key, gboolean modern) {
    return gtv_apply_key(buf, key, GTV_TELEX, modern);
}
