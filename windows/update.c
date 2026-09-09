#include "update.h"
#include "hook.h"
#include "tray.h"
#include "version.h"
#include "internal.h"
#include <glib/gstdio.h>
#include <shellapi.h>

typedef struct { HWND notify; gboolean manual; } CheckJob;

static volatile LONG update_in_flight = 0;
static volatile DWORD update_flight_tick = 0;

/* Append-only diagnostic log next to the config, so a silent updater can
 * be diagnosed from %APPDATA%/gotiengviet/update.log. Bounded size. */
static void update_log(const gchar *fmt, ...) {
    gchar *dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    g_mkdir_with_parents(dir, 0755);
    gchar *path = g_build_filename(dir, "update.log", NULL);
    g_free(dir);
    FILE *probe = g_fopen(path, "r");
    if (probe) {
        fseek(probe, 0, SEEK_END);
        if (ftell(probe) > 65536) {
            fclose(probe);
            g_remove(path);
            probe = NULL;
        } else fclose(probe);
    }
    FILE *f = g_fopen(path, "a");
    g_free(path);
    if (!f) return;
    GDateTime *now = g_date_time_new_now_local();
    gchar *ts = now ? g_date_time_format(now, "%Y-%m-%d %H:%M:%S") : g_strdup("?");
    fprintf(f, "[%s] ", ts);
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
    g_free(ts);
    if (now) g_date_time_unref(now);
}

/* Latest balloon ownership: the AI typo balloon and the update balloon
 * share one tray slot, so clicks are routed to whoever showed last. */
static volatile LONG update_balloon_owned = 0;

/* Pending update behind the balloon: click downloads and installs it. */
static gchar *pending_tag = NULL;
static gchar *pending_url = NULL;

static gpointer check_worker(gpointer data) {
    CheckJob *job = data;
    gchar *tag = NULL, *url = NULL;
    GtvUpdateStatus st = gtv_update_check(NULL, GTV_VERSION, &tag, &url);
    GtvUpdateResult *res = g_new(GtvUpdateResult, 1);
    res->status = (gint)st;
    res->tag = tag;
    res->url = url;
    if (job->notify)
        PostMessage(job->notify, WM_GTV_UPDATE_RESULT, (WPARAM)job->manual, (LPARAM)res);
    else {
        g_free(tag); g_free(url); g_free(res);
    }
    g_free(job);
    InterlockedExchange(&update_in_flight, 0);
    return NULL;
}

void gtv_update_check_async(HWND notify, gboolean manual) {
    if (manual) {
        /* Manual checks always run; auto-checks honour the 24h throttle. */
    } else if (!gtv_update_should_autocheck()) {
        return;
    } else {
        gtv_update_mark_checked();
    }
    if (InterlockedCompareExchange(&update_in_flight, 1, 0) != 0) return;
    CheckJob *job = g_new0(CheckJob, 1);
    job->notify = notify;
    job->manual = manual;
    GThread *th = g_thread_new("gtv-update-check", check_worker, job);
    if (!th) {
        g_free(job);
        InterlockedExchange(&update_in_flight, 0);
        return;
    }
    g_thread_unref(th);
}

void gtv_update_on_result(GtvUpdateResult *res, gboolean manual) {
    if (!res) return;
    if (res->status == GTV_UPDATE_AVAILABLE && res->tag && res->url) {
        g_free(pending_tag); g_free(pending_url);
        pending_tag = g_strdup(res->tag);
        pending_url = g_strdup(res->url);
        InterlockedExchange(&update_balloon_owned, 1);
        gchar *msg = g_strdup_printf("Co ban moi %s. Nhan vao day de tai va cai dat.", res->tag);
        gtv_tray_balloon("GoTiengViet cap nhat", msg);
        g_free(msg);
    } else if (manual) {
        InterlockedExchange(&update_balloon_owned, 0);
        if (res->status == GTV_UPDATE_CURRENT)
            gtv_tray_balloon("GoTiengViet cap nhat", "Ban dang dung ban moi nhat.");
        else
            gtv_tray_balloon("GoTiengViet cap nhat", "Khong kiem tra duoc ban moi. Thu lai sau.");
    }
    g_free(res->tag); g_free(res->url); g_free(res);
}

static gpointer download_worker(gpointer data) {
    gchar *url = data;
    gchar *name = g_path_get_basename(url);
    gchar tdir[MAX_PATH];
    DWORD n = GetTempPathA(sizeof(tdir), tdir);
    gchar *dest = g_strdup_printf("%s%s", n > 0 ? tdir : "C:\\Windows\\Temp\\", name);
    g_free(name);
    gboolean ok = gtv_update_download(url, dest);
    g_free(url);
    if (ok && g_app.hwnd_main)
        PostMessage(g_app.hwnd_main, WM_GTV_UPDATE_DOWNLOADED, 0, (LPARAM)dest);
    else
        g_free(dest);
    return NULL;
}

gboolean gtv_update_balloon_clicked(void) {
    if (InterlockedCompareExchange(&update_balloon_owned, 0, 0) != 1) return FALSE;
    if (!pending_tag || !pending_url) {
        InterlockedExchange(&update_balloon_owned, 0);
        return FALSE;
    }
    /* The balloon slot now belongs to the download progress message. */
    gchar *msg = g_strdup_printf("Dang tai ban %s...", pending_tag);
    gtv_tray_balloon("GoTiengViet cap nhat", msg);
    g_free(msg);
    gchar *url = pending_url;
    pending_url = NULL;
    g_free(pending_tag);
    pending_tag = NULL;
    GThread *th = g_thread_new("gtv-update-dl", download_worker, url);
    if (!th) {
        g_free(url);
        InterlockedExchange(&update_balloon_owned, 0);
        return TRUE;
    }
    g_thread_unref(th);
    return TRUE;
}

void gtv_update_disown_balloon(void) {
    InterlockedExchange(&update_balloon_owned, 0);
}

void gtv_update_on_downloaded(gchar *installer_path) {
    InterlockedExchange(&update_balloon_owned, 0);
    if (!installer_path || !*installer_path) {
        gtv_tray_balloon("GoTiengViet cap nhat", "Tai ban moi that bai. Thu lai sau.");
        g_free(installer_path);
        return;
    }
    /* Run the versioned Inno installer silently, then quit so locked
     * files (exe/dlls) can be replaced. /CLOSEAPPLICATIONS is a safety
     * net in case a second copy is still running. */
    ShellExecuteA(NULL, "open", installer_path, "/SILENT /CLOSEAPPLICATIONS", NULL, SW_SHOWNORMAL);
    g_free(installer_path);
    PostQuitMessage(0);
}
