#include "update.h"
#include "app.h"
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
void gtv_update_log(const gchar *fmt, ...) {
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
    gtv_update_log("check done manual=%d status=%d tag=%s url=%s",
                   job->manual, (gint)st, tag ? tag : "-", url ? url : "-");
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
    if (InterlockedCompareExchange(&update_in_flight, 1, 0) != 0) {
        /* A previous worker should always reset the slot; reclaim it if
         * it has been stuck longer than any check may take, so one wedged
         * run can never silence the updater until restart. */
        if ((DWORD)(GetTickCount() - update_flight_tick) < 120000) return;
        gtv_update_log("stale check slot reclaimed");
        InterlockedExchange(&update_in_flight, 0);
        if (InterlockedCompareExchange(&update_in_flight, 1, 0) != 0) return;
    }
    update_flight_tick = GetTickCount();
    gtv_update_log("check start manual=%d version=%s", manual, GTV_VERSION);
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
        gtv_update_log("balloon shown: update available %s", res->tag);
        gtv_tray_balloon_force("GoTiengViet cap nhat", msg);
        g_free(msg);
    } else if (manual) {
        InterlockedExchange(&update_balloon_owned, 0);
        if (res->status == GTV_UPDATE_CURRENT) {
            gchar *msg = g_strdup_printf("Ban dang dung ban moi nhat (%s).", GTV_VERSION);
            gtv_update_log("balloon shown: up to date %s", GTV_VERSION);
            gtv_tray_balloon_force("GoTiengViet cap nhat", msg);
            g_free(msg);
        } else {
            gtv_update_log("balloon shown: check error");
            gtv_tray_balloon_force("GoTiengViet cap nhat", "Khong kiem tra duoc ban moi. Thu lai sau.");
        }
    }
    g_free(res->tag); g_free(res->url); g_free(res);
}

static gpointer download_worker(gpointer data) {
    gchar *url = data;
    gchar *name = g_path_get_basename(url);
    /* Temp dir and username may contain non-ASCII: stay in UTF-16 until
     * the last step, convert only for curl's argv (UTF-8). */
    WCHAR tdir[MAX_PATH];
    gchar *dest = NULL;
    if (GetTempPathW(MAX_PATH, tdir)) {
        gunichar2 *wname = g_utf8_to_utf16(name ? name : "setup.exe", -1, NULL, NULL, NULL);
        WCHAR *wfull = g_new(WCHAR, MAX_PATH + 256);
        swprintf(wfull, MAX_PATH + 256, L"%ls%ls", tdir, (WCHAR*)wname);
        g_free(wname);
        dest = g_utf16_to_utf8(wfull, -1, NULL, NULL, NULL);
        g_free(wfull);
    }
    g_free(name);
    gtv_update_log("downloading %s", url);
    gboolean ok = dest && gtv_update_download(url, dest);
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
    gtv_update_log("download start tag=%s", pending_tag);
    gtv_tray_balloon_force("GoTiengViet cap nhat", msg);
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
        gtv_update_log("download failed");
        gtv_tray_balloon_force("GoTiengViet cap nhat", "Tai ban moi that bai. Thu lai sau.");
        g_free(installer_path);
        return;
    }
    /* Run the versioned Inno installer silently, then quit so locked
     * files (exe/dlls) can be replaced. /CLOSEAPPLICATIONS is a safety
     * net in case a second copy is still running. Unicode path: %TEMP%
     * and usernames are often non-ASCII. */
    gunichar2 *winstaller = g_utf8_to_utf16(installer_path, -1, NULL, NULL, NULL);
    gtv_update_log("installer launched %s", installer_path);
    ShellExecuteW(NULL, L"open", (LPCWSTR)winstaller, L"/SILENT /CLOSEAPPLICATIONS", NULL, SW_SHOWNORMAL);
    g_free(winstaller);
    g_free(installer_path);
    PostQuitMessage(0);
}
