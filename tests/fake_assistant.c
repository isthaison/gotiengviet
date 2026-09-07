#include <glib.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
    if(argc<2 || strcmp(argv[1],"--stdin"))return 2;
    GString *input=g_string_new("");int c;
    while((c=getchar())!=EOF)g_string_append_c(input,c);
    gboolean ok=g_file_set_contents(g_getenv("GTV_ASSISTANT_CAPTURE"),input->str,input->len,NULL);
    g_string_free(input,TRUE);return ok?0:1;
}
