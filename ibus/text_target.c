#include "text_target.h"
#include <string.h>
static const gchar *last_status="not-started";
const gchar *gtv_text_target_status(void){return last_status;}

/* HTML editors preserve visible trailing spaces as NBSP. Keep offsets intact:
 * only this one-codepoint representation difference is accepted. */
static gboolean same_typed_text(const gchar *left,const gchar *right){
    while(*left && *right){
        gunichar a=g_utf8_get_char(left),b=g_utf8_get_char(right);
        if(a==0xa0)a=' ';
        if(b==0xa0)b=' ';
        if(a!=b)return FALSE;
        left=g_utf8_next_char(left);right=g_utf8_next_char(right);
    }
    return !*left && !*right;
}
gboolean gtv_text_range(const gchar *text,gint cursor,gint anchor,const gchar *original,gint *start,gint *end){
    if(!text || !original || !*original || !g_utf8_validate(text,-1,NULL) || !g_utf8_validate(original,-1,NULL))return FALSE;
    gint length=g_utf8_strlen(text,-1),n=g_utf8_strlen(original,-1);
    if(cursor<0 || anchor<0 || cursor>length || anchor>length)return FALSE;
    *end=MAX(cursor,anchor);*start=cursor==anchor ? cursor-n : MIN(cursor,anchor);
    if(*start<0)return FALSE;
    gchar *part=g_utf8_substring(text,*start,*end);
    gboolean matches=same_typed_text(part,original);g_free(part);return matches;
}
static gboolean has_state(AtspiAccessible *obj,AtspiStateType state){
    AtspiStateSet *states=atspi_accessible_get_state_set(obj);
    gboolean yes=states && atspi_state_set_contains(states,state);
    g_clear_object(&states);return yes;
}
static AtspiAccessible *focused_editor(void){
    static gboolean initialized;
    if(!initialized){gint rc=atspi_init();if(rc!=0 && rc!=1)return NULL;initialized=TRUE;}
    atspi_set_timeout(80,80);
    AtspiAccessible *desktop=atspi_get_desktop(0);if(!desktop)return NULL;
    AtspiStateSet *states=atspi_state_set_new(NULL);
    atspi_state_set_add(states,ATSPI_STATE_FOCUSED);atspi_state_set_add(states,ATSPI_STATE_EDITABLE);
    AtspiMatchRule *rule=atspi_match_rule_new(states,ATSPI_Collection_MATCH_ALL,NULL,ATSPI_Collection_MATCH_ALL,
        NULL,ATSPI_Collection_MATCH_ALL,NULL,ATSPI_Collection_MATCH_ALL,FALSE);
    AtspiAccessible *found=NULL;
    gint apps=MIN(atspi_accessible_get_child_count(desktop,NULL),64);
    for(gint i=0;i<apps && !found;i++){
        AtspiAccessible *app=atspi_accessible_get_child_at_index(desktop,i,NULL);if(!app)continue;
        gint windows=MIN(atspi_accessible_get_child_count(app,NULL),32);
        for(gint j=0;j<windows && !found;j++){
            AtspiAccessible *window=atspi_accessible_get_child_at_index(app,j,NULL);if(!window)continue;
            if(has_state(window,ATSPI_STATE_ACTIVE)){
                AtspiCollection *collection=atspi_accessible_get_collection_iface(window);
                GArray *matches=collection ? atspi_collection_get_matches(collection,rule,ATSPI_Collection_SORT_ORDER_CANONICAL,4,TRUE,NULL) : NULL;
                for(guint k=0;matches && k<matches->len && !found;k++){
                    AtspiAccessible *candidate=g_array_index(matches,AtspiAccessible*,k);
                    if(atspi_accessible_get_role(candidate,NULL)!=ATSPI_ROLE_PASSWORD_TEXT)found=g_object_ref(candidate);
                }
                if(matches){for(guint k=0;k<matches->len;k++)g_object_unref(g_array_index(matches,AtspiAccessible*,k));g_array_free(matches,TRUE);}
                g_clear_object(&collection);
            }
            g_object_unref(window);
        }
        g_object_unref(app);
    }
    g_object_unref(rule);g_object_unref(states);g_object_unref(desktop);return found;
}
GtvTextTarget *gtv_text_target_select(const gchar *original,const gchar *replacement){
    last_status="no-active-editable";
    AtspiAccessible *editor=focused_editor();if(!editor)return NULL;
    AtspiText *text=atspi_accessible_get_text_iface(editor);g_object_unref(editor);if(!text)return NULL;
    last_status="text-read-failed";
    GtvTextTarget *target=NULL;gchar *all=NULL;AtspiRange *selection=NULL;GError *error=NULL;
    gint length=atspi_text_get_character_count(text,&error);if(error || length<0 || length>32768)goto done;
    all=atspi_text_get_text(text,0,length,&error);if(error || !all)goto done;
    gint cursor=atspi_text_get_caret_offset(text,&error),anchor=cursor;
    gint selections=atspi_text_get_n_selections(text,&error);if(error)goto done;
    if(selections>0){selection=atspi_text_get_selection(text,0,&error);if(error || !selection)goto done;cursor=selection->start_offset;anchor=selection->end_offset;g_free(selection);selection=NULL;}
    last_status="original-text-mismatch";
    gint start,end;if(!gtv_text_range(all,cursor,anchor,original,&start,&end))goto done;
    last_status="selection-rejected";
    gboolean selected=selections>0 ? atspi_text_set_selection(text,0,start,end,&error) : atspi_text_add_selection(text,start,end,&error);
    if(error || !selected)goto done;
    last_status="selection-pending";
    gchar *before=g_utf8_substring(all,0,start);
    target=g_new0(GtvTextTarget,1);target->text=g_object_ref(text);
    target->original=g_strdup(all);target->start=start;target->end=end;
    target->expected=g_strconcat(before,replacement,g_utf8_offset_to_pointer(all,end),NULL);g_free(before);
done:
    g_clear_error(&error);g_free(selection);g_free(all);g_object_unref(text);return target;
}
/* Chromium acknowledges SetSelection before its renderer updates the range. */
gboolean gtv_text_target_ready(GtvTextTarget *target){
    GError *error=NULL;
    gchar *actual=atspi_text_get_text(target->text,0,-1,&error);
    gboolean same=!error && !g_strcmp0(actual,target->original);
    g_clear_error(&error);g_free(actual);
    if(!same)return FALSE;
    AtspiRange *range=atspi_text_get_selection(target->text,0,&error);
    gboolean ready=!error && range && MIN(range->start_offset,range->end_offset)==target->start
        && MAX(range->start_offset,range->end_offset)==target->end;
    g_clear_error(&error);g_free(range);return ready;
}
gchar *gtv_text_current(gint *caret){
    if(caret)*caret=-1;
    AtspiAccessible *editor=focused_editor();if(!editor)return NULL;
    AtspiText *text=atspi_accessible_get_text_iface(editor);g_object_unref(editor);if(!text)return NULL;
    GError *error=NULL;
    gint length=atspi_text_get_character_count(text,&error);
    gchar *all=NULL;
    if(!error && length>=0 && length<=32768){
        all=atspi_text_get_text(text,0,length,&error);
        if(error){g_free(all);all=NULL;}
    }
    if(all && caret){
        GError *cerr=NULL;
        gint off=atspi_text_get_caret_offset(text,&cerr);
        if(!cerr)*caret=off;
        g_clear_error(&cerr);
    }
    g_clear_error(&error);g_object_unref(text);return all;
}
gboolean gtv_text_target_verify(GtvTextTarget *target){
    GError *error=NULL;gchar *actual=atspi_text_get_text(target->text,0,-1,&error);
    gboolean ok=!error && !g_strcmp0(actual,target->expected);g_clear_error(&error);g_free(actual);return ok;
}
void gtv_text_target_free(GtvTextTarget *target){if(!target)return;g_object_unref(target->text);g_free(target->expected);g_free(target->original);g_free(target);}
