// author : Jing
#ifndef GC_H_NSBL_
#define GC_H_NSBL_

#include <glib/ghash.h>
#include <stdio.h>

typedef struct {
    void * ptr;         //  object pointer
    int nref;           //  number of references
    int type;           //  type of object
} GC_Entry;

extern GHashTable * GCH;
void init_GC( GHashTable ** GChash );
void del_GC( GHashTable * GChash );
GC_Entry * GC_New_Entry( GHashTable * GChash, void * ptr, int type );
int GC_Ref( GHashTable * GChash, void * ptr, int type );
int GC_Deref( GHashTable * GChash,  void * ptr, int type );
void GC_Out( GHashTable * GChash, FILE * out );


// IMPORTANT : use this wrapper
#define gcInit()            init_GC( &GCH )
#define gcDel()             del_GC( GCH )
#define gcRef(p,t)          GC_Ref( GCH, p, t )
#define gcDef(p,t)          GC_Deref( GCH, p, t )
#define gcOUT(o)            GC_Out( GCH, o )
#endif
