/* Deterministic process fixture: tests the actual C Ollama subprocess protocol. */
#include <glib.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    const gchar *url=NULL,*timeout=NULL;
    for(int i=1;i+1<argc;i++) {
        if(!strcmp(argv[i],"--url")) url=argv[i+1];
        if(!strcmp(argv[i],"--max-time")) timeout=argv[i+1];
    }
    if(!url || !timeout) return 2;
    if(g_str_has_prefix(url,"http://fail/")) return 22;
    if(g_str_has_suffix(url,"/api/tags")) {if(strcmp(timeout,"1")) return 2;puts("{}");return 0;}
    if(!g_str_has_suffix(url,"/api/generate") || (strcmp(timeout,"20") && strcmp(timeout,"120"))) return 2;
    GString *body=g_string_new("");
    int c;while((c=getchar())!=EOF) g_string_append_c(body,c);
    if(strstr(body->str,"invalid model")){
        puts("{\"error\":\"invalid model name\"}");g_string_free(body,TRUE);return 22;
    }
    gboolean valid=strstr(body->str,"\"stream\":false") && strstr(body->str,"qwen2:0.5b");
    if(!strcmp(timeout,"120")) valid=valid &&
        (strstr(body->str,"Translate this Vietnamese text into English:") ||
         strstr(body->str,"Rewrite this text clearly and naturally"));
    gboolean completion=strstr(body->str,"Typed word: ch")!=NULL;
    g_string_free(body,TRUE);
    if(!valid) return 2;
    if(completion){puts("{\"response\":\"chào, chocolate, chào\"}");return 0;}
    puts(g_str_has_prefix(url,"http://malformed/") ? "malformed" : "{\"response\":\"  không  \"}");
    return 0;
}
