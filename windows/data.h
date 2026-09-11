#ifndef GTV_DATA_H
#define GTV_DATA_H

#include <windows.h>

/* Macro/emoji table manager: list, add/update, delete, open data folder.
 * Writes the user file and reloads this process's tables; other processes
 * (e.g. TSF hosts) pick the file up on restart. */
void gtv_data_show(HWND parent);

#endif /* GTV_DATA_H */
