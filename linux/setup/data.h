#ifndef GTV_SETUP_DATA_H
#define GTV_SETUP_DATA_H
#include <gtk/gtk.h>
/* Macro/emoji manager dialog. Saves the user file and reloads this
 * process's tables; the IBus engine reads the file at startup, so the
 * dialog offers `ibus restart` on close when something changed. */
void data_show(GtkWindow *parent);
#endif
