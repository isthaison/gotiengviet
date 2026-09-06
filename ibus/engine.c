/* IBus adapter for the shared C input engine. */
#include <ibus.h>
#include <sys/stat.h>
#include "../engine/internal.h"
#define ENGINE_NAME "gotiengviet"

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
    guint purpose;
    guint hints;
    gchar **candidates;
    int n_candidates;
    int cand_cursor;
};
struct _GoTiengVietEngineClass { IBusEngineClass parent; };
G_DEFINE_TYPE(IBusGoTiengVietEngine, ibus_gotiengviet_engine, IBUS_TYPE_ENGINE)

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
/* Đẩy preedit + gạch đỏ từ sai + bảng gợi ý (Tab chọn, Up/Down di chuyển, Esc bỏ) */
static void push_preedit(IBusGoTiengVietEngine *e, IBusEngine *engine, guint cursor, gboolean visible){
    glong plen=g_utf8_strlen(e->preedit->str, -1);
    debug_log("[push_preedit] str='%s' plen=%ld visible=%d\n", e->preedit->str, plen, visible);
    gboolean is_emoji=(e->preedit->str[0]==':' || e->preedit->str[0]==';' || e->preedit->str[0]=='<');
    gboolean bad=(!is_emoji && e->spellcheck && plen>=2 && !spell_word_valid(e->preedit->str));
    IBusText *t=ibus_text_new_from_string(e->preedit->str);
    if(bad) ibus_text_append_attribute(t, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_ERROR, 0, (gint)plen);
    else ibus_text_append_attribute(t, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, (gint)plen);
    ibus_engine_update_preedit_text_with_mode(engine, t, (guint)plen, visible, IBUS_ENGINE_PREEDIT_COMMIT);
    if(plen > 0 && visible){
        ibus_engine_show_preedit_text(engine);
    } else {
        ibus_engine_hide_preedit_text(engine);
    }
    clear_candidates(e);
    GPtrArray *sugs = NULL;
    if(is_emoji && plen >= 2){
        sugs = get_emoji_suggestions(e->preedit->str);
    } else {
        if(e->spellcheck && e->sentence_context && e->sentence_context->len > 0 && plen >= 1)
            sugs = gtv_vector_predict_next(e->sentence_context->str,e->preedit->str,5);
        if(bad){
            GPtrArray *corrections=get_suggestions(e->preedit->str);
            if(!sugs) sugs=g_ptr_array_new_with_free_func(g_free);
            for(guint i=0;i<corrections->len && sugs->len<5;i++)
                add_candidate_unique(sugs,g_ptr_array_index(corrections,i));
            g_ptr_array_unref(corrections);
        }
    }
    if(!sugs || sugs->len == 0){
        if(sugs) g_ptr_array_free(sugs, TRUE);
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
    g_ptr_array_free(sugs,TRUE);
    ibus_engine_update_lookup_table(engine, table, TRUE);
    g_object_unref(table);
}
static void ibus_gotiengviet_engine_reset(IBusGoTiengVietEngine *e){
    if(e->preedit) g_string_assign(e->preedit,"");
    ibus_engine_hide_preedit_text((IBusEngine*)e);
}
// Tray đổi method khi đang gõ không gây focus_in, nên reload config theo mtime mỗi phím
static gboolean load_config(gboolean *is_telex, gboolean *modern, gboolean *spell);
static time_t cfg_mtime_cache = 0;
static void reload_config_if_changed(IBusGoTiengVietEngine *e){
    gchar *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    struct stat st;
    if(stat(path, &st) == 0 && st.st_mtime != cfg_mtime_cache){
        cfg_mtime_cache = st.st_mtime;
        load_config(&e->mode_telex,&e->modern,&e->spellcheck);
    }
    g_free(path);
}
static gboolean ibus_gotiengviet_engine_process_key_event(IBusEngine *engine, guint keyval, guint keycode, guint modifiers){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    reload_config_if_changed(e);
    debug_log("[key] kv=0x%x ('%c') kc=%u mod=0x%x preedit='%s' purpose=%u caps=0x%x\n",
              keyval, (keyval>32 && keyval<127)?(char)keyval:' ', keycode, modifiers, e->preedit->str, e->purpose, e->caps);
    if(modifiers & IBUS_RELEASE_MASK) return FALSE;
    /* Let the desktop handle Shift/locks without committing a partial word.
     * Character case comes from keyval, already resolved by the keyboard layout. */
    switch(keyval){
        case IBUS_Shift_L: case IBUS_Shift_R:
        case IBUS_Caps_Lock: case IBUS_Shift_Lock:
        case IBUS_Num_Lock: case IBUS_Scroll_Lock:
            return FALSE;
        default:
            break;
    }
    // Phím tắt Ctrl/Alt/Super (Ctrl+C/V/X/Z, Ctrl+S...) — commit chữ đang dở rồi nhường cho app
    if(modifiers & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK | IBUS_HYPER_MASK | IBUS_META_MASK)){
        if(e->preedit->len>0){
            IBusText *t=ibus_text_new_from_string(e->preedit->str);
            ibus_engine_commit_text(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
        }
        return FALSE;
    }
    // Trường mật khẩu (Password / PIN): Không can thiệp preedit, nhường phím trực tiếp
    if(e->purpose == IBUS_INPUT_PURPOSE_PASSWORD || e->purpose == IBUS_INPUT_PURPOSE_PIN){
        return FALSE;
    }
    // Esc: hủy preedit nếu đang gõ dở
    if(keyval == IBUS_Escape){
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
            g_string_assign(e->preedit, e->candidates[e->cand_cursor]);
            push_preedit(e, engine, e->preedit->len, TRUE);
            return TRUE;
        }
        if(e->mode_telex && ((keyval>=IBUS_1 && keyval<=IBUS_5) || (keyval>=IBUS_KP_1 && keyval<=IBUS_KP_5))){
            int idx = (keyval>=IBUS_1 && keyval<=IBUS_5) ? (keyval - IBUS_1) : (keyval - IBUS_KP_1);
            if(idx < e->n_candidates){
                g_string_assign(e->preedit, e->candidates[idx]);
                gchar *word=expand_word(e->preedit->str);
                update_context(e, word);
                IBusText *t=ibus_text_new_from_string(word);
                g_free(word);
                ibus_engine_commit_text(engine,t);
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
            g_string_assign(e->preedit, e->candidates[e->cand_cursor]);
            gchar *word=expand_word(e->preedit->str);
            update_context(e, word);
            IBusText *t=ibus_text_new_from_string(word);
            g_free(word);
            ibus_engine_commit_text(engine,t);
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
        if(e->preedit->len>0){
            // remove last utf8 char
            gchar *prev = g_utf8_prev_char(e->preedit->str + e->preedit->len);
            g_string_truncate(e->preedit, prev - e->preedit->str);
            push_preedit(e, engine, e->preedit->len, TRUE);
            return TRUE;
        }
        return FALSE;
    }
    if(keyval==IBUS_space){
        if(e->preedit->len>0){
            gchar *commit = expand_word(e->preedit->str);
            update_context(e, commit);
            gchar *with_space = g_strdup_printf("%s ",commit);
            IBusText *t=ibus_text_new_from_string(with_space);
            ibus_engine_commit_text(engine,t);
            g_free(commit); g_free(with_space);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        return FALSE;
    }
    // All printable keys use the same C composer as the CLI and tests.
    gunichar key = ibus_keyval_to_unicode(keyval);
    if(key >= 0x20 && g_unichar_isprint(key)){
        GtvConfig config = {.mode = e->mode_telex ? GTV_TELEX : GTV_VNI,
                            .modern = e->modern, .spellcheck = FALSE};
        GtvEngine *composer = gtv_engine_new(&config);
        g_array_unref(composer->buffer);
        composer->buffer = gstring_to_ucs4(e->preedit);
        gchar *commit = gtv_engine_process(composer, key, NULL);
        if(commit){
            update_context(e, commit);
            ibus_engine_commit_text(engine, ibus_text_new_from_string(commit));
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
            ibus_engine_commit_text(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        return FALSE;
    }
    // Other keys: commit preedit and forward
    if(e->preedit->len>0){
        gchar *word=expand_word(e->preedit->str);
        update_context(e, word);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        ibus_engine_commit_text(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
        hide_suggest(e, engine);
    }
    return FALSE;
}
static gboolean load_config(gboolean *is_telex, gboolean *modern, gboolean *spell){
    gchar *directory=g_build_filename(g_get_user_config_dir(),"gotiengviet",NULL);
    GtvConfig config;
    gtv_config_load(&config,directory);
    if(is_telex) *is_telex=config.mode == GTV_TELEX;
    if(modern) *modern=config.modern;
    if(spell) *spell=config.spellcheck;
    gtv_config_clear(&config);g_free(directory);
    return TRUE;
}
static void ibus_gotiengviet_engine_focus_in(IBusEngine *engine){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    debug_log("[focus_in] engine=%p preedit='%s'\n", engine, e->preedit ? e->preedit->str : "");
    gboolean telex, modern, spell;
    load_config(&telex, &modern, &spell);
    e->mode_telex=telex;
    e->modern=modern;
    e->spellcheck=spell;
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
    if(e->sentence_context) g_string_assign(e->sentence_context, "");
    if(e->preedit && e->preedit->len>0){
        gchar *word=expand_word(e->preedit->str);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        ibus_engine_commit_text(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
    }
    hide_suggest(e, engine);
}
static void ibus_gotiengviet_engine_reset_cb(IBusEngine *engine){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    debug_log("[reset_cb] engine=%p preedit='%s'\n", engine, e->preedit ? e->preedit->str : "");
    if(e->sentence_context) g_string_assign(e->sentence_context, "");
    if(e->preedit && e->preedit->len>0){
        gchar *word=expand_word(e->preedit->str);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        ibus_engine_commit_text(engine,t);
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
    debug_log("[set_content_type] engine=%p purpose=%u hints=%u\n", engine, purpose, hints);
    e->purpose = purpose;
    e->hints = hints;
}
static void ibus_gotiengviet_engine_set_cursor_location(IBusEngine *engine, gint x, gint y, gint w, gint h){
    (void)engine; (void)x; (void)y; (void)w; (void)h;
}
static void ibus_gotiengviet_engine_candidate_clicked(IBusEngine *engine, guint index, guint button, guint state){
    (void)button; (void)state;
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    if(index < (guint)e->n_candidates && e->candidates){
        g_string_assign(e->preedit, e->candidates[index]);
        gchar *word=expand_word(e->preedit->str);
        update_context(e, word);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        ibus_engine_commit_text(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
        hide_suggest(e, engine);
    }
}
static void ibus_gotiengviet_engine_finalize(GObject *object){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)object;
    clear_candidates(e);
    if(e->preedit) g_string_free(e->preedit,TRUE);
    if(e->sentence_context) g_string_free(e->sentence_context,TRUE);
    G_OBJECT_CLASS(ibus_gotiengviet_engine_parent_class)->finalize(object);
}
static void ibus_gotiengviet_engine_class_init(IBusGoTiengVietEngineClass *klass){
    G_OBJECT_CLASS(klass)->finalize=ibus_gotiengviet_engine_finalize;
    IBusEngineClass *ec=IBUS_ENGINE_CLASS(klass);
    ec->process_key_event=ibus_gotiengviet_engine_process_key_event;
    ec->focus_in=ibus_gotiengviet_engine_focus_in;
    ec->focus_out=ibus_gotiengviet_engine_focus_out;
    ec->reset=ibus_gotiengviet_engine_reset_cb;
    ec->disable=ibus_gotiengviet_engine_disable;
    ec->set_capabilities=ibus_gotiengviet_engine_set_capabilities;
    ec->set_content_type=ibus_gotiengviet_engine_set_content_type;
    ec->set_cursor_location=ibus_gotiengviet_engine_set_cursor_location;
    ec->candidate_clicked=ibus_gotiengviet_engine_candidate_clicked;
}
static void ibus_gotiengviet_engine_init(IBusGoTiengVietEngine *e){
    e->preedit=g_string_new("");
    e->sentence_context=g_string_new("");
    e->modern=TRUE;
    e->caps=IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS;
    e->purpose=IBUS_INPUT_PURPOSE_FREE_FORM;
    e->hints=IBUS_INPUT_HINT_NONE;
    load_config(&e->mode_telex, &e->modern, &e->spellcheck);
}
static IBusBus *bus=NULL;
static IBusFactory *factory=NULL;
static gint engine_id = 0;
static IBusEngine* create_engine_cb(IBusFactory *f, const gchar *engine_name, gpointer user_data){
    gchar *path = g_strdup_printf("/org/freedesktop/IBus/Engine/%d", ++engine_id);
    GDBusConnection *conn = ibus_bus_get_connection(bus);
    IBusEngine *engine = ibus_engine_new_with_type(ibus_gotiengviet_engine_get_type(), engine_name, path, conn);
    g_free(path);
    if(!engine) return NULL;
    g_object_ref_sink(engine);
    IBusGoTiengVietEngine *ue = (IBusGoTiengVietEngine*)engine;
    // Một engine duy nhất "gotiengviet"; giữ tương thích tên cũ khi user còn sót config
    gboolean telex = TRUE, mod = TRUE, spell = TRUE;
    load_config(&telex, &mod, &spell);
    if(g_strcmp0(engine_name, "gotiengviet-vni") == 0) telex = FALSE;
    else if(g_strcmp0(engine_name, "gotiengviet-telex") == 0) telex = TRUE;
    ue->mode_telex = telex;
    ue->modern=mod;
    ue->spellcheck=spell;
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
