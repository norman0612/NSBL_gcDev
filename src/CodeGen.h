// author : Jing , Lixing 
#ifndef CODEGEN_H_NSBL_
#define CODEGEN_H_NSBL_
#include "ASTree.h"

#define REMOVE_DYN  0xF01

// node->tmp[]
#define GLOBAL_TMP  0
#define MATCH_TMP   1

int codeGen (struct Node * node);
void derivedTypeInitCode(struct Node* node, int type, int isglobal);
void stringInitCode(struct Node* node, int type, int isglobal);
void listInitCode(struct Node* node, int type, int isglobal);
int listCountCheck(struct Node* node, int type);
int codeAttr ( struct Node * node );
char * codeGetAttrVal( char * operand, int type, int lno );
char * codeFrontDecl(int lvl );
int codeAssignLeft( struct Node * node);
int codeFuncWrapDynArgs(struct Node* node, GArray* tcon, int* cnt);
char * codeForFreeDerivedVabInScope(ScopeId sid, int type, GList * gl, ScopeId lvl, int which);
char * codeForInitTmpVabInScope ( ScopeId sid, int type, GList * gl, ScopeId lvl, int which );
char * allFreeCodeInScope(ScopeId sid, GList * gl, ScopeId lvl);
char * allInitTmpVabCodeInScope(ScopeId sid, GList * gl, ScopeId lvl);
GList * getAllParaInFunc(struct Node * node, GList * gl);
GList * getReturnVab( struct Node * node, GList * gl);
GList * getAllScopeIdInside( struct Node * node, GList * gl, struct Node * target, int * rlt);

int codeAllGen(struct Node* node, char ** mainCode, char ** funCode);
void codeAllFuncLiteral(struct Node* node, char ** code);
void codeInclude(char ** code);
void codeAllGlobal(struct Node* node, char ** code);
char * wapperMainCode(char * mainBodyCode);
void exportCode(char * code);

#endif
