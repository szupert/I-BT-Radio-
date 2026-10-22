#include <esp_heap_caps.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
enum { reserve=262144,maximum=524288 };
void* ibt_codec_malloc(size_t bytes){
    if(!bytes)bytes=1;
    if(bytes>maximum||heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<bytes+reserve)return NULL;
    void* p=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(p&&heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<reserve){heap_caps_free(p);return NULL;}
    return p;
}
void* ibt_codec_calloc(size_t count,size_t bytes){
    if(bytes&&count>SIZE_MAX/bytes)return NULL;
    const size_t n=count*bytes;void* p=ibt_codec_malloc(n);if(p)memset(p,0,n);return p;
}
void* ibt_codec_new(size_t bytes){return ibt_codec_malloc(bytes);}
void ibt_codec_free(void* p){if(p)heap_caps_free(p);}
void* ibt_codec_realloc(void* p,size_t bytes){
    if(!p)return ibt_codec_malloc(bytes);
    if(!bytes){ibt_codec_free(p);return NULL;}
    // Allocate-copy-free preserves the old allocation if growth is denied.
    void* next=ibt_codec_malloc(bytes);if(!next)return NULL;
    size_t old=heap_caps_get_allocated_size(p);memcpy(next,p,old<bytes?old:bytes);ibt_codec_free(p);return next;
}
char* ibt_codec_strdup(const char* s){
    if(!s)return NULL;size_t n=strnlen(s,8193);if(n>8192)return NULL;
    char* p=ibt_codec_malloc(n+1);if(p)memcpy(p,s,n+1);return p;
}
// The pinned ESP32 binary's module argument is one ignored 32-bit register.
// Its weak defaults forward args 2/3 to malloc/calloc/realloc (verified in ELF).
void* media_lib_module_malloc(uint32_t module,size_t bytes){(void)module;return ibt_codec_malloc(bytes);}
void* media_lib_module_calloc(uint32_t module,size_t count,size_t bytes){(void)module;return ibt_codec_calloc(count,bytes);}
void* media_lib_module_realloc(uint32_t module,void* p,size_t bytes){(void)module;return ibt_codec_realloc(p,bytes);}
void media_lib_free(void* p){ibt_codec_free(p);}
