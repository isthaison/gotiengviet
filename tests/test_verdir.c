/* Tests for engine/verdir.c (versioned install layout helpers). Portable. */
#include "../engine/internal.h"
#include <glib/gstdio.h>

static void test_app_dir(void) {
    gchar *d;
    /* Versioned payload module -> app root two levels up. */
    d = gtv_app_dir_for_module("C:\\Users\\x\\AppData\\Local\\Programs\\GoTiengViet\\ver\\0.8.12\\gotiengviet.exe");
    g_assert_cmpstr(d, ==, "C:\\Users\\x\\AppData\\Local\\Programs\\GoTiengViet");
    g_free(d);
    d = gtv_app_dir_for_module("/opt/gtv/ver/0.8.12/gtv_engine.dll");
    g_assert_cmpstr(d, ==, "/opt/gtv");
    g_free(d);
    /* Root-level module -> own dir. */
    d = gtv_app_dir_for_module("C:\\Users\\x\\AppData\\Local\\Programs\\GoTiengViet\\gotiengviet.exe");
    g_assert_cmpstr(d, ==, "C:\\Users\\x\\AppData\\Local\\Programs\\GoTiengViet");
    g_free(d);
    /* Dev layout -> own dir. */
    d = gtv_app_dir_for_module("E:/git/gotiengviet/build/win/gotiengviet.exe");
    g_assert_cmpstr(d, ==, "E:/git/gotiengviet/build/win");
    g_free(d);
    /* Degenerate input never returns NULL. */
    d = gtv_app_dir_for_module("");
    g_assert_nonnull(d);
    g_free(d);
    d = gtv_app_dir_for_module(NULL);
    g_assert_nonnull(d);
    g_free(d);
}

static void test_current_and_dir(void) {
    gchar *tmp = g_dir_make_tmp("gotiengviet-ver-XXXXXX", NULL);
    g_assert_nonnull(tmp);
    /* No current.txt -> NULL current, ver dir falls back to appdir. */
    g_assert_null(gtv_ver_current(tmp));
    gchar *vd = gtv_ver_dir(tmp, NULL);
    g_assert_cmpstr(vd, ==, tmp);
    g_free(vd);
    gchar *cur = g_build_filename(tmp, "current.txt", NULL);
    g_assert_true(g_file_set_contents(cur, "0.8.12\n", -1, NULL));
    gchar *ver = gtv_ver_current(tmp);
    g_assert_cmpstr(ver, ==, "0.8.12");
    vd = gtv_ver_dir(tmp, ver);
    gchar *want = g_build_filename(tmp, "ver", "0.8.12", NULL);
    g_assert_cmpstr(vd, ==, want);
    g_free(want);
    g_free(vd);
    g_free(ver);
    g_free(cur);
    g_remove(tmp);
    g_free(tmp);
}

static void test_ver_cmp(void) {
    g_assert_cmpint(gtv_ver_cmp("0.8.9", "0.8.11"), <, 0);
    g_assert_cmpint(gtv_ver_cmp("0.8.11", "0.8.9"), >, 0);
    g_assert_cmpint(gtv_ver_cmp("0.8.12", "0.8.12"), ==, 0);
    g_assert_cmpint(gtv_ver_cmp("1.0", "0.9.9"), >, 0);
    g_assert_cmpint(gtv_ver_cmp("0.8", "0.8.0"), ==, 0);
}

static void test_forward(void) {
    gchar *tmp = g_dir_make_tmp("gotiengviet-fwd-XXXXXX", NULL);
    g_assert_nonnull(tmp);
    /* No current.txt -> no forward (dev/legacy). */
    g_assert_null(gtv_forward_target(tmp, "0.8.11"));
    gchar *cur = g_build_filename(tmp, "current.txt", NULL);
    g_assert_true(g_file_set_contents(cur, "0.8.12\n", -1, NULL));
    g_free(cur);
    /* Same version -> no forward. */
    g_assert_null(gtv_forward_target(tmp, "0.8.12"));
    /* Older own version but payload exe missing -> no forward. */
    g_assert_null(gtv_forward_target(tmp, "0.8.11"));
    /* Payload exe present -> forward target. */
    gchar *vdir = g_build_filename(tmp, "ver", "0.8.12", NULL);
    g_assert_cmpint(g_mkdir_with_parents(vdir, 0755), ==, 0);
    gchar *exe = g_build_filename(vdir, "gotiengviet.exe", NULL);
    g_assert_true(g_file_set_contents(exe, "x", -1, NULL));
    gchar *tgt = gtv_forward_target(tmp, "0.8.11");
    g_assert_cmpstr(tgt, ==, exe);
    g_free(tgt);
    g_free(exe);
    g_free(vdir);
    /* Newer own version -> no forward (downgrade guard). */
    g_assert_null(gtv_forward_target(tmp, "0.9.0"));
    /* Tear down. */
    gchar *p;
    p = g_build_filename(tmp, "ver", "0.8.12", "gotiengviet.exe", NULL);
    g_remove(p);
    g_free(p);
    p = g_build_filename(tmp, "ver", "0.8.12", NULL);
    g_remove(p);
    g_free(p);
    p = g_build_filename(tmp, "ver", NULL);
    g_remove(p);
    g_free(p);
    p = g_build_filename(tmp, "current.txt", NULL);
    g_remove(p);
    g_free(p);
    g_remove(tmp);
    g_free(tmp);
}

