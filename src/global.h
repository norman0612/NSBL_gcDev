#ifndef GLOBAL_H__JZ__12345
#define GLOBAL_H__JZ__12345
#include <stdio.h>
#include "SymbolTable.h"

/** util */
#ifdef _DEBUG
extern FILE* DEBUGIO;
#endif
#ifndef _NO_LOG
extern FILE* LOGIO;
#endif
extern long long LEXLINECOUNTER;


/** SymbolTable */
extern SymbolTable*            s_table;
extern SymbolTable*            tmp_table;
extern SymbolTableStack*       s_stack;
extern int                     isDynamicScope;
extern int                     isNoTypeCheck;
extern int                     maxLevel;

/** Error */
extern int ERRNO;
extern FILE* ERRORIO;

/** code */
extern char * OUTFILE;
extern FILE * OUTFILESTREAM;
extern char ** INDENT;
extern int  inLoop, inFunc, inMATCH, existMATCH, nMATCHsVab, existPIPE;
extern GList *returnList, *noReturn, *FuncParaList;
extern char * matchStaticVab, *frontDeclExp, *frontDeclExpTmp1;
#endif
