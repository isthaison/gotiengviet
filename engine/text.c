#include "internal.h"

GArray* gstring_to_ucs4(GString *s){
    glong len = 0;
    gunichar *ucs = g_utf8_to_ucs4(s->str, -1, NULL, &len, NULL);
    GArray *arr = g_array_sized_new(FALSE,FALSE,sizeof(gunichar),len);
    if(ucs){
        g_array_append_vals(arr, ucs, len);
        g_free(ucs);
    }
    return arr;
}
void ucs4_to_gstring(GArray *arr, GString *s){
    if (!arr->len) { g_string_truncate(s, 0); return; }
    gchar *utf8 = g_ucs4_to_utf8((gunichar*)arr->data, arr->len, NULL, NULL, NULL);
    if(utf8){
        g_string_assign(s, utf8);
        g_free(utf8);
    } else {
        g_string_assign(s,"");
    }
}

