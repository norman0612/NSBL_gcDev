// author : Jing

#ifndef GLOBAL_H_NSBL_
#define GLOBAL_H_NSBL_
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

/** code Generation */
extern char * OUTFILE;
extern FILE * OUTFILESTREAM;
extern char ** INDENT;
extern int  inLoop, inFunc, inFuncLiteral, isFunc, inMATCH, existMATCH, nMATCHsVab, existPIPE;
extern GList *returnList, *noReturn, *FuncParaList, *returnList2, *noReturn2;
extern char * matchStaticVab, *frontDeclExp, *frontDeclExpTmp1;
#endif
