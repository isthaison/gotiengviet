/* IBus adapter for the shared C input engine. */
#include <ibus.h>
#include "text_target.h"
#include <linux/input-event-codes.h>
#include <sys/stat.h>
#include "../engine/internal.h"

#include <stdarg.h>
static void debug_log(const char *fmt, ...){
    FILE *f = fopen("/tmp/gotiengviet_debug.log", "a");
    if(!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fclose(f);
}

/* IBus Engine definition */
typedef struct _GoTiengVietEngine IBusGoTiengVietEngine;
typedef struct _GoTiengVietEngineClass IBusGoTiengVietEngineClass;
struct _GoTiengVietEngine {
    IBusEngine parent;
    GString *preedit;
    GString *sentence_context;
    gboolean mode_telex;
    gboolean modern;
    gboolean spellcheck;
    guint caps;
    gboolean assistant_key_down, assistant_running, assistant_inline, assistant_cancelled;
    gchar *focus_id, *target_id, *target_text, *replacement;
    guint target_cursor, target_anchor, target_length;
    guint replacement_wait;
    gboolean replacing;
    gchar *insertion;
    GtvTextTarget *verified_target;
    guint verify_attempts;
    gboolean target_backspaces, cursor_expected, cursor_known;
    IBusRectangle last_cursor;
    GString *typed_text;
    guint purpose;
    gchar **candidates;
    int n_candidates;
    int cand_cursor;
    GtvConfig config;
    GCancellable *ai_cancellable;
    gchar *pending_query;
    guint suggest_timer;
    GHashTable *learned_words;
    gchar *learned_path;
    GHashTable *learned_fixes;
    gchar *learned_fixes_path;
    gboolean pending_bad;
    /* Verified-deletion state for Ctrl+T replacement: 0 none,
     * 1 surrounding-delete sent, 2 backspaces sent; bs_expected is the
     * full client text expected after the deletion. */
    gint bs_mode;
    guint bs_checks;
    gchar *bs_expected;
    gchar *bs_probe;
    gboolean candidates_from_ai;
};
struct _GoTiengVietEngineClass { IBusEngineClass parent; };
G_DEFINE_TYPE(IBusGoTiengVietEngine, ibus_gotiengviet_engine, IBUS_TYPE_ENGINE)

static gchar *word_key(const gchar *word){
    if(!word || !g_utf8_validate(word,-1,NULL))return NULL;
    gchar *lower=g_utf8_strdown(word,-1),*key=g_utf8_normalize(lower,-1,G_NORMALIZE_NFC);g_free(lower);
    if(!*key || g_utf8_strlen(key,-1)>64){g_free(key);return NULL;}
    for(const gchar *p=key;*p;p=g_utf8_next_char(p))if(!g_unichar_isalpha(g_utf8_get_char(p))){g_free(key);return NULL;}
    return key;
}
static void parse_learned_words(IBusGoTiengVietEngine *e,const gchar *contents){
    gchar **lines=g_strsplit_set(contents,"\r\n",10001);
    for(guint i=0;lines[i] && i<10000;i++){
        gchar *line=g_strstrip(lines[i]);
        if(!*line || *line=='#')continue;
        gchar *key=word_key(line);
        if(key && g_hash_table_size(e->learned_words)<10000)g_hash_table_add(e->learned_words,key);
        else g_free(key);
    }
    g_strfreev(lines);
}
/* User vocabulary first; when it does not exist yet, seed from the shipped
 * data/learned-words.txt so common words are accepted offline from the
 * first run. The seed is copied into the user file on the first save,
 * after which the user file is authoritative. */
static void load_learned_words(IBusGoTiengVietEngine *e){
    if(e->learned_words)return;
    e->learned_words=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
    if(!e->learned_path)e->learned_path=g_build_filename(g_get_user_config_dir(),"gotiengviet","learned-words.txt",NULL);
    gboolean have_user=g_file_test(e->learned_path,G_FILE_TEST_EXISTS);
    gchar *contents=NULL;
    if(have_user && g_file_get_contents(e->learned_path,&contents,NULL,NULL)){
        parse_learned_words(e,contents);
        g_free(contents);
        contents=NULL;
    }
    if(!have_user){
        gchar *seed=gtv_data_path("learned-words.txt");
        if(g_file_get_contents(seed,&contents,NULL,NULL)){
            parse_learned_words(e,contents);
            g_free(contents);
        }
        g_free(seed);
    }
}
static void parse_fixes(IBusGoTiengVietEngine *e,const gchar *contents){
    gchar **lines=g_strsplit_set(contents,"\r\n",2001);
    for(guint i=0;lines[i] && i<2000;i++){
        gchar *line=g_strstrip(lines[i]);
        if(!*line || *line=='#')continue;
        gchar *sep=strchr(line,'=');
        if(!sep)continue;
        *sep='\0';
        gchar *bad=word_key(g_strstrip(line)),*good=word_key(g_strstrip(sep+1));
        if(bad && good && strcmp(bad,good) && g_hash_table_size(e->learned_fixes)<2000)
            g_hash_table_replace(e->learned_fixes,bad,good);
        else{g_free(bad);g_free(good);}
    }
    g_strfreev(lines);
}
/* Typo map taught by Ollama (bad=good per line in learned-corrections.txt).
 * User file first; when it does not exist yet, seed from the shipped
 * data/learned-corrections.txt so classic typos are flagged offline
 * from the first run. The seed is copied into the user file on the
 * first save, after which the user file is authoritative. */
static void load_learned_fixes(IBusGoTiengVietEngine *e){
    if(e->learned_fixes)return;
    e->learned_fixes=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
    if(!e->learned_fixes_path)e->learned_fixes_path=g_build_filename(g_get_user_config_dir(),"gotiengviet","learned-corrections.txt",NULL);
    gboolean have_user=g_file_test(e->learned_fixes_path,G_FILE_TEST_EXISTS);
    gchar *contents=NULL;
    if(have_user && g_file_get_contents(e->learned_fixes_path,&contents,NULL,NULL)){
        parse_fixes(e,contents);
        g_free(contents);
        contents=NULL;
    }
    if(!have_user){
        gchar *seed=gtv_data_path("learned-corrections.txt");
        if(g_file_get_contents(seed,&contents,NULL,NULL)){
            parse_fixes(e,contents);
            g_free(contents);
        }
        g_free(seed);
    }
}
static void save_learned_fixes(IBusGoTiengVietEngine *e){
    GString *contents=g_string_new("");GHashTableIter iter;gpointer k,v;
    g_hash_table_iter_init(&iter,e->learned_fixes);
    while(g_hash_table_iter_next(&iter,&k,&v)){g_string_append(contents,k);g_string_append_c(contents,'=');g_string_append(contents,v);g_string_append_c(contents,'\n');}
    gchar *directory=g_path_get_dirname(e->learned_fixes_path);g_mkdir_with_parents(directory,0700);g_free(directory);
    if(!g_file_set_contents(e->learned_fixes_path,contents->str,contents->len,NULL))debug_log("[dictionary] save failed\n");
    g_string_free(contents,TRUE);
}
static void learn_fix(IBusGoTiengVietEngine *e,const gchar *typed,const gchar *correction){
    load_learned_fixes(e);
    gchar *bad=word_key(typed),*good=word_key(correction);
    if(!bad || !good || !strcmp(bad,good)){g_free(bad);g_free(good);return;}
    if(g_hash_table_size(e->learned_fixes)>=2000 || g_hash_table_contains(e->learned_fixes,bad)){g_free(bad);g_free(good);return;}
    g_hash_table_insert(e->learned_fixes,bad,good);
    save_learned_fixes(e);
}
static gchar *learned_fix_for(IBusGoTiengVietEngine *e,const gchar *word){
    load_learned_fixes(e);gchar *key=word_key(word);
    const gchar *found=key ? g_hash_table_lookup(e->learned_fixes,key) : NULL;
    gchar *out=found ? g_strdup(found) : NULL;g_free(key);return out;
}
static gint compare_strings(gconstpointer a,gconstpointer b){
    return strcmp(*(const gchar *const *)a,*(const gchar *const *)b);
}
/* Offline completions from Ollama-taught vocabulary: exact prefix first,
 * then accent-folded prefix (thong matches thông). Sorted and capped. */
static void learned_completions(IBusGoTiengVietEngine *e,const gchar *prefix,GPtrArray *out,guint max){
    if(!prefix || !*prefix || !out || max==0)return;
    load_learned_words(e);
    gchar *lower=word_key(prefix);
    if(!lower)return;
    gchar *folded_prefix=gtv_fold_accents(lower);
    GPtrArray *exact=g_ptr_array_new(),*folded=g_ptr_array_new();
    GHashTableIter iter;gpointer k;
    g_hash_table_iter_init(&iter,e->learned_words);
    while(g_hash_table_iter_next(&iter,&k,NULL)){
        const gchar *w=k;
        if(!g_strcmp0(w,lower))continue;
        if(g_str_has_prefix(w,lower))g_ptr_array_add(exact,(gpointer)w);
        else{
            gchar *folded_w=gtv_fold_accents(w);
            gboolean match=folded_w && g_str_has_prefix(folded_w,folded_prefix);
            g_free(folded_w);
            if(match)g_ptr_array_add(folded,(gpointer)w);
        }
    }
    g_ptr_array_sort(exact,compare_strings);g_ptr_array_sort(folded,compare_strings);
    for(guint i=0;i<exact->len && out->len<max;i++)add_candidate_unique(out,exact->pdata[i]);
    for(guint i=0;i<folded->len && out->len<max;i++)add_candidate_unique(out,folded->pdata[i]);
    g_ptr_array_unref(exact);g_ptr_array_unref(folded);g_free(folded_prefix);g_free(lower);
}
static gboolean word_valid(IBusGoTiengVietEngine *e,const gchar *word){
    load_learned_words(e);load_learned_fixes(e);gchar *key=word_key(word);
    gboolean known=key && g_hash_table_contains(e->learned_words,key);
    gboolean mistyped=key && g_hash_table_contains(e->learned_fixes,key);g_free(key);
    /* Explicitly accepted words win over the typo map: accepting a candidate
     * asserts its validity, while the map only records past corrections. */
    if(known) return TRUE;
    if(mistyped) return FALSE;
    return spell_word_valid(word);
}
static void learn_candidate(IBusGoTiengVietEngine *e,const gchar *candidate){
    if(!e->candidates_from_ai)return;
    /* Remember the Ollama-taught correction so the typo is flagged instantly
     * next time without network. Only in correction mode (pending_bad) with
     * single-word candidates, so mere completions are never marked as typos. */
    if(e->pending_bad && e->pending_query && e->preedit && e->preedit->len &&
       !strcmp(e->pending_query,e->preedit->str))
        learn_fix(e,e->pending_query,candidate);
    load_learned_words(e);gboolean changed=FALSE;
    gchar **words=g_strsplit_set(candidate," \t",-1);
    for(guint i=0;words[i] && g_hash_table_size(e->learned_words)<10000;i++){
        gchar *key=word_key(words[i]);if(key)changed=g_hash_table_add(e->learned_words,key) || changed;
    }
    g_strfreev(words);if(!changed)return;
    GString *contents=g_string_new("");GHashTableIter iter;gpointer key;
    g_hash_table_iter_init(&iter,e->learned_words);
    while(g_hash_table_iter_next(&iter,&key,NULL)){g_string_append(contents,key);g_string_append_c(contents,'\n');}
    gchar *directory=g_path_get_dirname(e->learned_path);g_mkdir_with_parents(directory,0700);g_free(directory);
    if(!g_file_set_contents(e->learned_path,contents->str,contents->len,NULL))debug_log("[dictionary] save failed\n");
    g_string_free(contents,TRUE);
}

static void commit_and_remember(IBusEngine *engine,IBusText *text){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    g_string_append(e->typed_text,text->text);
    if(e->typed_text->len>16000){
        const gchar *start=e->typed_text->str+e->typed_text->len-16000;
        while((*start & 0xc0)==0x80)start++;
        g_string_erase(e->typed_text,0,start-e->typed_text->str);
    }
    ibus_engine_commit_text(engine,text);
}
static void update_context(IBusGoTiengVietEngine *e, const char *committed){
    if(!e->sentence_context || !committed || !*committed) return;
    if(strpbrk(committed, ".!?;\n")){
        g_string_assign(e->sentence_context, "");
        return;
    }
    if(e->sentence_context->len > 0) g_string_append_c(e->sentence_context, ' ');
    g_string_append(e->sentence_context, committed);
    if(e->sentence_context->len > 120){
        gchar **tokens = g_strsplit_set(e->sentence_context->str, " ", -1);
        guint n = g_strv_length(tokens);
        if(n > 3){
            gchar *trimmed = g_strjoin(" ", tokens[n-3], tokens[n-2], tokens[n-1], NULL);
            g_string_assign(e->sentence_context, trimmed);
            g_free(trimmed);
        }
        g_strfreev(tokens);
    }
}

static void clear_candidates(IBusGoTiengVietEngine *e){
    if(e->suggest_timer){g_source_remove(e->suggest_timer);e->suggest_timer=0;}
    if(e->ai_cancellable){
        g_cancellable_cancel(e->ai_cancellable);
        g_clear_object(&e->ai_cancellable);
    }
    g_clear_pointer(&e->pending_query, g_free);
    e->pending_bad=FALSE;
    if(e->candidates){
        for(int i=0;i<e->n_candidates;i++) g_free(e->candidates[i]);
        g_free(e->candidates);
        e->candidates=NULL;
    }
    e->n_candidates=0;
    e->cand_cursor=0;
}
static void hide_suggest(IBusGoTiengVietEngine *e, IBusEngine *engine){
    clear_candidates(e);
    ibus_engine_hide_lookup_table(engine);
}

static void show_candidates(IBusGoTiengVietEngine *e, IBusEngine *engine, GPtrArray *sugs){
    e->candidates_from_ai=FALSE;
    if(e->candidates){
        for(int i=0;i<e->n_candidates;i++) g_free(e->candidates[i]);
        g_free(e->candidates);
        e->candidates=NULL;
    }
    e->n_candidates=0;
    e->cand_cursor=0;
    if(!sugs || sugs->len == 0){
        ibus_engine_hide_lookup_table(engine);
        return;
    }
    e->n_candidates=(int)sugs->len;
    e->cand_cursor=0;
    e->candidates=g_new(gchar*, sugs->len);
    IBusLookupTable *table=ibus_lookup_table_new(5, 0, TRUE, FALSE);
    g_object_ref_sink(table); /* update_lookup_table consumes floating references. */
    for(guint i=0;i<sugs->len;i++){
        e->candidates[i]=g_strdup((char*)sugs->pdata[i]);
        ibus_lookup_table_append_candidate(table, ibus_text_new_from_string(e->candidates[i]));
    }
    ibus_engine_update_lookup_table(engine, table, TRUE);
    g_object_unref(table);
}

static void on_ai_suggestions_ready(GObject *source, GAsyncResult *res, gpointer user_data){
    IBusGoTiengVietEngine *e = user_data;
    GError *error = NULL;
    GPtrArray *sugs = gtv_suggest_combined_finish(res, &error);
    if(error){
        g_clear_error(&error);
        g_object_unref(e);
        return;
    }
    if(sugs && g_task_get_cancellable(G_TASK(res))==e->ai_cancellable && e->pending_query && g_strcmp0(e->preedit->str, e->pending_query) == 0 && e->preedit->len > 0){
        if(sugs->len > 0){
            show_candidates(e, (IBusEngine*)e, sugs);
            e->candidates_from_ai=TRUE;
        }else{
            /* Ollama unreachable or empty: reuse Ollama-taught data offline.
             * Typo mode shows the remembered correction; completion mode
             * completes from learned vocabulary. Not marked as AI results. */
            GPtrArray *local=g_ptr_array_new_with_free_func(g_free);
            if(e->pending_bad){
                gchar *fix=learned_fix_for(e, e->pending_query);
                if(fix) g_ptr_array_add(local, fix);
            }else{
                learned_completions(e, e->pending_query, local, 5);
            }
            show_candidates(e, (IBusEngine*)e, local);
            g_ptr_array_unref(local);
        }
    }
    if(sugs) g_ptr_array_unref(sugs);
    g_object_unref(e);
}

static gboolean request_suggestions(gpointer data){
    IBusGoTiengVietEngine *e=data;e->suggest_timer=0;
    gboolean bad=e->spellcheck && !word_valid(e,e->preedit->str);
    e->pending_bad=bad;
    e->ai_cancellable=g_cancellable_new();
    gtv_suggest_combined_async(&e->config,e->sentence_context->str,e->preedit->str,bad,
        e->ai_cancellable,on_ai_suggestions_ready,g_object_ref(e));
    return G_SOURCE_REMOVE;
}

/* Đẩy preedit + gạch đỏ từ sai + bảng gợi ý (Tab chọn, Up/Down di chuyển, Esc bỏ) */
static void push_preedit(IBusGoTiengVietEngine *e, IBusEngine *engine, guint cursor, gboolean visible){
    glong plen=g_utf8_strlen(e->preedit->str, -1);
    debug_log("[push_preedit] str='%s' plen=%ld visible=%d\n", e->preedit->str, plen, visible);
    gboolean is_emoji=(e->preedit->str[0]==':' || e->preedit->str[0]==';' || e->preedit->str[0]=='<');
    gboolean bad=(!is_emoji && e->spellcheck && plen>=2 && !word_valid(e,e->preedit->str));
    IBusText *t=ibus_text_new_from_string(e->preedit->str);
    if(bad) ibus_text_append_attribute(t, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_ERROR, 0, (gint)plen);
    else ibus_text_append_attribute(t, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, (gint)plen);
    ibus_engine_update_preedit_text_with_mode(engine, t, (guint)plen, visible, IBUS_ENGINE_PREEDIT_COMMIT);
    if(plen > 0 && visible){
        ibus_engine_show_preedit_text(engine);
    } else {
        ibus_engine_hide_preedit_text(engine);
    }

    if(e->suggest_timer){g_source_remove(e->suggest_timer);e->suggest_timer=0;}
    if(e->ai_cancellable){
        g_cancellable_cancel(e->ai_cancellable);
        g_clear_object(&e->ai_cancellable);
    }
    g_clear_pointer(&e->pending_query, g_free);
    e->pending_bad=FALSE;

    if(plen == 0 || !visible){
        hide_suggest(e, engine);
        return;
    }

    if(is_emoji && plen >= 2){
        GPtrArray *sugs = get_emoji_suggestions(e->preedit->str);
        show_candidates(e, engine, sugs);
        if(sugs) g_ptr_array_free(sugs, TRUE);
        return;
    }

    show_candidates(e,engine,NULL);
    if(e->config.ai_enabled && e->purpose!=IBUS_INPUT_PURPOSE_PASSWORD && e->purpose!=IBUS_INPUT_PURPOSE_PIN){
        e->pending_query=g_strdup(e->preedit->str);
        e->suggest_timer=g_timeout_add_full(G_PRIORITY_DEFAULT,350,request_suggestions,g_object_ref(e),g_object_unref);
    }
}

static void ibus_gotiengviet_engine_reset(IBusGoTiengVietEngine *e){
    if(e->suggest_timer){g_source_remove(e->suggest_timer);e->suggest_timer=0;}
    if(e->ai_cancellable){
        g_cancellable_cancel(e->ai_cancellable);
        g_clear_object(&e->ai_cancellable);
    }
    g_clear_pointer(&e->pending_query, g_free);
    e->pending_bad=FALSE;
    if(e->preedit) g_string_assign(e->preedit,"");
    ibus_engine_hide_preedit_text((IBusEngine*)e);
}
// Tray đổi method khi đang gõ không gây focus_in, nên reload config theo mtime mỗi phím
static void reload_engine_config(IBusGoTiengVietEngine *e){
    gchar *directory = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gtv_config_clear(&e->config);
    gtv_config_load(&e->config, directory);
    e->mode_telex = (e->config.mode == GTV_TELEX);
    e->modern = e->config.modern;
    e->spellcheck = e->config.spellcheck;
    g_free(directory);
}
static time_t cfg_mtime_cache = 0;
static void reload_config_if_changed(IBusGoTiengVietEngine *e){
    gchar *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    struct stat st;
    if(stat(path, &st) == 0 && st.st_mtime != cfg_mtime_cache){
        cfg_mtime_cache = st.st_mtime;
        reload_engine_config(e);
    }
    g_free(path);
}
static gchar *assistant_input(const gchar *text,guint cursor,guint anchor,const gchar *preedit){
    gchar *input=NULL;
    if(text && g_utf8_validate(text,-1,NULL)){
        guint length=g_utf8_strlen(text,-1);
        cursor=MIN(cursor,length);anchor=MIN(anchor,length);
        guint start=MIN(cursor,anchor),end=MAX(cursor,anchor);
        if(start==end){
            const gchar *stop=g_utf8_offset_to_pointer(text,cursor);
            const gchar *begin=stop;
            while(begin>text && stop-begin<16000){
                const gchar *previous=g_utf8_prev_char(begin);
                if(*previous=='\n')break;
                begin=previous;
            }
            input=g_strndup(begin,stop-begin);
        }else input=g_utf8_substring(text,start,MIN(end,start+8000));
    }
    if(!input)input=g_strdup("");
    gchar *combined=g_strconcat(input,cursor==anchor ? preedit : "",NULL);g_free(input);
    return combined;
}
/* A Backspace fallback is limited to the plain Vietnamese text we just emitted.
 * Reject marks/emoji/control sequences: clients may erase those by grapheme. */
static gboolean backspace_text_supported(const gchar *text){
    if(!text || !*text || g_utf8_strlen(text,-1)>512)return FALSE;
    for(const gchar *p=text;*p;p=g_utf8_next_char(p)){
        gunichar c=g_utf8_get_char(p);
        if(c>0x1eff || g_unichar_ismark(c) || !g_unichar_isprint(c))return FALSE;
    }
    return TRUE;
}
static gboolean is_modifier_key(guint keyval){
    switch(keyval){
        case IBUS_Shift_L: case IBUS_Shift_R:
        case IBUS_Control_L: case IBUS_Control_R:
        case IBUS_Alt_L: case IBUS_Alt_R:
        case IBUS_Super_L: case IBUS_Super_R:
        case IBUS_Hyper_L: case IBUS_Hyper_R:
        case IBUS_Meta_L: case IBUS_Meta_R:
        case IBUS_Caps_Lock: case IBUS_Shift_Lock:
        case IBUS_Num_Lock: case IBUS_Scroll_Lock:
        case IBUS_ISO_Level3_Shift: case IBUS_Mode_switch:
            return TRUE;
        default:
            return FALSE;
    }
}
static GtvTextTarget *(*select_text_target)(const gchar*,const gchar*)=gtv_text_target_select;
static gboolean verify_replacement(gpointer data){
    IBusGoTiengVietEngine *e=data;
    gboolean verified=gtv_text_target_verify(e->verified_target);
    if(!verified && ++e->verify_attempts<20)return G_SOURCE_CONTINUE;
    debug_log("[assistant-state] verified-replacement=%d\n",verified);
    e->replacing=FALSE;
    if(verified)ibus_engine_hide_auxiliary_text((IBusEngine*)e);
    else ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string("Ứng dụng chưa xác nhận thay câu. Không tự xóa hoặc thử chèn lại."),TRUE);
    gtv_text_target_free(e->verified_target);e->verified_target=NULL;return G_SOURCE_REMOVE;
}
static gboolean commit_selected_replacement(gpointer data){
    IBusGoTiengVietEngine *e=data;
    gboolean valid=e->replacement && !e->assistant_cancelled && !e->preedit->len
        && !g_strcmp0(e->focus_id,e->target_id);
    if(valid && gtv_text_target_ready(e->verified_target)){
        e->verify_attempts=0;e->cursor_expected=TRUE;
        g_string_truncate(e->typed_text,0);
        commit_and_remember((IBusEngine*)e,ibus_text_new_from_string(e->replacement));
        g_clear_pointer(&e->replacement,g_free);
        ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string("Đang xác nhận câu đã thay…"),TRUE);
        g_timeout_add_full(G_PRIORITY_DEFAULT,100,verify_replacement,g_object_ref(e),g_object_unref);
        return G_SOURCE_REMOVE;
    }
    if(valid && ++e->verify_attempts<20)return G_SOURCE_CONTINUE;
    e->replacing=FALSE;
    gtv_text_target_free(e->verified_target);e->verified_target=NULL;
    if(valid)ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string("Ứng dụng chưa chọn được đầy đủ câu gốc. Nhấn Enter để thử lại."),TRUE);
    return G_SOURCE_REMOVE;
}
static gboolean snapshot_text(IBusGoTiengVietEngine *e,gchar **text,gint *cursor,gint *anchor);
static gchar *expected_after_delete(const gchar *text,gint caret,guint len);
static void send_word_backspaces(IBusEngine *engine,guint count);
static void cleanup_bs(IBusGoTiengVietEngine *e);
static gboolean verify_deletion(gpointer data);
static gboolean apply_replacement(gpointer data){
    IBusGoTiengVietEngine *e=data;
    if(++e->replacement_wait>100){
        g_clear_pointer(&e->replacement,g_free);return G_SOURCE_REMOVE;
    }
    debug_log("[assistant-state] apply cancelled=%d preedit=%u focus-match=%d history-match=%d fallback=%d\n",e->assistant_cancelled,(guint)e->preedit->len,!g_strcmp0(e->focus_id,e->target_id),!g_strcmp0(e->typed_text->str,e->target_text),e->target_backspaces);
    if(e->preedit->len || e->assistant_cancelled)return G_SOURCE_CONTINUE;
    if(e->focus_id && e->target_id && g_strcmp0(e->focus_id,e->target_id)!=0)return G_SOURCE_CONTINUE;
    if(e->purpose==IBUS_INPUT_PURPOSE_PASSWORD || e->purpose==IBUS_INPUT_PURPOSE_PIN)return G_SOURCE_CONTINUE;
    /* AT-SPI selection+replace needs a modern client; without surrounding
     * support the original text would stay and the commit insert on top. */
    if(!e->target_backspaces && (e->caps & IBUS_CAP_SURROUNDING_TEXT)){
        gchar *original=assistant_input(e->target_text,e->target_cursor,e->target_anchor,"");
        e->verified_target=select_text_target(original,e->replacement);g_free(original);
        debug_log("[assistant-state] selection=%s\n",gtv_text_target_status());
        if(e->verified_target){
            e->replacing=TRUE;e->verify_attempts=0;
            g_timeout_add_full(G_PRIORITY_DEFAULT,25,commit_selected_replacement,g_object_ref(e),g_object_unref);
            return G_SOURCE_REMOVE;
        }
    }
    /* Terminal-safe deletion: some clients (notably VTE terminals) ignore
     * DeleteSurroundingText and replace nothing on commit, so committing
     * right away would INSERT the replacement on top of the original.
     * Delete first, re-read, and commit only when the original text is
     * really gone. Full-text comparison (not a range check) also stays
     * correct when the original phrase repeats nearby. */
    if(!e->target_backspaces){
        IBusText *text=NULL;guint cursor=0,anchor=0;
        ibus_engine_get_surrounding_text((IBusEngine*)e,&text,&cursor,&anchor);
        gboolean matches=text && !g_strcmp0(text->text,e->target_text) && cursor==e->target_cursor && anchor==e->target_anchor;
        if(matches){
            gint start=0,end=0;
            if(gtv_text_range(text->text,cursor,anchor,e->target_text,&start,&end)){
                gchar *before=g_utf8_substring(text->text,0,start);
                g_clear_pointer(&e->bs_expected,g_free);
                g_clear_pointer(&e->bs_probe,g_free);
                e->bs_expected=g_strconcat(before,g_utf8_offset_to_pointer(text->text,end),NULL);
                e->bs_probe=g_utf8_substring(text->text,start,end);
                g_free(before);
                gint offset=cursor==anchor ? -(gint)e->target_length : (gint)MIN(cursor,anchor)-(gint)cursor;
                ibus_engine_delete_surrounding_text((IBusEngine*)e,offset,e->target_length);
                debug_log("[assistant-state] delete-sent len=%u, verifying\n",e->target_length);
                e->bs_mode=1;e->bs_checks=0;e->replacing=TRUE;
                g_timeout_add_full(G_PRIORITY_DEFAULT,150,verify_deletion,g_object_ref(e),g_object_unref);
                g_clear_object(&text);
                return G_SOURCE_REMOVE;
            }
        }
        g_clear_object(&text);
    }
    /* Backspace fallback: exact character count, still verified before the
     * commit below. Flood guard keeps long pastes from stalling the client. */
    if(e->target_length==0){
        commit_and_remember((IBusEngine*)e,ibus_text_new_from_string(e->replacement));
        g_clear_pointer(&e->replacement,g_free);
        return G_SOURCE_REMOVE;
    }
    if(e->target_length<=300){
        gchar *cur=NULL;gint cur_c=-1,cur_a=-1;
        if(snapshot_text(e,&cur,&cur_c,&cur_a) && cur_c==cur_a){
            gint s=0,en=0;
            if(gtv_text_range(cur,cur_c,cur_a,e->target_text,&s,&en)){
                g_clear_pointer(&e->bs_expected,g_free);
                g_clear_pointer(&e->bs_probe,g_free);
                e->bs_expected=expected_after_delete(cur,cur_c,e->target_length);
                e->bs_probe=g_utf8_substring(cur,s,en);
            }
        }
        g_free(cur);
        if(e->bs_expected){
            send_word_backspaces((IBusEngine*)e,e->target_length);
            debug_log("[assistant-state] backspaces-sent len=%u, verifying\n",e->target_length);
            e->bs_mode=2;e->bs_checks=0;e->replacing=TRUE;
            g_timeout_add_full(G_PRIORITY_DEFAULT,150,verify_deletion,g_object_ref(e),g_object_unref);
            return G_SOURCE_REMOVE;
        }
    }
    /* Keep the replacement for a later Enter retry; the caller shows the
     * retry message. Never insert blindly. */
    debug_log("[assistant-state] delete-unverifiable, kept for retry\n");
    return G_SOURCE_CONTINUE;
}
/* Full client text + caret/anchor snapshot. AT-SPI side never selects. */
static gboolean snapshot_text(IBusGoTiengVietEngine *e,gchar **text,gint *cursor,gint *anchor){
    *text=NULL;*cursor=-1;*anchor=-1;
    if(e->caps & IBUS_CAP_SURROUNDING_TEXT){
        IBusText *t=NULL;guint c=0,a=0;
        ibus_engine_get_surrounding_text((IBusEngine*)e,&t,&c,&a);
        if(t&&t->text){*text=g_strdup(t->text);*cursor=(gint)c;*anchor=(gint)a;}
        g_clear_object(&t);
        return *text!=NULL;
    }
    *text=gtv_text_current(cursor);
    if(*text)*anchor=*cursor;
    return *text!=NULL;
}
/* Expected full text after deleting len chars before caret (owned/NULL). */
static gchar *expected_after_delete(const gchar *text,gint caret,guint len){
    if(!text||!g_utf8_validate(text,-1,NULL))return NULL;
    glong tlen=g_utf8_strlen(text,-1);
    if(caret<0||caret>tlen||len>(guint)caret)return NULL;
    gchar *head=g_utf8_substring(text,0,caret-(gint)len);
    gchar *out=g_strconcat(head,g_utf8_offset_to_pointer(text,caret),NULL);
    g_free(head);return out;
}
static void send_word_backspaces(IBusEngine *engine,guint count){
    for(guint i=0;i<count;i++){
        ibus_engine_forward_key_event(engine,IBUS_BackSpace,0,0);
        ibus_engine_forward_key_event(engine,IBUS_BackSpace,0,IBUS_RELEASE_MASK);
    }
}
static void cleanup_bs(IBusGoTiengVietEngine *e){
    g_clear_pointer(&e->bs_expected,g_free);
    g_clear_pointer(&e->bs_probe,g_free);
    if(e->bs_mode!=0){e->bs_mode=0;e->replacing=FALSE;}
    e->bs_checks=0;
}
/* Runs 150ms after a deletion. Commit only on full-text match; the probe
 * (exact segment armed for deletion) aborts fast when the text changed
 * under us, so a stale length can never eat neighboring characters.
 * The replacement is kept for Enter retry; nothing is ever inserted
 * without a verified deletion. */
