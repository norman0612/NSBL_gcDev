#include <stdio.h>
#include "GC.h"

int main() {
    gcInit();
    int i,j,k,l;
    gcRef( &i, 1 );
    gcRef( &j, 1 );
    gcRef( &k, 1 );
    gcRef( &i, 1);

    gcDef( &l, 1);
    gcDef( &i, 1);
    gcDef( &i, 1);
    gcDel();
}
