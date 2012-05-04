#include "GC.h"
#include <stdio.h>
#include <stdlib.h>

GHashTable * GCH;

void init_GC( GHashTable ** GChash ) {
    *GChash = g_hash_table_new( g_direct_hash, g_direct_equal );
}

void GC_Output_Entry ( gpointer key, gpointer value, gpointer stream ) {
    GC_Entry * gce = (GC_Entry *) value;
    FILE * out = (FILE *) stream;
    fprintf(out, "   | %12p | %4d | %4d |\n", gce->ptr, gce->type, gce->nref);
}

void GC_Out( GHashTable * GChash, FILE * out ) {
    fprintf(out, "GC |   pointer    | type | nref |\n");
    g_hash_table_foreach( GChash, & GC_Output_Entry, (void *) out );
}

void GC_Remove_Entry( gpointer key, gpointer value, gpointer dummy ) {
    GC_Entry * gce = (GC_Entry *) value;
    free( gce );
}

void del_GC( GHashTable * GChash ) {
    int ll = g_hash_table_size( GChash );
    if (ll!=0) {
        fprintf(stdout, "Warning: GC: member still exsits.\n");
        GC_Out( GChash, stdout );
        g_hash_table_foreach( GChash, & GC_Remove_Entry, NULL );
    }
    g_hash_table_destroy( GChash );
}

GC_Entry * GC_New_Entry( GHashTable * GChash, void * ptr, int type ){
    GC_Entry * gce = (GC_Entry *) malloc ( sizeof( GC_Entry ) );
    gce->ptr = ptr;
    gce->type = type;
    gce->nref = 0;
    g_hash_table_insert( GChash, (void *) ptr, (void *) gce );
    return gce;
}

int GC_Ref( GHashTable * GChash, void * ptr, int type ) {
    GC_Entry *gce = (GC_Entry *) g_hash_table_lookup( GChash, ptr );
    if (gce == NULL) gce = GC_New_Entry( GChash, ptr, type );
//    GC_Out( GChash, stdout );
    return ++gce->nref;
}
    
int GC_Deref( GHashTable * GChash,  void * ptr, int type ) {
    GC_Entry *gce = (GC_Entry *) g_hash_table_lookup( GChash, ptr );
    if (gce == NULL) {
        fprintf(stdout, "Error: GC: delete not existing member %p of type %d.\n", ptr, type);
        GC_Out( GChash, stdout );
        return 99;
    }
    if (gce->nref == 1) {
        g_hash_table_remove( GChash, ptr );
        free(gce);
    }
    return --gce->nref;
}
