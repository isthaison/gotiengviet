#include "internal.h"
gboolean vni_transform(GArray *buf, gunichar key, gboolean modern) {
    return gtv_apply_key(buf, key, GTV_VNI, modern);
}
