#include "text_target.h"
#include <gio/gio.h>
#include <stdio.h>
int main(void){
 GSubprocess *app=g_subprocess_new(G_SUBPROCESS_FLAGS_STDIN_PIPE,NULL,g_getenv("GTV_TEST_TEXT_FIXTURE"),NULL);
 GtvTextTarget *target=NULL;
 for(int i=0;i<25 && !target;i++){g_usleep(200000);target=gtv_text_target_select("GTV probe 7294 — Xin chào.","Hello");}
 gboolean ok=FALSE;
 if(target){g_output_stream_write_all(g_subprocess_get_stdin_pipe(app),"Hello\n",6,NULL,NULL,NULL);for(int i=0;i<20&&!ok;i++){g_usleep(100000);ok=gtv_text_target_verify(target);}gtv_text_target_free(target);}
 printf("Native GTK selection and read-back verified: %d\n",ok);
 g_subprocess_force_exit(app);g_subprocess_wait(app,NULL,NULL);g_object_unref(app);return ok?0:1;
}