static void test_cleanup(void) {
    gchar *tmp = g_dir_make_tmp("gotiengviet-clean-XXXXXX", NULL);
    g_assert_nonnull(tmp);
    const gchar *vers[] = {"0.8.9", "0.8.10", "0.8.11", "0.8.12", NULL};
    for (guint i = 0; vers[i]; i++) {
        gchar *d = g_build_filename(tmp, "ver", vers[i], NULL);
        g_assert_cmpint(g_mkdir_with_parents(d, 0755), ==, 0);
        gchar *f = g_build_filename(d, "x.txt", NULL);
        g_assert_true(g_file_set_contents(f, "x", -1, NULL));
        g_free(f);
        g_free(d);
    }
    gchar *cur = g_build_filename(tmp, "current.txt", NULL);
    g_assert_true(g_file_set_contents(cur, "0.8.12\n", -1, NULL));
    g_free(cur);
    /* Keeps current (0.8.12) + newest other (0.8.11), removes 2. */
    g_assert_cmpuint(gtv_ver_cleanup(tmp), ==, 2);
    gchar *keep;
    keep = g_build_filename(tmp, "ver", "0.8.12", NULL);
    g_assert_true(g_file_test(keep, G_FILE_TEST_IS_DIR));
    g_free(keep);
    keep = g_build_filename(tmp, "ver", "0.8.11", NULL);
    g_assert_true(g_file_test(keep, G_FILE_TEST_IS_DIR));
    g_free(keep);
    keep = g_build_filename(tmp, "ver", "0.8.9", NULL);
    g_assert_false(g_file_test(keep, G_FILE_TEST_EXISTS));
    g_free(keep);
    /* Second run removes nothing. */
    g_assert_cmpuint(gtv_ver_cleanup(tmp), ==, 0);
    /* No current.txt: keeps newest only. */
    gchar *curpath = g_build_filename(tmp, "current.txt", NULL);
    g_remove(curpath);
    g_free(curpath);
    g_assert_cmpuint(gtv_ver_cleanup(tmp), ==, 1);
    keep = g_build_filename(tmp, "ver", "0.8.12", NULL);
    g_assert_true(g_file_test(keep, G_FILE_TEST_IS_DIR));
    g_free(keep);
    /* Tear down. */
    g_assert_cmpuint(gtv_ver_cleanup(tmp), ==, 0);
    keep = g_build_filename(tmp, "ver", "0.8.12", "x.txt", NULL);
    g_remove(keep);
    g_free(keep);
    keep = g_build_filename(tmp, "ver", "0.8.12", NULL);
    g_remove(keep);
    g_free(keep);
    keep = g_build_filename(tmp, "ver", NULL);
    g_remove(keep);
    g_free(keep);
    g_remove(tmp);
    g_free(tmp);
}

static void test_legacy(void) {
    gchar *tmp = g_dir_make_tmp("gotiengviet-leg-XXXXXX", NULL);
    g_assert_nonnull(tmp);
    gchar *old = g_build_filename(tmp, "gotiengviet.exe", NULL);
    g_assert_true(g_file_set_contents(old, "x", -1, NULL));
    /* No versioned layout -> flat files untouched. */
    gtv_legacy_cleanup(tmp);
    g_assert_true(g_file_test(old, G_FILE_TEST_EXISTS));
    /* Migrated layout -> flat leftovers removed. */
    gchar *vdir = g_build_filename(tmp, "ver", "0.8.12", NULL);
    g_assert_cmpint(g_mkdir_with_parents(vdir, 0755), ==, 0);
    g_free(vdir);
    gchar *cur = g_build_filename(tmp, "current.txt", NULL);
    g_assert_true(g_file_set_contents(cur, "0.8.12\n", -1, NULL));
    g_free(cur);
    gchar *dd = g_build_filename(tmp, "data", NULL);
    g_assert_cmpint(g_mkdir_with_parents(dd, 0755), ==, 0);
    gchar *df = g_build_filename(dd, "macros.txt", NULL);
    g_assert_true(g_file_set_contents(df, "x", -1, NULL));
    g_free(df);
    g_free(dd);
    gtv_legacy_cleanup(tmp);
    g_assert_false(g_file_test(old, G_FILE_TEST_EXISTS));
    g_free(old);
    gchar *dd2 = g_build_filename(tmp, "data", NULL);
    g_assert_false(g_file_test(dd2, G_FILE_TEST_EXISTS));
    g_free(dd2);
    /* Tear down. */
    gchar *p;
    p = g_build_filename(tmp, "ver", "0.8.12", NULL);
    g_remove(p);
    g_free(p);
    p = g_build_filename(tmp, "ver", NULL);
    g_remove(p);
    g_free(p);
    p = g_build_filename(tmp, "current.txt", NULL);
    g_remove(p);
    g_free(p);
    g_remove(tmp);
    g_free(tmp);
}

int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/verdir/app-dir", test_app_dir);
    g_test_add_func("/verdir/current-and-dir", test_current_and_dir);
    g_test_add_func("/verdir/cmp", test_ver_cmp);
    g_test_add_func("/verdir/forward", test_forward);
    g_test_add_func("/verdir/cleanup", test_cleanup);
    g_test_add_func("/verdir/legacy", test_legacy);
    return g_test_run();
}