static gboolean verify_deletion(gpointer data){
    IBusGoTiengVietEngine *e=data;
    gboolean live=e->replacement && !e->assistant_cancelled && !e->preedit->len
        && !(e->focus_id && e->target_id && g_strcmp0(e->focus_id,e->target_id))
        && e->purpose!=IBUS_INPUT_PURPOSE_PASSWORD && e->purpose!=IBUS_INPUT_PURPOSE_PIN
        && e->bs_expected && e->bs_probe && e->target_length>0;
    if(live){
        gchar *cur=NULL;gint c=-1,a=-1;
        gboolean have=snapshot_text(e,&cur,&c,&a);
        gboolean gone=have && !g_strcmp0(cur,e->bs_expected);
        gboolean intact=FALSE;
        if(!gone && have && c==a && e->target_length>0 && (guint)c>=e->target_length){
            gchar *seg=g_utf8_substring(cur,c-(gint)e->target_length,c);
            intact=seg && !g_strcmp0(seg,e->bs_probe);
            g_free(seg);
        }
        debug_log("[assistant-state] verify mode=%d checks=%u gone=%d intact=%d\n",
                  e->bs_mode,e->bs_checks,gone,intact);
        if(gone){
            g_free(cur);
            commit_and_remember((IBusEngine*)e,ibus_text_new_from_string(e->replacement));
            g_clear_pointer(&e->replacement,g_free);
            ibus_engine_hide_auxiliary_text((IBusEngine*)e);
            cleanup_bs(e);
            return G_SOURCE_REMOVE;
        }
        if(!intact){
            g_free(cur);
            debug_log("[assistant-state] segment changed under us, kept for retry\n");
            ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string("Câu gốc đã đổi trong lúc thay thế. Nhấn Enter để thử lại, Esc để hủy."),TRUE);
            cleanup_bs(e);
            return G_SOURCE_REMOVE;
        }
        if(e->bs_mode==1 && e->target_length<=300){
            /* Surrounding delete was ignored: fall back to Backspaces,
             * recomputing the expectation from this fresh snapshot. */
            g_clear_pointer(&e->bs_expected,g_free);
            e->bs_expected=expected_after_delete(cur,c,e->target_length);
            g_free(cur);
            if(!e->bs_expected){
                ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string("Không xác nhận được câu gốc trong ô nhập. Nhấn Enter để thử lại, Ctrl+T để lấy lại câu, Esc để hủy."),TRUE);
                cleanup_bs(e);
                return G_SOURCE_REMOVE;
            }
            send_word_backspaces((IBusEngine*)e,e->target_length);
            e->bs_mode=2;e->bs_checks=0;
            return G_SOURCE_CONTINUE;
        }
        g_free(cur);
        if(++e->bs_checks<=12)return G_SOURCE_CONTINUE;
    }
    if(e->replacement){
        debug_log("[assistant-state] delete-unverifiable, kept for retry\n");
        ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string("Không xác nhận được câu gốc trong ô nhập. Nhấn Enter để thử lại, Ctrl+T để lấy lại câu, Esc để hủy."),TRUE);
    }else{
        ibus_engine_hide_auxiliary_text((IBusEngine*)e);
    }
    cleanup_bs(e);
    return G_SOURCE_REMOVE;
}
/* Reply to ProcessKeyEvent before querying the client's accessibility service.
 * A synchronous IBus client cannot service AT-SPI calls while waiting for Enter. */
