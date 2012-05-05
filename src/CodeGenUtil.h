// author : Jing
#ifndef CODEGENUTIL_H_NSBL_
#define CODEGENUTIL_H_NSBL_
#include "SymbolTable.h"

// string operation
char * strCatAllocBase(char* sep, int n, char ** ptr);
char * strCatAlloc(char* sep, int n, ...);
char * strRightCatAlloc(char * base, char* sep, int n, ...);
void  strFreeAll(int n, ...);
#define strCatAllocSpace(n,...)       strCatAlloc(' ',n,...)

// auxiliary funcs
char * strLine(int l);
char * codeFreeFuncName( int type );
char * codeRemoveAttrFuncName( int type );
char * opMacro(int ma);
char * DynOP(int ma);
char * typeMacro(int t);
char * assignFunc(int t);
char * gotolabel();
char * tmpReturnTmp();
char * tmpMatch();
char * tmpMatchStr();
char * tmpMatchStrVab();
char * tmpGraphVab();
SymbolTableEntry* tmpVab(int type, ScopeId sid);
char * tmpVabAssign( SymbolTableEntry* e, char * value );
char * tmpVabDel( SymbolTableEntry* e );
void codeIndentInit();
void codeIndentFree();

#endif