static gboolean accept_replacement(gpointer data){
    IBusGoTiengVietEngine *e=data;
    e->replacing=FALSE;
    if(!e->replacement || e->assistant_cancelled)return G_SOURCE_REMOVE;
    if(!apply_replacement(e)){
        if(!e->replacing)ibus_engine_hide_auxiliary_text((IBusEngine*)e);
    }else ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string("Không xác nhận được câu gốc trong ô nhập. Ctrl+T để lấy lại câu; Esc để hủy."),TRUE);
    return G_SOURCE_REMOVE;
}
static void assistant_closed(GObject *object,GAsyncResult *result,gpointer data){
    IBusGoTiengVietEngine *e=data;gchar *output=NULL;GError *error=NULL;
    gboolean ok=g_subprocess_communicate_utf8_finish(G_SUBPROCESS(object),result,&output,NULL,&error);
    e->assistant_running=FALSE;
    debug_log("[assistant-state] response ok=%d cancelled=%d length=%zu\n",ok,e->assistant_cancelled,output?strlen(output):0);
    if(e->assistant_cancelled){
        g_free(output);g_clear_error(&error);g_object_unref(e);return;
    }
    if(ok && g_subprocess_get_successful(G_SUBPROCESS(object)) && output && *output && e->target_id){
        g_free(e->replacement);e->replacement=g_strdup(output);e->replacement_wait=0;
        if(e->assistant_inline){
            gchar *preview=g_strdup_printf("%s\nEnter: thay câu · Tab: đổi viết lại/dịch · Esc: hủy",output);
            ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string(preview),TRUE);
            g_free(preview);
        }else g_timeout_add_full(G_PRIORITY_DEFAULT,50,apply_replacement,g_object_ref(e),g_object_unref);
    }
    if(e->assistant_inline && (!ok || !g_subprocess_get_successful(G_SUBPROCESS(object)) || !output || !*output))
        ibus_engine_update_auxiliary_text((IBusEngine*)e,ibus_text_new_from_string("Ollama chưa trả kết quả. Ctrl+T để thử lại."),TRUE);
    g_free(output);g_clear_error(&error);g_object_unref(e);
}
static void toggle_assistant_action(void){
    gchar *directory=g_build_filename(g_get_user_config_dir(),"gotiengviet",NULL);
    gchar *path=g_build_filename(directory,"assistant.conf",NULL);GKeyFile *file=g_key_file_new();
    g_key_file_load_from_file(file,path,G_KEY_FILE_NONE,NULL);
    gint action=g_key_file_get_integer(file,"assistant","action",NULL);
    g_key_file_set_integer(file,"assistant","action",action==1?0:1);
    if(g_mkdir_with_parents(directory,0700)==0)g_key_file_save_to_file(file,path,NULL);
    g_key_file_unref(file);g_free(path);g_free(directory);
}
static gboolean open_text_assistant(IBusEngine *engine){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    if(e->assistant_running || e->replacement || e->replacing)return TRUE;
    IBusText *surrounding=NULL;guint cursor=0,anchor=0;
    ibus_engine_get_surrounding_text(engine,&surrounding,&cursor,&anchor);
    if(surrounding && surrounding->text){
        guint length=g_utf8_strlen(surrounding->text,-1);
        cursor=MIN(cursor,length);anchor=MIN(anchor,length);
    }
    gchar *combined=assistant_input(surrounding ? surrounding->text : NULL,cursor,anchor,e->preedit->str);
    if(!surrounding || !surrounding->text || !*surrounding->text){
        g_free(combined);combined=g_strconcat(e->typed_text->str,e->preedit->str,NULL);
    }
    e->target_backspaces=FALSE;
    g_clear_pointer(&e->target_id,g_free);g_clear_pointer(&e->target_text,g_free);
    if(surrounding && surrounding->text && (*surrounding->text || !e->typed_text->len) && e->focus_id && (e->caps & IBUS_CAP_SURROUNDING_TEXT)){
        e->target_id=g_strdup(e->focus_id);
        e->target_text=g_strdup(surrounding->text);
        e->target_cursor=cursor;e->target_anchor=anchor;
        e->target_length=g_utf8_strlen(combined,-1);
        if(e->preedit->len && cursor==anchor){
            gchar *before=g_utf8_substring(e->target_text,0,cursor);
            gchar *next=g_strconcat(before,e->preedit->str,g_utf8_offset_to_pointer(e->target_text,cursor),NULL);
            g_free(before);g_free(e->target_text);e->target_text=next;
            e->target_cursor+=g_utf8_strlen(e->preedit->str,-1);e->target_anchor=e->target_cursor;
        }
    }
    if(!e->target_id && e->focus_id){
        gchar *recent=g_strconcat(e->typed_text->str,e->preedit->str,NULL);
        if(backspace_text_supported(recent)){
            e->target_backspaces=TRUE;e->target_id=g_strdup(e->focus_id);
            e->target_text=g_strdup(recent);e->target_length=g_utf8_strlen(recent,-1);
            g_free(combined);combined=g_strdup(recent);
        }
        g_free(recent);
    }
    debug_log("[assistant] caps=0x%x focus=%d backspaces=%d length=%u\n",e->caps,e->focus_id!=NULL,e->target_backspaces,e->target_length);
    g_clear_object(&surrounding);
    GError *error=NULL;
    e->assistant_inline=e->target_id!=NULL;
    GSubprocess *child=g_subprocess_new(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE,&error,"gotiengviet-assistant","--stdin",e->assistant_inline ? "--headless" : NULL,NULL);
    if(!child){g_warning("Cannot open text assistant: %s",error->message);g_clear_error(&error);g_free(combined);return FALSE;}
    e->cursor_expected=e->preedit->len>0;
    e->assistant_cancelled=FALSE;
    e->assistant_running=TRUE;
    if(e->assistant_inline)ibus_engine_update_auxiliary_text(engine,ibus_text_new_from_string("Ollama đang xử lý…"),TRUE);
    g_subprocess_communicate_utf8_async(child,combined,NULL,assistant_closed,g_object_ref(e));
    g_object_unref(child);g_free(combined);
    if(e->preedit->len){
        commit_and_remember(engine,ibus_text_new_from_string(e->preedit->str));
        ibus_gotiengviet_engine_reset(e);
        ibus_engine_update_preedit_text(engine,ibus_text_new_from_string(""),0,FALSE);
        hide_suggest(e,engine);
    }
    return TRUE;
}
static gboolean ibus_gotiengviet_engine_process_key_event(IBusEngine *engine, guint keyval, guint keycode, guint modifiers){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    reload_config_if_changed(e);
    debug_log("[key] kv=0x%x ('%c') kc=%u mod=0x%x preedit='%s' purpose=%u caps=0x%x\n",
              keyval, (keyval>32 && keyval<127)?(char)keyval:' ', keycode, modifiers, e->preedit->str, e->purpose, e->caps);
    if(modifiers & IBUS_FORWARD_MASK)return FALSE;
    if(modifiers & IBUS_RELEASE_MASK){
        if(keyval==IBUS_t || keyval==IBUS_T){
            gboolean handled=e->assistant_key_down;e->assistant_key_down=FALSE;return handled;
        }
        return FALSE;
    }
    if(e->replacing){
        if(keyval==IBUS_Return || keyval==IBUS_KP_Enter || is_modifier_key(keyval))return TRUE;
        debug_log("[assistant-state] interrupted key=0x%x modifiers=0x%x\n",keyval,modifiers);
        e->assistant_cancelled=TRUE;ibus_engine_hide_auxiliary_text(engine);
        if(keyval==IBUS_Escape)return TRUE;
    }
    gboolean bare_key=!(modifiers & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK | IBUS_MOD4_MASK | IBUS_SHIFT_MASK));
    if(e->assistant_inline && e->assistant_running &&
       (keyval==IBUS_Return || keyval==IBUS_KP_Enter) && bare_key){
        debug_log("[assistant-state] enter-before-ready\n");
        ibus_engine_update_auxiliary_text(engine,ibus_text_new_from_string("Ollama đang xử lý, chưa có kết quả để thay."),TRUE);
        return TRUE;
    }
    if(e->assistant_inline && e->assistant_running && !is_modifier_key(keyval) &&
       !((keyval==IBUS_t || keyval==IBUS_T) && (modifiers & IBUS_CONTROL_MASK))){
        e->assistant_cancelled=TRUE;ibus_engine_hide_auxiliary_text(engine);
        if(keyval==IBUS_Escape)return TRUE;
    }
    if(e->assistant_inline && e->replacement){
        if(keyval==IBUS_Tab && bare_key){
            g_clear_pointer(&e->replacement,g_free);toggle_assistant_action();return open_text_assistant(engine);
        }
        if((keyval==IBUS_Return || keyval==IBUS_KP_Enter) && bare_key){
            if(e->bs_mode!=0)return TRUE; /* deletion verification in flight */
            e->replacement_wait=0;
            e->replacing=TRUE;
            g_timeout_add_full(G_PRIORITY_DEFAULT,25,accept_replacement,g_object_ref(e),g_object_unref);
            return TRUE;
        }
        if(keyval==IBUS_Escape){
            g_clear_pointer(&e->replacement,g_free);ibus_engine_hide_auxiliary_text(engine);return TRUE;
        }
        if(!is_modifier_key(keyval)){
            g_clear_pointer(&e->replacement,g_free);ibus_engine_hide_auxiliary_text(engine);
        }
    }
    /* Let the desktop handle Shift/locks/modifiers without committing a partial word.
     * Character case comes from keyval, already resolved by the keyboard layout. */
    if(is_modifier_key(keyval)) return FALSE;
    if((keyval==IBUS_t || keyval==IBUS_T) &&
       (modifiers & IBUS_CONTROL_MASK) &&
       !(modifiers & (IBUS_SUPER_MASK | IBUS_MOD4_MASK | IBUS_MOD1_MASK | IBUS_SHIFT_MASK)) &&
       e->purpose!=IBUS_INPUT_PURPOSE_PASSWORD && e->purpose!=IBUS_INPUT_PURPOSE_PIN)
    {
        if(e->assistant_key_down)return TRUE;
        e->assistant_key_down=open_text_assistant(engine);
        return e->assistant_key_down;
    }
    // Phím tắt Ctrl/Alt/Super (Ctrl+C/V/X/Z, Ctrl+S...) — commit chữ đang dở rồi nhường cho app
    if(modifiers & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK | IBUS_HYPER_MASK | IBUS_META_MASK)){
        if(e->preedit->len>0){
            IBusText *t=ibus_text_new_from_string(e->preedit->str);
            commit_and_remember(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
        }
        g_string_truncate(e->typed_text,0);
        return FALSE;
    }
    // Trường mật khẩu (Password / PIN): Không can thiệp preedit, nhường phím trực tiếp
    if(e->purpose == IBUS_INPUT_PURPOSE_PASSWORD || e->purpose == IBUS_INPUT_PURPOSE_PIN){
        return FALSE;
    }
    // Esc: hủy preedit nếu đang gõ dở
    if(keyval == IBUS_Escape){
        g_string_truncate(e->typed_text,0);
        if(e->preedit->len > 0){
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty = ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine, empty, 0, FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        hide_suggest(e, engine);
        return FALSE;
    }
    if(keyval == '.' || keyval == '?' || keyval == '!' || keyval == ';' || keyval == '\n'){
        if(e->sentence_context) g_string_assign(e->sentence_context, "");
    }
    // Tab chọn gợi ý, 1..5 chọn nhanh, Up/Down di chuyển, Enter commit gợi ý, Esc bỏ bảng gợi ý
    if(e->n_candidates>0 && e->candidates){
        if(keyval==IBUS_Tab){
            learn_candidate(e,e->candidates[e->cand_cursor]);
            g_string_assign(e->preedit, e->candidates[e->cand_cursor]);
            push_preedit(e, engine, e->preedit->len, TRUE);
            return TRUE;
        }
        if(e->mode_telex && ((keyval>=IBUS_1 && keyval<=IBUS_5) || (keyval>=IBUS_KP_1 && keyval<=IBUS_KP_5))){
            int idx = (keyval>=IBUS_1 && keyval<=IBUS_5) ? (keyval - IBUS_1) : (keyval - IBUS_KP_1);
            if(idx < e->n_candidates){
                learn_candidate(e,e->candidates[idx]);
                g_string_assign(e->preedit, e->candidates[idx]);
                gchar *word=expand_word(e->preedit->str);
                update_context(e, word);
                IBusText *t=ibus_text_new_from_string(word);
                g_free(word);
                commit_and_remember(engine,t);
                ibus_gotiengviet_engine_reset(e);
                IBusText *empty=ibus_text_new_from_string("");
                ibus_engine_update_preedit_text(engine,empty,0,FALSE);
                hide_suggest(e, engine);
                return TRUE;
            }
        }
        if(keyval==IBUS_Up || keyval==IBUS_Down){
            if(keyval==IBUS_Down) e->cand_cursor=(e->cand_cursor+1)%e->n_candidates;
            else e->cand_cursor=(e->cand_cursor+e->n_candidates-1)%e->n_candidates;
            IBusLookupTable *table=ibus_lookup_table_new(5, (guint)e->cand_cursor, TRUE, FALSE);
            g_object_ref_sink(table);
            for(int i=0;i<e->n_candidates;i++)
                ibus_lookup_table_append_candidate(table, ibus_text_new_from_string(e->candidates[i]));
            ibus_engine_update_lookup_table(engine, table, TRUE);
            g_object_unref(table);
            return TRUE;
        }
        if(keyval==IBUS_Return || keyval==IBUS_KP_Enter){
            learn_candidate(e,e->candidates[e->cand_cursor]);
            g_string_assign(e->preedit, e->candidates[e->cand_cursor]);
            gchar *word=expand_word(e->preedit->str);
            update_context(e, word);
            IBusText *t=ibus_text_new_from_string(word);
            g_free(word);
            commit_and_remember(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        if(keyval==IBUS_Escape){
            hide_suggest(e, engine);
            return TRUE;
        }
    }
    // Backspace
    if(keyval==IBUS_BackSpace){
        e->cursor_expected=TRUE;
        if(e->preedit->len>0){
            // remove last utf8 char
            gchar *prev = g_utf8_prev_char(e->preedit->str + e->preedit->len);
            g_string_truncate(e->preedit, prev - e->preedit->str);
            push_preedit(e, engine, e->preedit->len, TRUE);
            return TRUE;
        }
        if(e->typed_text->len){
            gchar *prev=g_utf8_prev_char(e->typed_text->str+e->typed_text->len);
            g_string_truncate(e->typed_text,prev-e->typed_text->str);
        }
        return FALSE;
    }
    if(keyval==IBUS_space){
        e->cursor_expected=TRUE;
        if(e->preedit->len>0){
            gchar *commit = expand_word(e->preedit->str);
            update_context(e, commit);
            gchar *with_space = g_strdup_printf("%s ",commit);
            IBusText *t=ibus_text_new_from_string(with_space);
            commit_and_remember(engine,t);
            g_free(commit); g_free(with_space);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        g_string_append_c(e->typed_text,' ');
        return FALSE;
    }
    // All printable keys use the same C composer as the CLI and tests.
    gunichar key = ibus_keyval_to_unicode(keyval);
    if(key >= 0x20 && g_unichar_isprint(key)){
        e->cursor_expected=TRUE;
        GtvConfig config = {.mode = e->mode_telex ? GTV_TELEX : GTV_VNI,
                            .modern = e->modern, .spellcheck = FALSE};
        GtvEngine *composer = gtv_engine_new(&config);
        g_array_unref(composer->buffer);
        composer->buffer = gstring_to_ucs4(e->preedit);
        gchar *commit = gtv_engine_process(composer, key, NULL);
        if(commit){
            update_context(e, commit);
            commit_and_remember(engine, ibus_text_new_from_string(commit));
            g_free(commit);
            ibus_gotiengviet_engine_reset(e);
            ibus_engine_update_preedit_text(engine, ibus_text_new_from_string(""), 0, FALSE);
            hide_suggest(e, engine);
        } else {
            ucs4_to_gstring(composer->buffer, e->preedit);
            push_preedit(e, engine, e->preedit->len, TRUE);
        }
        gtv_engine_free(composer);
        return TRUE;
    }
    // Enter -> commit
    if(keyval==IBUS_Return || keyval==IBUS_KP_Enter){
        if(e->preedit->len>0){
            gchar *word=expand_word(e->preedit->str);
            update_context(e, word);
            IBusText *t=ibus_text_new_from_string(word);
            g_free(word);
            commit_and_remember(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        return FALSE;
    }
    g_string_truncate(e->typed_text,0);
    // Other keys: commit preedit and forward
    if(e->preedit->len>0){
        gchar *word=expand_word(e->preedit->str);
        update_context(e, word);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        commit_and_remember(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
        hide_suggest(e, engine);
    }
    return FALSE;
}
static void ibus_gotiengviet_engine_enable(IBusEngine *engine){
    ibus_engine_get_surrounding_text(engine,NULL,NULL,NULL);
}
static void ibus_gotiengviet_engine_focus_in(IBusEngine *engine){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    e->assistant_key_down=FALSE;
    if(!e->focus_id){
        e->focus_id=g_strdup_printf("/input/%p",(void*)engine);
    }
    ibus_engine_get_surrounding_text(engine,NULL,NULL,NULL);
    debug_log("[focus_in] engine=%p preedit='%s'\n", engine, e->preedit ? e->preedit->str : "");
    reload_engine_config(e);
    e->purpose=IBUS_INPUT_PURPOSE_FREE_FORM;
    // Đảm bảo focus vào input mới thì xóa sạch buffer preedit cũ
    if(e->preedit && e->preedit->len>0){
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
    }
    // Một engine duy nhất "gotiengviet": chuyển Telex/VNI trên indicator của app GoTiengViet
    // Đồng bộ cache mtime để reload_config_if_changed không load lại ngay
    gchar *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    struct stat st;
    if(stat(path, &st) == 0) cfg_mtime_cache = st.st_mtime;
    g_free(path);
    hide_suggest(e, engine);
}
static void ibus_gotiengviet_engine_focus_out(IBusEngine *engine){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    debug_log("[focus_out] engine=%p preedit='%s'\n", engine, e->preedit ? e->preedit->str : "");
    if(e->replacing)return;
    if(e->sentence_context) g_string_assign(e->sentence_context, "");
    if(e->preedit && e->preedit->len>0){
        gchar *word=expand_word(e->preedit->str);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        commit_and_remember(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
    }
    hide_suggest(e, engine);
}
static void focus_in_id(IBusEngine *engine,const gchar *id,const gchar *client){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    debug_log("[focus_in_id] engine=%p id='%s' client='%s'\n", engine, id ? id : "", client ? client : "");
    if(g_strcmp0(e->focus_id,id)!=0){
        if(e->assistant_inline && e->target_id && g_strcmp0(e->target_id,id)!=0){
            e->assistant_cancelled=TRUE;
            g_clear_pointer(&e->replacement,g_free);
            ibus_engine_hide_auxiliary_text(engine);
        }
        g_free(e->focus_id);
        e->focus_id=g_strdup(id);
        g_string_truncate(e->typed_text,0);
    }
    ibus_gotiengviet_engine_focus_in(engine);
}
static void focus_out_id(IBusEngine *engine,const gchar *id){
    debug_log("[focus_out_id] engine=%p id='%s'\n", engine, id ? id : "");
    ibus_gotiengviet_engine_focus_out(engine);
}
static void ibus_gotiengviet_engine_reset_cb(IBusEngine *engine){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    debug_log("[reset_cb] engine=%p preedit='%s'\n", engine, e->preedit ? e->preedit->str : "");
    if(e->sentence_context) g_string_assign(e->sentence_context, "");
    if(e->preedit && e->preedit->len>0){
        gchar *word=expand_word(e->preedit->str);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        commit_and_remember(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
        ibus_engine_hide_preedit_text(engine);
    }
    hide_suggest(e, engine);
}
static void ibus_gotiengviet_engine_disable(IBusEngine *engine){
    ibus_gotiengviet_engine_focus_out(engine);
}
static void ibus_gotiengviet_engine_set_capabilities(IBusEngine *engine, guint caps){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    debug_log("[set_caps] engine=%p caps=0x%x\n", engine, caps);
    e->caps = caps;
}
static void ibus_gotiengviet_engine_set_content_type(IBusEngine *engine, guint purpose, guint hints){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    (void)hints;
    debug_log("[set_content_type] engine=%p purpose=%u hints=%u\n", engine, purpose, hints);
    e->purpose = purpose;
}
static void ibus_gotiengviet_engine_set_cursor_location(IBusEngine *engine, gint x, gint y, gint w, gint h){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    gboolean moved=e->cursor_known && (e->last_cursor.x!=x || e->last_cursor.y!=y);
    if(moved && !e->cursor_expected && !e->replacing){
        g_string_truncate(e->typed_text,0);
        if(e->target_backspaces && e->assistant_inline){
            e->assistant_cancelled=TRUE;g_clear_pointer(&e->replacement,g_free);
            ibus_engine_hide_auxiliary_text(engine);
        }
    }
    e->last_cursor=(IBusRectangle){x,y,w,h};e->cursor_known=TRUE;e->cursor_expected=FALSE;
}
static void ibus_gotiengviet_engine_candidate_clicked(IBusEngine *engine, guint index, guint button, guint state){
    (void)button; (void)state;
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    if(index < (guint)e->n_candidates && e->candidates){
        learn_candidate(e,e->candidates[index]);
        g_string_assign(e->preedit, e->candidates[index]);
        gchar *word=expand_word(e->preedit->str);
        update_context(e, word);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        commit_and_remember(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
        hide_suggest(e, engine);
    }
}
static void ibus_gotiengviet_engine_finalize(GObject *object){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)object;
    clear_candidates(e);
    gtv_config_clear(&e->config);
    g_clear_pointer(&e->learned_words,g_hash_table_unref);g_free(e->learned_path);
    g_clear_pointer(&e->learned_fixes,g_hash_table_unref);g_free(e->learned_fixes_path);
    g_clear_pointer(&e->bs_expected,g_free);
    g_free(e->focus_id);g_free(e->target_id);g_free(e->target_text);g_free(e->replacement);g_free(e->insertion);gtv_text_target_free(e->verified_target);
    g_string_free(e->typed_text,TRUE);
    if(e->preedit) g_string_free(e->preedit,TRUE);
    if(e->sentence_context) g_string_free(e->sentence_context,TRUE);
    G_OBJECT_CLASS(ibus_gotiengviet_engine_parent_class)->finalize(object);
}
static void ibus_gotiengviet_engine_class_init(IBusGoTiengVietEngineClass *klass){
    G_OBJECT_CLASS(klass)->finalize=ibus_gotiengviet_engine_finalize;
    IBusEngineClass *ec=IBUS_ENGINE_CLASS(klass);
    ec->process_key_event=ibus_gotiengviet_engine_process_key_event;
    ec->focus_in_id=focus_in_id;
    ec->focus_out_id=focus_out_id;
    ec->focus_in=ibus_gotiengviet_engine_focus_in;
    ec->focus_out=ibus_gotiengviet_engine_focus_out;
    ec->reset=ibus_gotiengviet_engine_reset_cb;
    ec->disable=ibus_gotiengviet_engine_disable;
    ec->enable=ibus_gotiengviet_engine_enable;
    ec->set_capabilities=ibus_gotiengviet_engine_set_capabilities;
    ec->set_content_type=ibus_gotiengviet_engine_set_content_type;
    ec->set_cursor_location=ibus_gotiengviet_engine_set_cursor_location;
    ec->candidate_clicked=ibus_gotiengviet_engine_candidate_clicked;
}
static void ibus_gotiengviet_engine_init(IBusGoTiengVietEngine *e){
    e->preedit=g_string_new("");
    e->typed_text=g_string_new("");
    e->sentence_context=g_string_new("");
    e->modern=TRUE;
    e->caps=IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS;
    e->purpose=IBUS_INPUT_PURPOSE_FREE_FORM;
    reload_engine_config(e);
}
static IBusBus *bus=NULL;
static IBusFactory *factory=NULL;
static gint engine_id = 0;
static IBusEngine* create_engine_cb(IBusFactory *f, const gchar *engine_name, gpointer user_data){
    gchar *path = g_strdup_printf("/org/freedesktop/IBus/Engine/%d", ++engine_id);
    GDBusConnection *conn = ibus_bus_get_connection(bus);
    IBusEngine *engine = g_object_new(ibus_gotiengviet_engine_get_type(), "engine-name",engine_name,"object-path",path,"connection",conn,"has-focus-id",TRUE,NULL);
    g_free(path);
    if(!engine) return NULL;
    g_object_ref_sink(engine);
    IBusGoTiengVietEngine *ue = (IBusGoTiengVietEngine*)engine;
    // Một engine duy nhất "gotiengviet"; giữ tương thích tên cũ khi user còn sót config
    reload_engine_config(ue);
    if(g_strcmp0(engine_name, "gotiengviet-vni") == 0) ue->mode_telex = FALSE;
    else if(g_strcmp0(engine_name, "gotiengviet-telex") == 0) ue->mode_telex = TRUE;
    return engine;
}
/* Crash handling - chỉ dùng glib hệ thống */
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
static void crash_handler(int sig){
    void *bt[32]; int n = backtrace(bt, 32);
    gchar *dir = g_build_filename(g_get_user_cache_dir(), "gotiengviet", NULL);
    g_mkdir_with_parents(dir, 0755);
    gchar *log = g_build_filename(dir, "crash.log", NULL);
    FILE *f = fopen(log, "a");
    if(f){
        time_t now=time(NULL);
        fprintf(f, "\n=== CRASH %s signal %d (%s) ===\n", ctime(&now), sig, strsignal(sig));
        backtrace_symbols_fd(bt, n, fileno(f));
        fclose(f);
    }
    g_free(dir); g_free(log);
    // Ghi ra stderr để apport bắt
    fprintf(stderr, "GoTiengViet crash signal %d, log saved\n", sig);
    backtrace_symbols_fd(bt, n, STDERR_FILENO);
    // Thử reset preedit thay vì abort cứng
    _exit(1); // để ibus-daemon tự restart engine (watchdog)
}
static void install_crash_handlers(void){
    struct sigaction sa; memset(&sa,0,sizeof(sa));
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    // Bỏ qua SIGPIPE
    signal(SIGPIPE, SIG_IGN);
}

static void bus_connected_cb(IBusBus *b, gpointer user_data){
    IBusComponent *c = NULL;
    if(g_file_test("/usr/share/ibus/component/gotiengviet.xml", G_FILE_TEST_EXISTS)){
        c = ibus_component_new_from_file("/usr/share/ibus/component/gotiengviet.xml");
    }
    if(!c){
        c = ibus_component_new("org.freedesktop.IBus.GoTiengViet","GoTiengViet Engine (thuần hệ thống)","0.3.0","GPL","GoTiengViet Project","https://github.com/isthaison/gotiengviet","/usr/libexec/ibus-engine-gotiengviet --ibus","gotiengviet");
        IBusEngineDesc *d = ibus_engine_desc_new_varargs(
            "name", "gotiengviet",
            "longname", "GoTiengViet",
            "description", "GoTiengViet: Telex/VNI - github.com/isthaison/gotiengviet",
            "language", "vi",
            "license", "GPL",
            "author", "GoTiengViet",
            "icon", "gotiengviet",
            "layout", "us",
            "setup", "/usr/libexec/ibus-setup-gotiengviet",
            NULL);
        ibus_component_add_engine(c, d);
    }
    ibus_bus_register_component(bus, c);
    if(!factory){
        factory=ibus_factory_new(ibus_bus_get_connection(bus));
        g_signal_connect(factory, "create-engine", G_CALLBACK(create_engine_cb), NULL);
        ibus_factory_add_engine(factory, "gotiengviet", ibus_gotiengviet_engine_get_type());
        // Giữ tên cũ để máy đã cài không mất engine khi chưa re-login
        ibus_factory_add_engine(factory, "gotiengviet-telex", ibus_gotiengviet_engine_get_type());
        ibus_factory_add_engine(factory, "gotiengviet-vni", ibus_gotiengviet_engine_get_type());
    }
    ibus_bus_request_name(bus,"org.freedesktop.IBus.GoTiengViet",0);
}
int main(int argc, char **argv){
    install_crash_handlers();
    gtv_init();
    if(argc>1 && strcmp(argv[1],"--ibus")==0){
        ibus_init();
        bus=ibus_bus_new();
        g_signal_connect(bus,"connected",G_CALLBACK(bus_connected_cb),NULL);
        // Nếu bus đã connected sẵn (trường hợp restart nhanh), tạo factory ngay
        if(ibus_bus_is_connected(bus)){
            bus_connected_cb(bus, NULL);
        }
        g_main_loop_run(g_main_loop_new(NULL,FALSE));
    } else {
        printf("GoTiengViet IBus engine - Telex/VNI thuần hệ thống\n");
        printf("Test: as->á, dd->đ, hoaf->hoà\n");
        // quick test using telex_transform
        GString *s=g_string_new("chao");
        GArray *buf=gstring_to_ucs4(s);
        telex_transform(buf,'f',TRUE);
        GString *out=g_string_new("");
        ucs4_to_gstring(buf,out);
        printf("chao+f => %s (expect chào)\n",out->str);
        g_string_free(s,TRUE); g_string_free(out,TRUE); g_array_free(buf,TRUE);
    }
    return 0;
}
