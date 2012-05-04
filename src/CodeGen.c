/********************************************************************
 * CodeGen.c                                                        *
 * This is the source for code generation in NSBL. Code Gen is done *
 * on the ASTree, via post-order traversal.                         *
 *******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "CodeGen.h"
#include "SymbolTable.h"
#include "Parser.tab.h"
#include "global.h"
#include "Error.h"
#include "operator.h"
#include "CodeGenUtil.h"
#include "NSBLio.h"
#include "Derivedtype.h"

char * OUTFILE;                 // Output file name 
FILE * OUTFILESTREAM;           // Output file stream

int     inLoop, 
        inFunc;                 // flags to indicate : inside of loop or func
int     inMATCH,                // flags to indicate : inside of match operator
        existMATCH,             //                   : exist match operator in subtree of AST
        existPIPE,              //                   : exist pipe operator in subtree of AST
        nMATCHsVab;             // count number of dynamic variables in Match
GList *returnList, *noReturn, *FuncParaList;
char * matchStaticVab, *frontDeclExp, *frontDeclExpTmp1;
char * LoopGotoLabel;
struct Node * FuncBody, * LoopBody;

void derivedTypeInitCode(struct Node* node, int type, int isglobal){
	if(node->token == AST_COMMA){
		derivedTypeInitCode(node->child[0], type, isglobal);
		derivedTypeInitCode(node->child[1], type, isglobal);
    	node->code = strCatAlloc("",2, node->child[0]->code, node->child[1]->code);
        if(node->scope[0]==0) node->codetmp = strCatAlloc("",3,node->child[0]->codetmp,",",node->child[1]->codetmp);
	}else if (node->token == IDENTIFIER) {
        codeGen(node);
        if(isglobal)
            node->code = strCatAlloc("",3 ,INDENT[node->scope[0]],node->symbol->bind," = NULL; ");
        else
            node->code = strCatAlloc("",5 ,INDENT[node->scope[0]],sTypeName(type), " * ",node->symbol->bind," = NULL; ");
        
		switch(type){
			case GRAPH_T:
                node->code = strRightCatAlloc(node->code,"",3 ,"assign_operator_graph ( &( ",
                    node->symbol->bind," ) , new_graph() );\n");
				break;
			case VERTEX_T:
                node->code = strRightCatAlloc(node->code,"",3 ,"assign_operator_vertex ( &( ",
                    node->symbol->bind," ) ,new_vertex() );\n");
				break;
			case EDGE_T:
                node->code = strRightCatAlloc(node->code,"",3 ,"assign_operator_edge ( &( ",
                    node->symbol->bind," ) ,new_edge() );\n");
				break;
			default:
				break;
		}
	}
    else if (node->token == AST_ASSIGN) {
        codeGen(node->child[0]); codeGen(node->child[1]);
        if (node->child[1]->type != type) {
            ERRNO= ErrorInitDerivedType;
            errorInfo(ERRNO, node->line, "type mismatch for the initialization of derived type.\n");
            node->code = NULL;
            return;
        }
        if(isglobal)
            node->code = strCatAlloc("",3 ,INDENT[node->scope[0]],node->child[0]->symbol->bind," = NULL; ");
        else
            node->code = strCatAlloc("",5 ,INDENT[node->scope[0]],sTypeName(type), " * ",node->child[0]->symbol->bind," = NULL; ");
        switch(type) {
            case GRAPH_T:
                node->code = strRightCatAlloc(node->code,"",5 ,"assign_operator_graph ( &( ",
                    node->child[0]->symbol->bind," ) , ", node->child[1]->code, " );\n");
                break;
            case VERTEX_T:
                node->code = strRightCatAlloc(node->code,"",5 ,"assign_operator_vertex ( &( ",
                    node->child[0]->symbol->bind," ) , ", node->child[1]->code, " );\n");
                break;
            case EDGE_T:
                node->code = strRightCatAlloc(node->code,"",5 ,"assign_operator_edge ( &( ",
                    node->child[0]->symbol->bind," ) , ", node->child[1]->code, " );\n");
                break;
            default:
                break;
        }
        if(node->scope[0]==0) node->codetmp = strCatAlloc("",1,node->child[0]->codetmp);
    }
    else {
        ERRNO = ErrorIllegalDerivedTypeDeclaration;
        errorInfo(ERRNO, node->line, "Illegal declaration of derived type  (vertex, edge, graph).\n");
    }
}

void stringInitCode(struct Node* node, int type, int isglobal){
	if(node->token == AST_COMMA){
		stringInitCode(node->child[0], type, isglobal);
		stringInitCode(node->child[1], type, isglobal);
		node->code = strCatAlloc("", 2, node->child[0]->code, node->child[1]->code);
        if(node->scope[0]==0) node->codetmp = strCatAlloc("",3,node->child[0]->codetmp,",",node->child[1]->codetmp);
	}else if(node->token == AST_ASSIGN){
        codeGen(node->child[0]); codeGen(node->child[1]);
		if(isglobal)
			node->code = strCatAlloc("",5,INDENT[node->scope[0]], 
                node->child[0]->symbol->bind, " = ", node->child[1]->code, ";\n"); 
		else
			node->code = strCatAlloc("",7,INDENT[node->scope[0]], 
                sTypeName(type), " * ", node->child[0]->symbol->bind, " = ", node->child[1]->code, ";\n");	
        if(node->scope[0]==0) node->codetmp = strCatAlloc("",1,node->child[0]->codetmp);
	}else{
        codeGen(node);
		if(isglobal)
			node->code = strCatAlloc("",3,INDENT[node->scope[0]], node->symbol->bind, " = g_string_new(\"\");\n"); 
		else
			node->code = strCatAlloc("",5,INDENT[node->scope[0]], sTypeName(type), " * ", node->symbol->bind, " = g_string_new(\"\");\n");
	}
}

void listInitCode(struct Node* node, int type, int isglobal){
    int mtype = (type == VLIST_T) ? VERTEX_T : EDGE_T;
    if(node->token == AST_COMMA){
        listInitCode(node->child[0], type, isglobal);
        listInitCode(node->child[1], type, isglobal);
        node->code = strCatAlloc("", 2, node->child[0]->code, node->child[1]->code);
        if(node->scope[0]==0) node->codetmp = strCatAlloc("",3,node->child[0]->codetmp,",",node->child[1]->codetmp); 
    }
    else if (node->token == AST_ASSIGN){
        codeGen(node->child[0]); codeGen(node->child[1]);
        char num[32];
        int flag = listCountCheck(node->child[1], mtype);
        int nArgs = (flag > 0)? flag : 0;
        sprintf(num,"%d\0", nArgs);
        node->code = strCatAlloc("", 9, INDENT[node->scope[0]],
            (isglobal)? "" : "ListType * ", node->child[0]->symbol->bind, 
                " = NULL; assign_operator_list ( &( ", node->child[0]->symbol->bind,
                ") , list_declaration( ", typeMacro(mtype), " , ", num);
        if(nArgs>0) node->code = strRightCatAlloc( node->code, "",3, " , ", node->child[1]->code, ") );\n");
        else node->code = strRightCatAlloc( node->code, "", 1, ") );\n");        
        // if not init by [], 
        if (flag<0) {
            node->code = strRightCatAlloc( node->code, "", 5,
                "assign_operator_list ( & (", node->child[0]->symbol->bind, " ) , ( ",
                node->child[1]->code, " ) );\n");
        }
        if(node->scope[0]==0) node->codetmp = strCatAlloc("",1,node->child[0]->codetmp);
    }
    else { // empty list
        codeGen(node);
        node->code = strCatAlloc("", 8, INDENT[node->scope[0]],
            (isglobal)? "" : "ListType * ", node->symbol->bind,
                " = NULL; assign_operator_list ( &( ", node->symbol->bind, " ) , list_declaration( ", typeMacro(mtype), " , 0 ) );\n");    
    }        
}

// count number of initializor in [ ...]
int listCountCheck(struct Node* node, int type){
	int count = 0, flag = 0;
	struct Node* tn = node;
	if(tn->token != AST_LIST_INIT) {
        return -1;
    }
    if(tn->nch > 0) {
		tn = tn->child[0];
    	while (tn->token == AST_COMMA ) {
			if (tn->child[1]->token != IDENTIFIER) { flag = ErrorAssignmentExpression; break; }
        	if ( tn->child[1]->type != type ) { flag = ErrorListMixedType; break; }
        	tn = tn->child[0];
        	count++;
    	}
    	if (tn->token == IDENTIFIER && flag == 0) {
        	if ( tn->type != type ) flag = ErrorListMixedType;
        	count++;
    	//}
		//else if(tn->token == AST_ASSIGN){
        //	if ( tn->type != type ) flag = ErrorListMixedType;
        //	count++;
		}else{
			flag = ErrorAssignmentExpression;
		}
    	if (flag == ErrorListMixedType) {
        	ERRNO = flag;
        	errorInfo(ERRNO, node->line, "list Initialization with wrong type.\n");
    	}
		else if(flag == ErrorAssignmentExpression){
			ERRNO = flag;
			errorInfo(ERRNO, node->line, "list Initialization with wrong argument expression.\n");
		}
	}
    return count;
}

/*
char * codeDel( struct Node * node ) {
    if (node == NULL) return NULL;
#ifdef _DEBUG
    debugInfo("codeDel: NODE = %d\n",node->token);
#endif
    if (node->token == AST_COMMA) {
        char * lfc = codeDel( node->child[0] );
        char * rtc = codeDel( node->child[1] );
        return strCatAlloc("", 2, lfc, rtc);
    }
    else if (node->token == AST_DEL_ATTRIBUTE) {
        codeGen(node);
        char * freeFunc = codeRemoveAttrFuncName(node->child[0]->type);
        return strCatAlloc("", 7, INDENT[node->scope[0]],freeFunc," ( ", node->child[0]->code, " , \"", node->child[1]->code, "\" );\n");
        return NULL;
    }
    else if (node->token == IDENTIFIER) {
        int type = node->symbol->type;
        if (type<=FLOAT_T || type>=FUNC_T) {
            ERRNO = ErrorDelVariableOfWrongType;
            errorInfo(ERRNO,node->line,"delete variable of type `%s'.\n",sTypeName(type));
            return NULL;
        } 
        char * freeFunc = codeFreeFuncName(type);
        return strCatAlloc("",7,INDENT[node->scope[0]],freeFunc," ( ", node->symbol->bind, " );", node->symbol->bind, " = NULL;\n");
    }
    else {
        fprintf(stderr, "codeDel: unknown node %d !!!!!!\n", node->token);
        return NULL;
    }
}*/

int codeAttr ( struct Node * node ) {
    // codeGen should already be called on this node, before codeAttr
    char * code = node->code;
    if(node->type<=0 || node->type>STRING_T) {
        ERRNO = ErrorBinaryOperationWithDynamicType;
        errorInfo(ERRNO, node->line, "Binary Operation with Dynamic Type.\n");
        return 1;
    }
    SymbolTableEntry* e = tmpVab( DYN_ATTR_T, node->scope[1] );
    frontDeclExp = strRightCatAlloc( frontDeclExp, "",8 ,
        INDENT[node->scope[0]], "assign_operator_attr( &( ",
            e->bind, " ), new_attr_", typeMacro(node->type),
        "( ", code," ) );\n"); 
    node->code = strCatAlloc("", 1, e->bind);
    free(code);
    //node->tmp[0] = REMOVE_DYN;  // set remove flag
    return 0;
}

char * codeGetAttrVal( char * operand, int type, int lno ) {
    if(type != BOOL_T && type != INT_T && type != FLOAT_T && type != STRING_T) {
        ERRNO = ErrorGetAttrForWrongType;
        errorInfo(ERRNO, lno, "get attribute value for wrong type.\n");
        return NULL;
    }
    return strCatAlloc("",7,"get_attr_value_",typeMacro(type),
            " ( ",  operand, " , ", strLine(lno), " ) " );
}

char * codeFrontDecl(int lvl ) {
    char * decl = NULL;
    if(1||existMATCH == 1 || existPIPE == 1){ // for MATCH
       decl = strRightCatAlloc(decl, "", 2,INDENT[lvl],frontDeclExp);
       free(frontDeclExp); frontDeclExp= NULL;
       existMATCH = 0; existPIPE = 0;
    }
    return decl;
}

int codeAssignLeft( struct Node * node) {
    if (node->token == IDENTIFIER) {
        codeGen(node);
    }
    else if (node->token == AST_ATTRIBUTE) {
        // assume: NO assignment in MATCH
        node->child[0]->code = strCatAlloc("", 1, node->child[0]->symbol->bind);
        node->child[1]->code = strCatAlloc("", 1, node->child[1]->lexval.sval);
        // 
        SymbolTableEntry* et = tmpVab( DYN_ATTR_T, node->scope[1] );
        char * code = NULL;
        // put "1" for xxx_get_attribute to auto allocate storage
        if(node->child[0]->type == VERTEX_T )
            code = strCatAlloc("", 7, "vertex_get_attribute( ",
                    node->child[0]->code, " ,  \"", node->child[1]->code, "\", 1, ", strLine(node->line)," )");
        else if(node->child[0]->type == EDGE_T )
                code = strCatAlloc("", 7, "edge_get_attribute( ",
                    node->child[0]->code, " ,  \"", node->child[1]->code, "\", 1, ", strLine(node->line)," )");
        else {
                ERRNO = ErrorGetAttrForWrongType;
                errorInfo(ERRNO, node->line, "Access attribute for type `%s'.\n",
                    sTypeName(node->child[0]->type) );
        }
        char * cass = tmpVabAssign( et, code );
        frontDeclExp = strRightCatAlloc( frontDeclExp, "" , 1, cass );
        node->code = strCatAlloc( "", 1, et->bind );
        free(code);free(cass);
        node->type = DYNAMIC_T;
    }
    else if (node->token == DYN_ATTRIBUTE) {
        SymbolTableEntry* et = tmpVab( DYN_ATTR_T, node->scope[1] );
        char* code = strCatAlloc("",6,
                "object_get_attribute( _obj, _obj_type, ",
                "\"::",node->lexval.sval, "\", 1, ",strLine(node->line)," ) ");
        char * cass = tmpVabAssign( et, code );
        frontDeclExp = strRightCatAlloc( frontDeclExp, "" , 1, cass );
        node->code = strCatAlloc( "", 1, et->bind );
        free(code);free(cass);
        node->type = DYNAMIC_T;
    }
}

int codeFuncWrapDynArgs(struct Node* node, GArray* tcon, int* cnt){
    if(node->token == AST_COMMA) {
        codeFuncWrapDynArgs(node->child[0], tcon, cnt);
        codeFuncWrapDynArgs(node->child[1], tcon, cnt);
        node->code = strCatAlloc("", 3, node->child[0]->code, " , ", node->child[1]->code);
    }
    else if (node->token == AST_ARG_EXPS) {
        codeGen(node);
        if(tcon->len > *cnt) {
            int rtype = g_array_index(tcon, int, *cnt);
            if(node->type < 0) {
                char * ctmp = node->code;
                node->code = codeGetAttrVal(ctmp, rtype , node->line);
                free(ctmp);
            }
            else if (node->type >=0 && node->type != rtype ) {
                ERRNO = ErrorFunctionCallIncompatibleParameterType;
                errorInfo(ERRNO, node->line, "function arg has incompatible arguments to its declaration.\n");
            }
        }
        (*cnt)++;
    }
    return 0;
}

char * codeForFreeDerivedVabInScope(ScopeId sid, int type, GList * gl, ScopeId lvl, int which){
    GList * vals = NULL;
    if (which == 0) vals = sTableAllVarScope( sid, type );
    else if (which == 1) vals = tmpTableAllVarScope( sid, type );

    char * code = NULL, * freefunc = codeFreeFuncName(type);
    int i, l = g_list_length( vals );
    SymbolTableEntry * e;
#ifdef _DEBUG
    int ll = g_list_length( gl );
    debugInfo("codeForFreeDerivedVabInScope: sid=%d, type=%d, l=%d, ll=%d\n", sid,type,l,ll);
    if(ll>0) {
        int i;
        for (i=0; i<ll; ++i) {
            e = (SymbolTableEntry *) g_list_nth_data( gl, i );
            debugInfoExt("      gl[%d] ==> %s\n", i, e->bind);
        }
    }
#endif
    for ( i=0; i<l; ++i ){
        e = (SymbolTableEntry *) g_list_nth_data( vals, i );
        if( g_list_find( gl, (gpointer) e ) == NULL )
            code = strRightCatAlloc( code, "", 7, INDENT[lvl], freefunc, "( ", e->bind, " );", e->bind, " = NULL;\n" );
    }
    g_list_free( vals );
    return code;
}

char * codeForInitTmpVabInScope ( ScopeId sid, int type, GList * gl, ScopeId lvl, int which ){
    GList * vals = NULL;
    if (which == 0) vals = sTableAllVarScope( sid, type );
    else if (which == 1) vals = tmpTableAllVarScope( sid, type );

    char * code = NULL, * freefunc = codeFreeFuncName(type);
    int i, l = g_list_length( vals );
    SymbolTableEntry * e;
    int isptr = ( type == VLIST_T || type == ELIST_T || type == GRAPH_T || type == VERTEX_T ||
        type == EDGE_T || type == DYN_ATTR_T ) ? 1: 0 ;
    char * def;
    switch (type) {
        case BOOL_T: def = "false"; break;
        case INT_T: def = "0"; break;
        case FLOAT_T: def = "0.0"; break;
        default: def = "NULL"; break;
    }
    for ( i=0; i<l; ++i ){
        e = (SymbolTableEntry *) g_list_nth_data( vals, i );
        if( g_list_find( gl, (gpointer) e ) == NULL ) {
            code = strRightCatAlloc( code, "", 7, INDENT[lvl], sTypeName(e->type),
                 (isptr) ? " * ": " ", e->bind, " = ", def, ";\n");
        }
    }
    g_list_free( vals );
    return code;
}

char * allFreeCodeInScope(ScopeId sid, GList * gl, ScopeId lvl) {
    char * sc = codeForFreeDerivedVabInScope( sid, STRING_T, gl, lvl, 0 );
    char * vc = codeForFreeDerivedVabInScope( sid, VERTEX_T, gl, lvl, 0 );
    char * ec = codeForFreeDerivedVabInScope( sid, EDGE_T, gl, lvl, 0 );
    char * gc = codeForFreeDerivedVabInScope( sid, GRAPH_T, gl, lvl, 0 );
    char * vlc = codeForFreeDerivedVabInScope( sid, VLIST_T, gl, lvl, 0 );
    char * elc = codeForFreeDerivedVabInScope( sid, ELIST_T, gl, lvl, 0 );
    char * tsc = codeForFreeDerivedVabInScope( sid, STRING_T, gl, lvl, 1 );
    char * tvc = codeForFreeDerivedVabInScope( sid, VERTEX_T, gl, lvl, 1 );
    char * tec = codeForFreeDerivedVabInScope( sid, EDGE_T, gl, lvl, 1 );
    char * tgc = codeForFreeDerivedVabInScope( sid, GRAPH_T, gl, lvl, 1 );
    char * tvlc = codeForFreeDerivedVabInScope( sid, VLIST_T, gl, lvl, 1 );
    char * telc = codeForFreeDerivedVabInScope( sid, ELIST_T, gl, lvl, 1 );
    char * tatt = codeForFreeDerivedVabInScope( sid, DYN_ATTR_T, gl, lvl, 1 );

    char * rlt = strCatAlloc("", 13, sc, vc, ec, gc, vlc, elc,
                    tsc, tvc, tec, tgc, tvlc, telc, tatt );
    free(sc);free(vc);free(ec);free(gc);free(vlc);free(elc);
    free(tsc);free(tvc);free(tec);free(tgc);free(tvlc);free(telc);free(tatt);
    return rlt;
}

char * allInitTmpVabCodeInScope(ScopeId sid, GList * gl, ScopeId lvl) {
    char * tvc = codeForInitTmpVabInScope( sid, VERTEX_T, gl, lvl, 1 );
    char * tec = codeForInitTmpVabInScope( sid, EDGE_T, gl, lvl, 1 );
    char * tgc = codeForInitTmpVabInScope( sid, GRAPH_T, gl, lvl, 1 );
    char * tvlc = codeForInitTmpVabInScope( sid, VLIST_T, gl, lvl, 1 );
    char * telc = codeForInitTmpVabInScope( sid, ELIST_T, gl, lvl, 1 );
    char * tatt = codeForInitTmpVabInScope( sid, DYN_ATTR_T, gl, lvl, 1);
    char * rlt =  strCatAlloc("", 6, tvc, tec,tgc,tvlc,telc,tatt);
    
    free(tvc);free(tec);free(tgc);free(tvlc);free(telc);free(tatt);
    return rlt;
}

GList * getAllParaInFunc(struct Node * node, GList * gl) {
    if (node ==NULL) return gl;
    else if (node->token == AST_COMMA) {
        gl = getAllParaInFunc(node->child[0], gl);
        gl = getAllParaInFunc(node->child[1], gl);
    }
    else if (node->token == AST_PARA_DECLARATION) {
        gl = g_list_append( gl, node->child[1]->symbol );
    }
    else {
        fprintf(stderr, "getAllParaInFunc: unknow node %d !!!!!!!!!!\n", node->token);
    }
    return gl;
}

GList * getReturnVab( struct Node * node, GList * gl) {
    if (node == NULL) return gl;
    else if (node->token == AST_JUMP_RETURN) {
        if (node->nch!=0) {
            gl = g_list_append( gl, node->child[0]->symbol );
        }
        return gl;
    }

    int i;
    for (i=0; i<node->nch; ++i) {
        gl = getReturnVab( node->child[i], gl );
    }
    return gl;
}

GList * getAllScopeIdInside( struct Node * node, GList * gl, struct Node * target, int * rlt) {
    if (node == NULL) return gl;
    int flag = (node == target);
    if (flag == 0) {   // I am not target, try my child
        int i;
        for (i=0; i<node->nch; ++i) {
            gl = getAllScopeIdInside( node->child[i], gl, target, &flag );
            if(flag != 0) break; // only one path
        }
    }
    if (flag == 1) { // find it 
        int tl = g_list_length( gl );
        int i, fflag = 0;
        for ( i=0; i<tl; i++ ) {   // check duplicate
            int * ii = g_list_nth_data( gl , i );
            if ( *ii == node->scope[1] ) { fflag = 1; break; }
        }
        if (!fflag) gl = g_list_append( gl, &(node->scope[1]) );
    }
    *rlt = flag;
    return gl;
}

/** recursively generate code piece on each node */
int codeGen (struct Node * node) {
    if( node == NULL ) return;
    int token = node->token, errflag = 0;
    char * op = opMacro(token);
    struct Node *lf, *rt, *sg;
    char* printFunc;
    char* var;
    char* endBrace;
    char* printVattr;
    char* printCall;    	
    char* fileloc;
    char* comma;
    switch (token) {
/************************************************************************************/
        case INTEGER_CONSTANT :
        case FLOAT_CONSTANT :
        case BOOL_TRUE :
        case BOOL_FALSE :
        case STRING_LITERAL :
            // code and type already done in ASTree.c
            break;
        case IDENTIFIER :
            // type is done when insert into symtable
            if (node->symbol->bind!=NULL){  // should always true 
                if(inMATCH==0){ // not in Match
                    node->code = strCatAlloc("", 1, node->symbol->bind);
                    if (node->type == VERTEX_T || node->type == EDGE_T || 
                            node->type == VLIST_T || node->type == ELIST_T
                                || node->type == STRING_T || node->type == GRAPH_T) {
                        node->codetmp = strCatAlloc("",2,"* ",node->symbol->bind);
                    }
                    else {
                        node->codetmp = strCatAlloc("",1,node->symbol->bind);
                    }
                }
                else{ // in Match
                    node->code = strCatAlloc("",3,"_str->",node->symbol->bind,"_match");
                    if (node->type == VERTEX_T || node->type == EDGE_T ||
                            node->type == VLIST_T || node->type == ELIST_T
                                || node->type == STRING_T || node->type == GRAPH_T) {
                        node->codetmp = strCatAlloc("",2,"* ",node->symbol->bind);
                    }
                    else {
                        node->codetmp = strCatAlloc("",1,node->symbol->bind);
                    }
                    matchStaticVab = strRightCatAlloc( matchStaticVab, "", 5, 
                        INDENT[1], sTypeName(node->type),
                        (node->type == VERTEX_T || node->type == EDGE_T ||
                            node->type == VLIST_T || node->type == ELIST_T
                                || node->type == STRING_T || node->type == GRAPH_T)
                                 ? " * " :" ",
                        node->symbol->bind,"_match;\n");
                    frontDeclExpTmp1 = strRightCatAlloc( frontDeclExpTmp1, "", 2,
                        (nMATCHsVab++==0) ? "" : " , ",
                        node->symbol->bind);
                }
            }
            else
                ERRNO = ErrorNoBinderForId;
            break;
        case DYN_ATTRIBUTE :
            node->code = strCatAlloc("",6,
                "object_get_attribute( _obj, _obj_type, ", 
                "\"::",node->lexval.sval, "\", 0, ", strLine(node->line)," ) ");
            node->type = DYNAMIC_T;
            break;
        case AST_DEL_ATTRIBUTE :
            node->child[0]->code = strCatAlloc("", 1, node->child[0]->symbol->bind);
            node->child[1]->code = strCatAlloc("", 1, node->child[1]->lexval.sval);
            break;
        case AST_COMMA :
            lf = node->child[0]; rt = node->child[1];
            codeGen( lf );codeGen( rt );
            node->code = strCatAlloc(" ",3,lf->code,",",rt->code);
            if(node->scope[0]==0){ // for global declaration
                node->codetmp = strCatAlloc(" ",3,lf->codetmp,",",rt->codetmp);
            }
            node->type = node->child[1]->type;
            break;
		case AST_LIST_INIT:
			node->type = LIST_INIT_T;
			if(node->child==NULL)
				node->code = strCatAlloc("", 1, " ");
			else{
				sg = node->child[0];
				codeGen(sg);
				node->code = strCatAlloc("", 1, sg->code);
			}
			break;
        case AST_TYPE_SPECIFIER :
            node->code = strCatAlloc(" ",1,sTypeName(node->lexval.ival));
            break;
        case AST_DECLARATION : {
            lf = node->child[0];
            rt = node->child[1];
            codeGen( lf );
            int ttype = lf->lexval.ival;
            if( ttype != VLIST_T && ttype != ELIST_T )
                codeGen( node->child[1] );
            node->code = codeFrontDecl(node->scope[0] );
            // when the declaration is in scope 0, we need to generate two places of code for c
            // 1. external global declaration 
            // 2. assignment in main func, if possible
            if(node->scope[0]==0) {
				switch(ttype){
					case GRAPH_T: 
					case VERTEX_T:
					case EDGE_T:
						derivedTypeInitCode(rt, ttype, 1);
						node->code = strRightCatAlloc(node->code,"", 1, rt->code);
						break;
					case STRING_T:
						stringInitCode(rt, ttype, 1);
					    node->code = strRightCatAlloc(node->code,"", 1, rt->code);
						break;
					case VLIST_T:
                    case ELIST_T:
                        listInitCode(rt, ttype, 1);
                        node->code = strRightCatAlloc(node->code,"", 1, rt->code);
                        break;
					default:
               		    node->code = strRightCatAlloc(node->code,"",3,INDENT[node->scope[0]],rt->code,";\n");
				}
                node->codetmp = strCatAlloc("",5,INDENT[node->scope[0]],lf->code," ",rt->codetmp,";\n");
            }
            // If scope > 0, no bother, just declaration everything in one c declaration
            else {
				switch(node->child[0]->lexval.ival){
					case GRAPH_T:
					case VERTEX_T:
					case EDGE_T:
                        derivedTypeInitCode(rt, ttype, 0);
                        node->code = strRightCatAlloc(node->code,"", 1, rt->code);
						break;
					case STRING_T:
                        stringInitCode(rt, ttype, 0);
                        node->code = strRightCatAlloc(node->code,"", 1, rt->code);
						break;
					case VLIST_T :
                    case ELIST_T :
                        listInitCode(rt, ttype, 0);
                        node->code = strRightCatAlloc(node->code,"", 1, rt->code);
                        break;
					default:
                		node->code = strRightCatAlloc(node->code,"",5,INDENT[node->scope[0]],lf->code," ",rt->code,";\n");
				}
            }
            break;
        }
/************************************************************************************/
        case DEL : {
            //node->code = codeDel(node->child[0]);
            break;
        }
/************************************************************************************/
        case AST_ASSIGN :               // assignment_operator 
            if(inMATCH > 0) {
                ERRNO = ErrorAssignInMatch;
                errorInfo ( ERRNO, node->line, "assignment in Match operator.\n");
                return ERRNO;
            }
            lf =  node->child[0]; rt = node->child[1];
            if(lf->token != IDENTIFIER && lf->token != AST_ATTRIBUTE && lf->token != DYN_ATTRIBUTE) {
                ERRNO = ErrorAssignLeftOperand;
                errorInfo ( ERRNO, node->line, "the left operand of assign operator MUST be IDENTIFIER or ATTRIBUTE.\n");
                return ERRNO;
            }
            codeAssignLeft(lf);
            codeGen(rt);
            // type check and implicit type conversion
            if(lf->type == rt->type && lf->type>=0 ) {
                if ( lf->type == INT_T || lf->type == FLOAT_T || lf->type == BOOL_T ) {
                    node->code = strCatAlloc(" ",3,lf->code,op,rt->code);
                    node->type = lf->type;
                }
                else if ( lf->type == STRING_T || lf->type == VLIST_T || lf->type == ELIST_T ||
                            lf->type == VERTEX_T || lf->type == EDGE_T || 
                                lf->type == GRAPH_T ){
                    char * func = assignFunc(lf->type);
					node->type = lf->type;
                    node->code = strCatAlloc("", 6,
                        func, " ( &(", lf->code, ") , (",
                        rt->code, ") ) " 
                    );
                }
                else {
                    ERRNO = ErrorOperatorNotSupportedByType;
                    errorInfo(ERRNO, node->line, "operator `%s' is not supported by type `%s'\n",op,sTypeName(lf->type));
                    return ERRNO;
                }
            }
            // float ==> int
            else if (lf->type == INT_T && rt->type == FLOAT_T)  {
                node->code = strCatAlloc(" ",4,lf->code,op,"(int)", rt->code);
                node->type = INT_T;
            }
            // int ==> float
            else if (lf->type == FLOAT_T && rt->type == INT_T) {
                node->code = strCatAlloc(" ",4,lf->code,op,"(float)", rt->code);
                node->type = FLOAT_T;
            }
            // DYNAMIC 
            else if (lf->type < 0 || rt->type < 0) {
                if (lf->type < 0 ) { // DYNAMIC = DYNAMIC or STATIC
                    int flag = 0;
                    if (rt->type >=0) flag = codeAttr(rt);
                    if (!flag){
                        frontDeclExp = strRightCatAlloc(frontDeclExp, "", 10,
                            INDENT[node->scope[0]],
                            "assign_operator (", lf->code, " , ", rt->code, 
                            (lf->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR",
                            (rt->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR",
                            " , ", strLine(node->line), " );\n "
                        );
                        node->code = strCatAlloc("", 1, lf->code);
                    }
                    node->type = DYNAMIC_T;
                    //node->tmp[0] = REMOVE_DYN;
                }
                else  { // STATIC = DYNAMIC
                    frontDeclExp = strRightCatAlloc(frontDeclExp, "", 11,
                        INDENT[node->scope[0]],
                        "assign_operator_to_static (", rt->code, " , ", typeMacro(lf->type), 
                        " , (void *)&", lf->code,  
                        (rt->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR",
                        " , ", strLine(node->line), " );\n" );
                    node->code = strCatAlloc("", 1, lf->code);
                    //debugInfo("%s\n",node->code);
                    node->type = lf->type;
                }
            }
            else { // ERROR
                node->code = NULL;
                ERRNO = ErrorTypeMisMatch;
                errorInfo(ERRNO, node->line, "type mismatch for the operands of operator `%s'\n",op);
                return ERRNO;
            }
            // for global declaration in c
            if(node->scope[0]==0){
                node->codetmp = strCatAlloc("",1,lf->codetmp);
            }
            break;

        case APPEND :      
			lf = node->child[0]; rt = node->child[1];
			codeGen( lf );codeGen( rt );
            // TODO
			if(lf->type==GRAPH_T && rt->type==VERTEX_T){
				node->code = strCatAlloc("", 5, "g_insert_v(", lf->code, ", ", rt->code, ")");
			}
			else if(lf->type==GRAPH_T && rt->type==EDGE_T){
				node->code = strCatAlloc("", 5, "g_insert_e(", lf->code, ", ", rt->code, ")");	
			}
			else if(lf->type==GRAPH_T && (rt->type==VLIST_T || rt->type == ELIST_T) ){
				node->code = strCatAlloc("", 5, "g_append_list(", lf->code, ", ", rt->code, ")");
			}
			else if(lf->type==VLIST_T && rt->type==VERTEX_T){
				node->code = strCatAlloc("", 5, "list_append(", lf->code, ", VERTEX_T, ", rt->code, ")");
			}
			else if(lf->type==ELIST_T && rt->type==EDGE_T){
				node->code = strCatAlloc("", 5, "list_append(", lf->code, ", EDGE_T, ", rt->code, ")");
			}else{
				ERRNO = ErrorAssignmentExpression;
				errorInfo(ERRNO, node->line, "append expression error\n");
				return ERRNO;
			}
            break;
/************************************************************************************/
        case OR  :          
        case AND :          
            lf =  node->child[0]; rt = node->child[1];
            codeGen(lf);codeGen(rt);
            node->type = BOOL_T;
            if(lf->type >= 0 && rt->type >= 0) {
                if (lf->type != rt->type){
                    ERRNO = ErrorTypeMisMatch;
                    errorInfo(ERRNO, node->line, "type mismatch for the operands of operator `%s'\n",op);
                    return ERRNO;
                }
                else if (lf->type == BOOL_T) {
                    node->code = strCatAlloc(" ",3,lf->code,op,rt->code);
                }
                else {
                    ERRNO = ErrorOperatorNotSupportedByType;
                    errorInfo(ERRNO, node->line, "operator `%s' is only supported by type `bool'\n",op);
                    return ERRNO;
                }
            }
            else { // DYNAMIC
                int flag = 0;
                if (lf->type > 0 && lf->type == BOOL_T) flag = codeAttr(lf);
                if (rt->type > 0 && rt->type == BOOL_T) flag = codeAttr(rt);
                if (lf->type > 0 && lf->type != BOOL_T ||
                        rt->type > 0 && rt->type != BOOL_T) {
                    ERRNO = ErrorOperatorNotSupportedByType;
                    errorInfo(ERRNO, node->line, "operator `%s' is only supported by type `bool'\n",op);
                    return ERRNO;
                }
                if(!flag) {
                    SymbolTableEntry* e = tmpVab( DYN_ATTR_T, node->scope[1] );
                    frontDeclExp = strRightCatAlloc( frontDeclExp, "", 16,
                        INDENT[node->scope[0]],
                        "assign_operator_attr ( &( ", e->bind, " ) , ",
                        " binary_operator ( ", lf->code, " , ", rt->code, " , ", DynOP(token),
                        ", FLAG_NO_REVERSE",
                        ( (lf->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR" ),
                        ( (rt->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR" ),
                        " , ", strLine(node->line), " ) );\n"
                    );

                    node->code = strCatAlloc("",1,e->bind);
                }
                //node->tmp[0] = REMOVE_DYN;
                node->type = DYN_BOOL_T;
            }    
            break;
/************************************************************************************/
        case EQ :           
        case NE :           
            lf =  node->child[0]; rt = node->child[1];
            codeGen(lf);codeGen(rt);
            if(lf->type >= 0 && rt->type >= 0){ // STATIC
                if (lf->type != rt->type) {
                    if(lf->type == INT_T && rt->type == FLOAT_T) {
                        node->code = strCatAlloc(" ",4,"(float)",lf->code,op,rt->code);
                        node->type = BOOL_T;
                    }
                    else if(lf->type == FLOAT_T && rt->type == INT_T) {
                        node->code = strCatAlloc(" ",4,lf->code,op,"(float)",rt->code);
                        node->type = BOOL_T;
                    }
                    else {
                        ERRNO = ErrorTypeMisMatch;
                        errorInfo(ERRNO, node->line, "type mismatch for the operands of operator `%s'\n",op);
                        return ERRNO;
                    }
                }
                else {
                    node->code = strCatAlloc(" ",3,lf->code,op,rt->code);
                    node->type = BOOL_T;
                }
            }
            else {  // DYNAMIC
                int flag = 0;
                if(lf->type >= 0) flag = codeAttr(lf);
                if(rt->type >= 0) flag = codeAttr(rt);
                if(!flag) {
                    SymbolTableEntry* e = tmpVab( DYN_ATTR_T, node->scope[1] );
                    frontDeclExp = strRightCatAlloc( frontDeclExp, "", 15,
                    INDENT[node->scope[0]],
                    "assign_operator_attr( &( ", e->bind,
                    ") , binary_operator ( ", lf->code, " , ", rt->code, " , ", DynOP(token),
                    ", FLAG_NO_REVERSE",
                    ( (lf->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR" ),
                    ( (rt->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR" ),
                    " , ", strLine(node->line), " ) );\n"
                    );
                    node->code = strCatAlloc("",1, e->bind);
                }
                node->tmp[0] = REMOVE_DYN;
                node->type = DYN_BOOL_T; 
            }
            break;
/************************************************************************************/
        case LT :           
        case GT :           
        case LE :           
        case GE :           
            lf =  node->child[0]; rt = node->child[1];
            codeGen(lf);codeGen(rt);
            node->type = BOOL_T;
            if(lf->type == rt->type && (lf->type == INT_T) || (lf->type == FLOAT_T ) ) 
                node->code = strCatAlloc(" ",3,lf->code,op,rt->code);
            else if (lf->type == INT_T && rt->type == FLOAT_T) 
                node->code = strCatAlloc(" ",4, "(float)",lf->code,op,rt->code);
            else if (rt->type == INT_T && lf->type == FLOAT_T) 
                node->code = strCatAlloc(" ",4, lf->code,op,"(float)",rt->code);
            else if (lf->type <= 0 || rt->type <= 0) {  // DYNAMIC
                if(lf->type >=0 && lf->type != INT_T && lf->type != FLOAT_T ||
                    lf->type >=0 && lf->type != INT_T && lf->type != FLOAT_T ){
                    ERRNO = ErrorTypeMisMatch;
                    errorInfo(ERRNO, node->line, "type mismatch for the operands of operator `%s'\n",op);
                    return ERRNO;
                }
                int flag = 0;
                if(lf->type >= 0) flag = codeAttr(lf);
                if(rt->type >= 0) flag = codeAttr(rt);
                if(!flag) {
                    SymbolTableEntry* e = tmpVab( DYN_ATTR_T, node->scope[1] );
                    frontDeclExp = strRightCatAlloc( frontDeclExp, "", 15,
                    INDENT[node->scope[0]],
                    "assign_operator_attr( &( ", e->bind,
                    " ) , binary_operator ( ", lf->code, " , ", rt->code, " , ", DynOP(token),
                    ", FLAG_NO_REVERSE",
                    ( (lf->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR" ),
                    ( (rt->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR" ),
                    " , ", strLine(node->line), " ) );\n"
                    );
                    node->code = strCatAlloc("", 1, e->bind);
                }
                //node->tmp[0] = REMOVE_DYN;
                node->type = DYNAMIC_T; 
            }
            else {
                ERRNO = ErrorTypeMisMatch;
                errorInfo(ERRNO, node->line, "type mismatch for the operands of operator `%s'\n",op);
                return ERRNO;
            }
            
            break;
/************************************************************************************/
        case ADD :          
        case SUB :          
        case MUL :          
        case DIV :          
            lf =  node->child[0]; rt = node->child[1];
            codeGen(lf);codeGen(rt);
            if(lf->type == rt->type && (lf->type == INT_T) || (lf->type == FLOAT_T ) ) {
                node->code = strCatAlloc(" ",3,lf->code,op,rt->code);
                node->type = lf->type;
            }
            else if (lf->type == INT_T && rt->type == FLOAT_T) {
                node->code = strCatAlloc(" ",4, "(float)",lf->code,op,rt->code);
                node->type = FLOAT_T;
            }
            else if (rt->type == INT_T && lf->type == FLOAT_T) {
                node->code = strCatAlloc(" ",4, lf->code,op,"(float)",rt->code);
                node->type = FLOAT_T;
            }
            else if (lf->type < 0 || rt->type < 0) { // DYNAMIC
#ifdef _DEBUG
                debugInfo("DYNAMIC : %d : (%d, %d) \n",
                    node->token, lf->type, rt->type );
#endif
                int flag = 0;
                if(lf->type>=0) flag = codeAttr(lf);    // if STATIC, wapper to Attr
                if(rt->type>=0) flag = codeAttr(rt);
                if (!flag) {
                    SymbolTableEntry* e = tmpVab( DYN_ATTR_T, node->scope[1] );
                    frontDeclExp = strRightCatAlloc( frontDeclExp, "", 15,
                        INDENT[node->scope[0]],
                        "assign_operator_attr( &( ", e->bind,
                        " ) , binary_operator( ", lf->code, " , ", rt->code, " , ", DynOP(token), 
                        ", FLAG_NO_REVERSE",
                        (lf->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR" ,
                        (rt->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR" ,
                        " , ", strLine(node->line), " ) );\n"
                    );
                    node->code = strCatAlloc("", 1, e->bind);
                    //node->tmp[0] = REMOVE_DYN;
                    node->type = DYNAMIC_T;
                }         
            }
            else {
                ERRNO = ErrorTypeMisMatch;
                errorInfo(ERRNO, node->line, "type mismatch for the operands of operator `%s'\n",op);
                return ERRNO;
            }
            break;
/************************************************************************************/
        case AST_CAST :
            lf =  node->child[0]; rt = node->child[1];
            int castType = lf->lexval.ival;
            codeGen(rt);
            if(rt->type >= 0) {
                if(castType == rt->type) {
                    node->code = strCatAlloc(" ",4,"(",sTypeName(lf->lexval.ival),")" , rt->code);
                    node->type = castType;
                }
                else if ( (castType == INT_T && rt->type == FLOAT_T) ||
                        (castType == FLOAT_T && rt->type == INT_T) ) {
                    node->code = strCatAlloc(" ",4,"(",sTypeName(lf->lexval.ival),")" , rt->code);
                    node->type = castType;
                }
                else {
                    ERRNO = ErrorCastType;
                    errorInfo(ERRNO, node->line, "cast from `%s' to `%s' is invalid\n", sTypeName(castType), sTypeName(rt->type) );
                    return ERRNO;
                }
            }
            else {  // DYNAMIC
                SymbolTableEntry* e = tmpVab( DYN_ATTR_T, node->scope[1] );
                frontDeclExp = strRightCatAlloc( frontDeclExp, "",11 ,
                    INDENT[node->scope[0]],
                    "assign_operator_attr( &( ", e->bind,
                    " ) , cast_operator( ", rt->code, " , ", typeMacro(castType),
                    (rt->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR",
                    " , ", strLine(node->line), " ) );\n"
                );
                node->code = strCatAlloc( "" , 1, e->bind);
                //node->tmp[0] = REMOVE_DYN;
                node->type = DYNAMIC_T;
            }
            break; 
/************************************************************************************/
        case AST_UNARY_PLUS :   
        case AST_UNARY_MINUS :  
        case AST_UNARY_NOT :    
            sg = node->child[0];
            codeGen(sg);
            if ( sg->type >= 0) {
                if ( (sg->type == INT_T || sg->type == FLOAT_T) &&
                    ( token == AST_UNARY_PLUS || token == AST_UNARY_MINUS) ) {
                    node->code = strCatAlloc(" ",4,op,"(",sg->code,")");
                    node->type = sg->type;
                }
                else if ( sg->type == BOOL_T && token == AST_UNARY_NOT ) {
                    node->code = strCatAlloc(" ",4,op,"(",sg->code,")");
                    node->type = sg->type;
                }
                else {
                    ERRNO = ErrorOperatorNotSupportedByType;
                    errorInfo(ERRNO, node->line, "unary operator `%s' is not supported by type `%s'.\n",op,sTypeName(sg->type));
					return ERRNO;
                }
            }
            else { // DYNAMIC
                SymbolTableEntry* e = tmpVab( DYN_ATTR_T, node->scope[1] );
                frontDeclExp = strRightCatAlloc( frontDeclExp, "",11,
                    INDENT[node->scope[0]],
                    "assign_operator_attr( &( ", e->bind,
                    " ) , unary_operator (", sg->code, " , ", DynOP(token),
                    (sg->tmp[0]==REMOVE_DYN) ? " , FLAG_DESTROY_ATTR" : " , FLAG_KEEP_ATTR",
                    " , ", strLine(node->line), " ) );\n"
                );
                node->code = strCatAlloc("", 1, e->bind);
                //node->tmp[0] = REMOVE_DYN;
                if(token==AST_UNARY_NOT) node->type = DYN_BOOL_T;
                else node->type = DYNAMIC_T;
            }
            break;
/************************************************************************************/
        case ARROW :
			lf = node->child[0]; sg = node->child[1]; rt = node->child[2];
			if(lf->token!=IDENTIFIER || sg->token!=IDENTIFIER || rt->token!=IDENTIFIER){
				ERRNO = ErrorEdgeAssignExpression;
				errorInfo(ERRNO, node->line, "edge assign expression error\n");
			}
			if(lf->type!=EDGE_T||sg->type!=VERTEX_T||rt->type!=VERTEX_T){
				ERRNO = ErrorEdgeAssignExpression;
				errorInfo(ERRNO, node->line, "edge assign illegal var type error\n");
			}
			codeGen(lf); codeGen(sg); codeGen(rt);
			node->code = strCatAlloc("",7,"edge_assign_direction(", lf->code, ", ", sg->code, ", ", rt->code, ")");
			node->codetmp = NULL;
            break;
/************************************************************************************/
        case AST_FUNC_CALL :
            //  lookup symbol table, also set type
            errflag = sTableLookupFunc(node);
            //  code Gen
            if(!errflag) {
                if (node->nch > 1){         //    if have args
                    int cnt = 0;            //    count number of args
                    codeFuncWrapDynArgs(node->child[1], node->symbol->typeCon, &cnt);
                    if (node->symbol->typeCon->len != cnt) {
                        ERRNO = ErrorFunctionCallNOTEqualNumberOfParameters;
                        errorInfo(ERRNO, node->line, "function Call has inconsistent number of arguments to its declaration. %d, %d\n", node->symbol->typeCon->len, cnt);
                    }
                }
                if(node->symbol->type == FUNC_LITERAL_T && inMATCH > 0) {
                    if(node->nch == 1)
                        node->code = strCatAlloc("",2,node->symbol->bind, " ( _obj, _obj_type )" );
                    else
                        node->code = strCatAlloc("",4,node->symbol->bind, " ( _obj, _obj_type, ",
                            node->child[1]->code, " )");
                }
                else if (node->symbol->type == FUNC_T) {
                    if(node->nch == 1) node->code = strCatAlloc(" ",2,node->symbol->bind,"()");
                    else node->code = strCatAlloc(" ",4,node->symbol->bind,"(", node->child[1]->code, " )");
                }
                else {
                    ERRNO = ErrorWrongFuncCall;
                    errorInfo(ERRNO,node->line,"invalid func call.\n");
                }
            }
            break;
        case AST_ARG_EXPS :
            codeGen(node->child[0]);
            node->type = node->child[0]->type;
            node->code = strCatAlloc(" ", 1, node->child[0]->code );
            break;
/************************************************************************************/
        case PIPE :{
			lf = node->child[0];
			rt = node->child[1];
			codeGen(lf);codeGen(rt);
			if(lf->type!=ELIST_T && lf->type!=VLIST_T){
				ERRNO = ErrorPipeWrongType;
				errorInfo(ERRNO, node->line, "pipe can NOT be operated on type `%s'.\n", sTypeName(lf->type));
			}
			existPIPE = 1;
            char* nltype = (lf->type == VLIST_T) ? typeMacro(EDGE_T) : typeMacro(VERTEX_T);
            SymbolTableEntry* enl = tmpVab( (lf->type == VLIST_T) ? ELIST_T : VLIST_T , node->scope[1] );
            SymbolTableEntry* elen = tmpVab( INT_T, node->scope[1] );
            SymbolTableEntry* ei = tmpVab( INT_T, node->scope[1] );
            char * cass = tmpVabAssign( enl, " new_list ()" );
            char * ident = INDENT[node->scope[0]];
			frontDeclExp = strRightCatAlloc( frontDeclExp,"", 29,
                    ident, "// START_PIPE\n",
                    ident, cass,
					ident, enl->bind, "->type = ", nltype, ";\n",
					ident, "int ", elen->bind, " = g_list_length(", lf->code, "->list);\n",
					ident, "int ", ei->bind, ";\n",
					ident,"for(", ei->bind, "=0; ", ei->bind, "<", elen->bind, "; ", ei->bind, "++){\n");
            if(lf->type == ELIST_T) {
                if (rt->token == STARTING_VERTICES) 
                    frontDeclExp = strRightCatAlloc( frontDeclExp, "",8 ,
                        enl->bind, " = list_append( ", enl->bind, ", VERTEX_T, ((EdgeType*)g_list_nth_data(", lf->code,"->list, ", ei->bind, "))->start);\n");
                else if (rt->token == ENDING_VERTICES)
                    frontDeclExp = strRightCatAlloc( frontDeclExp, "", 8,
                        enl->bind, " = list_append( ", enl->bind, ", VERTEX_T, ((EdgeType*)g_list_nth_data(", lf->code,"->list, ", ei->bind, "))->end);\n");
                else {
                    //
                }
            }
            else if (lf->type == VLIST_T) {
                if (rt->token == OUTCOMING_EDGES)
                    frontDeclExp = strRightCatAlloc( frontDeclExp, "",8 ,
                        enl->bind, " = list_append_gl(", enl->bind, ", ((VertexType*)g_list_nth_data(", lf->code, "->list, ", ei->bind, "))->outEdges, EDGE_T);\n");
                else if (rt->token == INCOMING_EDGES)
                    frontDeclExp = strRightCatAlloc( frontDeclExp, "",8 ,
                        enl->bind, " = list_append_gl(", enl->bind, ", ((VertexType*)g_list_nth_data(", lf->code, "->list, ", ei->bind, "))->inEdges, EDGE_T);\n");
                else {
                    //
                }
            }
            frontDeclExp = strRightCatAlloc( frontDeclExp,"",1 ,"} // END_PIPE\n");
            //if(lf->tmp[0]==REMOVE_DYN) {
            //    frontDeclExp = strRightCatAlloc(frontDeclExp,"",3,
            //        "destroy_list ( ", lf->code, ");\n"
            //    );
            //}
            node->code = strCatAlloc("",1,enl->bind);  
			if(lf->type == ELIST_T)
            	node->type = VLIST_T;
			else
				node->type = ELIST_T;
            //node->tmp[0] = REMOVE_DYN; 
			break;
	    }
/************************************************************************************/
        case AST_MATCH :
            lf = node->child[0];        // list
            rt = node->child[1];        // condition
            sg = node->child[2];        // scope_out
            codeGen(lf); 
            // get the STR name, func name, 
            char * tmpfunc = tmpMatch();
            char * match_str = tmpMatchStr();
            char * match_str_val = tmpMatchStrVab();
            // declaration of STR
            frontDeclExpTmp1 = frontDeclExp;            // store everything before match
            frontDeclExp = NULL;                        //    clear front code
            frontDeclExpTmp1 = strRightCatAlloc( frontDeclExpTmp1, "", 5,
                "struct ", match_str, " ", match_str_val, " = {"
            );
            nMATCHsVab = 0;
            inMATCH++; codeGen(rt); inMATCH--;
            frontDeclExpTmp1 = strRightCatAlloc( frontDeclExpTmp1,"",1,"};\n" );
            if(lf->type != VLIST_T && lf->type != ELIST_T) {
                ERRNO = ErrorMactchWrongType;
                errorInfo(ERRNO,node->line," match can NOT be operated on type `%s'.\n",sTypeName(lf->type) );
                return ERRNO;
            }
            //  check return type == bool
            if(rt->type != BOOL_T && rt->type >=0 ){
                ERRNO = ErrorInvalidReturnType;
                errorInfo(ERRNO,node->line,"the body of Match operator must return bool result.\n");
                return ERRNO;
            } 
            // set FLAG for STR declaration
            // FLAG cleared in AST_EXP_STAT
            existMATCH = 1;
            if(rt->type < 0) {  // if DYNAMIC, convert to BOOL_T
                char * ctmp = rt->code;
                rt->code = codeGetAttrVal( rt->code,  BOOL_T, node->line );
                free(ctmp);
            }
            // first generate struct and func for this match 
            char* func_body = codeFrontDecl( 1 );                           // get func body
            char* freecode = allFreeCodeInScope( sg->scope[1], NULL, 1 );
            char* initcode = allInitTmpVabCodeInScope( sg->scope[1], NULL, 1 );
            SymbolTableEntry* ert = tmpVab( BOOL_T, sg->scope[1] );
            node->codetmp = strCatAlloc("", 24,
                "struct ",match_str, " {\n",
                matchStaticVab,
                "};\n",
                "bool ", tmpfunc, 
                " ( void * _obj, int _obj_type, struct ", match_str, " * _str ) {\n",
                initcode,
                func_body,
                INDENT[1], "bool ", ert->bind, " = ", rt->code, ";\n",
                freecode,
                INDENT[1], "return ", ert->bind, ";\n",
                "} // END_MATCH_FUNC \n"
            );
            free(func_body); free(freecode);free(initcode);
            free(matchStaticVab); matchStaticVab =NULL;                     // clear str decl body
            frontDeclExp = frontDeclExpTmp1;                                // restore front code before match
            // 
            int ttype = ( lf->type == VLIST_T )? VERTEX_T: EDGE_T ;
            SymbolTableEntry* elt = tmpVab( VLIST_T, node->scope[1] );
            SymbolTableEntry* elen = tmpVab( INT_T, node->scope[1] );
            SymbolTableEntry* ei = tmpVab( INT_T, node->scope[1] );
            SymbolTableEntry* eb = tmpVab( BOOL_T, node->scope[1] );
            SymbolTableEntry* eobj = tmpVab( ttype, node->scope[1] );
            char * cass = tmpVabAssign( elt, " new_list ()" );
            char * ident = INDENT[node->scope[0]];
            char * cdel = tmpVabDel( eobj );
            frontDeclExp = strRightCatAlloc(frontDeclExp,"", 69,
                ident,"// START_MATCH\n",
                ident,cass,
                ident,elt->bind, "->type = (", lf->code, ")->type;\n",
                ident,"int ", elen->bind, " = g_list_length( (", lf->code, ")->list );\n",
                ident,"int ", ei->bind, ";\n",
                ident,"bool ", eb->bind, ";\n",
                ident,"for (", ei->bind, "=0; ", ei->bind, "<", elen->bind, "; ", ei->bind, "++) {\n",
                ident,assignFunc(ttype)," ( &( ", eobj->bind, " ), list_getelement ( ",
                lf->code, " , ", ei->bind, ") );\n",

                ident,"if ( ", eb->bind, " = ", tmpfunc, "( ",eobj->bind, ", ( ", lf->code, ")->type, &", match_str_val,") ) {\n",
                ident,elt->bind, " = list_append ( ", elt->bind, " , ",typeMacro(ttype)," , ",  eobj->bind, ");\n",
                ident,"}\n",
                ident,cdel,
                ident,"} // END_MATCH \n"
            );
            node->code = strCatAlloc("",1,elt->bind);  
            node->type = lf->type;
            break;
/************************************************************************************/
        case AST_LIST_MEMBER:
            lf = node->child[0];
            rt = node->child[1];
            codeGen(lf); codeGen(rt);
            if(lf->type != VLIST_T && lf->type != ELIST_T) {
                ERRNO = ErrorGetMemberForNotListType;
                errorInfo( ERRNO, node->line, "get member for not list type.\n");
                return ERRNO;
            }
            if(rt->type == INT_T){
                node->code = strCatAlloc( "", 6 ,
                    (lf->type == VLIST_T) ? "(VertexType *) " : "(EdgeType *) ",
                    "list_getelement ( ", lf->code, " , " ,rt->code, " )" );
            }
            else if (rt->type < 0) {  // DYNAMIC
                node->code = strCatAlloc( "" , 8, 
                    (lf->type == VLIST_T) ? "(VertexType *) " : "(EdgeType *) ",
                    "list_getelement ( ", lf->code,
                    ", get_attr_value_INT_T ( ", rt->code, " , ", strLine(node->line), " ) )");
            }
            node->type = (lf->type == VLIST_T) ? VERTEX_T : EDGE_T;
            break;
        case AST_LENGTH:
            sg = node->child[0];
            codeGen(sg);
            if(sg->type != VLIST_T && sg->type != ELIST_T) {
                ERRNO = ErrorGetLengthForTypeNotList;
                errorInfo( ERRNO, node->line, "get length for type not list.\n");
                return ERRNO;
            }
            node->code = strCatAlloc( "", 3,
                "g_list_length( ",sg->code,"->list )" );
            node->type = INT_T; 
            break;
/************************************************************************************/
        case AST_ATTRIBUTE : 
            if(inMATCH==0){
                node->child[0]->code = strCatAlloc("", 1, node->child[0]->symbol->bind);
            }
            else {
                node->child[0]->code = strCatAlloc("", 3,"_str->", node->child[0]->symbol->bind,"_match");
                matchStaticVab = strRightCatAlloc( matchStaticVab,"", 5,
                    INDENT[1], sTypeName(node->child[0]->symbol->type), 
                        " * ", node->child[0]->symbol->bind,"_match;\n");
                frontDeclExpTmp1 = strRightCatAlloc( frontDeclExpTmp1, "", 2,
                    (nMATCHsVab++==0) ? "" : " , ",
                    node->child[0]->symbol->bind);
            }
            node->child[1]->code = strCatAlloc("", 1, node->child[1]->lexval.sval); 
            if(node->child[0]->type == VERTEX_T ) 
                node->code = strCatAlloc("", 7, "vertex_get_attribute( ",
                    node->child[0]->code, " ,  \"", node->child[1]->code, "\", 0, ", strLine(node->line),")");
            else if(node->child[0]->type == EDGE_T )
                node->code = strCatAlloc("", 7, "edge_get_attribute( ",
                    node->child[0]->code, " ,  \"", node->child[1]->code, "\", 0, ", strLine(node->line), ")");
            else {
                ERRNO = ErrorGetAttrForWrongType;
                errorInfo(ERRNO, node->line, "Access attribute for type `%s'.\n",
                    sTypeName(node->child[0]->type) );
                node->code = NULL;
            }
            node->type = DYNAMIC_T;
            break;
/************************************************************************************/
        case AST_GRAPH_PROP :
			lf = node->child[0]; rt = node->child[1];
			codeGen(lf); codeGen(rt);
			if(lf->type != GRAPH_T){
				ERRNO = ErrorWrongArgmentType;
				errorInfo(ERRNO, node->line, "need a graph type for AllV and AllE operation, but type used is `%s'. \n",
						sTypeName(lf->type) );
				return;
			}
			switch(rt->token){
				case ALL_VERTICES:
					node->type = VLIST_T;
					node->code = strCatAlloc("", 3, "get_g_vlist(", lf->code, ")");
					break;
				case ALL_EDGES:
					node->type = ELIST_T;
					node->code = strCatAlloc("", 3, "get_g_elist(", lf->code, ")");
					break;
				default:
					ERRNO = ErrorOperatorNotSupportedByType;
					errorInfo(ERRNO, node->line, "Undifined Operation for graph \n");
					return;
			}
            break;
/************************************************************************************/
        case AST_COMP_STAT :            // compound_statement
        case AST_COMP_STAT_NO_SCOPE :
            if(node->nch == 0) { // empty
                node->code = strCatAlloc("",2,INDENT[node->scope[0]],"{} // EMPTY_COMP \n");
            }
            else {
                sg = node->child[0];
                codeGen(sg);
                char * freecode = NULL;
                char * initcode = NULL;
                if(token == AST_COMP_STAT) {  // GC
                    freecode = allFreeCodeInScope(node->child[1]->scope[1], NULL, node->child[1]->scope[0] );
                    initcode = allInitTmpVabCodeInScope( node->child[1]->scope[1], NULL, node->child[1]->scope[0] );
                    node->code = strCatAlloc("",7,INDENT[node->scope[0]],"{\n",initcode, 
                        node->child[0]->code,freecode,INDENT[node->scope[0]],"} // END_COMP\n");
                }
                else {
                    node->code = strCatAlloc("",5,INDENT[node->scope[0]],"// BEGIN_COMP_NO_SCOPE\n",
                        node->child[0]->code, INDENT[node->scope[0]],"// END_COMP_NO_SCOPE\n");
                }
            }
            break;
        case AST_STAT_LIST :
            lf = node->child[0]; rt = node->child[1];
            codeGen(lf); codeGen(rt);
            node->code = strCatAlloc("",2,lf->code,rt->code);
            break;
/************************************************************************************/
        case AST_EXP_STAT :             // expression_statement
            if(node->nch == 0)  { // empty
                node->code = strCatAlloc("",1,";\n");
            }
            else {
                codeGen(node->child[0]);
                node->code = codeFrontDecl(node->scope[0]);
                node->code = strRightCatAlloc(node->code,"",3,INDENT[node->scope[0]],node->child[0]->code, ";\n");
            }
            break;
/************************************************************************************/
        case AST_IF_STAT :              // selection_statement
            lf = node->child[0]; rt = node->child[1];            
            codeGen(lf); 
            node->code = codeFrontDecl(node->scope[0]);
            if(lf->type == BOOL_T) {
                codeGen(rt);
                node->code = strRightCatAlloc(node->code, "",7, 
                    INDENT[node->scope[0]],"if ( ", lf->code, " ){ \n", 
                    rt->code, 
                    INDENT[node->scope[0]]," }// END_IF \n");
            }
            else if (lf->type < 0) { // DYNAMIC
                //char * ctmp = tmpAttr();
                SymbolTableEntry* etmp = tmpVab( DYN_ATTR_T, node->scope[1] );
                char * cassign = tmpVabAssign( etmp, lf->code );
                codeGen(rt);
                node->code = strRightCatAlloc(node->code, "", 11,
                    INDENT[node->scope[0]],"// START_IF\n",
                    INDENT[node->scope[0]],cassign,
                    INDENT[node->scope[0]],"if ( ", codeGetAttrVal(etmp->bind, BOOL_T, node->line), " ) {\n",
                    rt->code,
                    INDENT[node->scope[0]],"}// END_IF\n"
                );
                free(cassign);
            }
            else {
                ERRNO = ErrorIfConditionNotBOOL;
                errorInfo(ERRNO, node->line, "condition in IF statement is NOT of type `bool'.\n"); 
                return ERRNO;
            }
            break;
        case AST_IFELSE_STAT :
            lf = node->child[0]; sg = node->child[1]; rt = node->child[2];
            codeGen(lf); 
            node->code = codeFrontDecl(node->scope[0]);
            if(lf->type == BOOL_T) {
                codeGen(sg); codeGen(rt);
                node->code = strRightCatAlloc(node->code, "",10,
                    INDENT[node->scope[0]],"if ( ", lf->code, " ){ \n",
                    sg->code, 
                    INDENT[node->scope[0]],"}\nelse{\n", rt->code, 
                    INDENT[node->scope[0]]," }// END_IF\n");
            }
            else if (lf->type < 0) { // DYNAMIC
                //char * ctmp = tmpAttr();
                SymbolTableEntry* etmp = tmpVab( DYN_ATTR_T, node->scope[1] );
                char * cassign = tmpVabAssign( etmp, lf->code );
                codeGen(sg); codeGen(rt);
                node->code = strRightCatAlloc(node->code, "", 14, 
                    INDENT[node->scope[0]],"// START_IF\n",
                    INDENT[node->scope[0]],cassign,
                    INDENT[node->scope[0]],"if ( ", codeGetAttrVal(etmp->bind, BOOL_T, node->line), " ) {\n",
                    sg->code, 
                    INDENT[node->scope[0]],"} else {\n", rt->code,
                    INDENT[node->scope[0]],"}// END_IF\n"
                );
                free(cassign);
            }
            else {
                ERRNO = ErrorIfConditionNotBOOL;
                errorInfo(ERRNO, node->line, "condition in IF statement is NOT of type `bool'.\n");
                return ERRNO;
            }
            break;
/************************************************************************************/
        case AST_WHILE : {               // iteration_statement
            lf = node->child[0]; rt = node->child[1];
            char * tmpcode;
            char * label = strCatAlloc("",1,gotolabel());
			frontDeclExpTmp1 = frontDeclExp; frontDeclExp = NULL;
            codeGen(lf); tmpcode = frontDeclExp;
			frontDeclExp = frontDeclExpTmp1; frontDeclExpTmp1 = NULL;
			node->code = codeFrontDecl(node->scope[0] );
            inLoop++;
            LoopBody = rt->child[0]; LoopGotoLabel = label;
            codeGen(rt); 
            LoopBody = NULL; LoopGotoLabel = NULL;
            inLoop--;

            if(lf->type>=0){
                node->code = strRightCatAlloc(node->code, "", 10, 
                    INDENT[node->scope[0]],"while ( ", lf->code, " ) {\n",
                    rt->code, INDENT[node->scope[0]], 
                    label,": {} ", INDENT[node->scope[0]],
                    "} //END_OF_WHILE\n");
            }
            else { // DYNAMIC
                //char * ctmp = tmpAttr();
                SymbolTableEntry* etmp = tmpVab( DYN_ATTR_T, node->scope[1] );
                char * cass = tmpVabAssign( etmp, lf->code );
                //char * cdel = tmpVabDel( etmp );
                node->code = strRightCatAlloc(node->code, "", 20,
					INDENT[node->scope[0]], tmpcode,
                    INDENT[node->scope[0]],"// START_OF_WHILE\n",
                    INDENT[node->scope[0]],cass,
                    INDENT[node->scope[0]],"while ( ", codeGetAttrVal(etmp->bind, BOOL_T,node->line),
                    " ) {\n", rt->code, 
                    INDENT[node->scope[0]],label,": {\n",
				   	INDENT[node->scope[0]],tmpcode,
                    INDENT[node->scope[0]],cass,
                    INDENT[node->scope[0]],"}\n}//END_OF_WHILE\n");
                free(label); label = NULL;
				free(cass); cass = NULL;
            }
		    free(tmpcode);tmpcode = NULL;
            free(label);label = NULL;
            break;
        }
        case AST_FOR : {
            struct Node *f1 = node->child[0],
                        *f2 = node->child[1],
                        *f3 = node->child[2],
                        *fs = node->child[3];
			char * cf1 = NULL, *cf2 = NULL, *cf3 = NULL;
            char * label = strCatAlloc("",1,gotolabel());
			frontDeclExpTmp1 = frontDeclExp; frontDeclExp = NULL;
            codeGen(f1); cf1 = frontDeclExp; frontDeclExp = NULL;
			codeGen(f2); cf2 = frontDeclExp; frontDeclExp = NULL;
			codeGen(f3); cf3 = frontDeclExp; frontDeclExp = NULL;
			frontDeclExp = frontDeclExpTmp1; frontDeclExpTmp1 = NULL;
            node->code = codeFrontDecl(node->scope[0] );
            inLoop++; LoopBody = fs->child[0]; LoopGotoLabel = label;
            codeGen(fs); 
            LoopBody = NULL; inLoop--; LoopGotoLabel = NULL;
            if (f1->type>=0 && f2->type>=0 && f3->type>=0){
                node->code = strRightCatAlloc(node->code, "",13, INDENT[node->scope[0]],
                    "for (", (f1!=NULL)? f1->code : "", ";", 
                             (f2!=NULL)? f2->code : "", ";", 
                             (f3!=NULL)? f3->code : "", ") {\n",
                             fs->code, 
                            INDENT[node->scope[0]],label,":{}\n",
                            "} //END_OF_FOR\n");
            }
            else {  // DYNAMIC :: translate for to while
                //char * ctmp = tmpAttr();
                SymbolTableEntry* etmp = tmpVab( DYN_ATTR_T, node->scope[1] );
				char * cass = tmpVabAssign(etmp, f2->code);
                node->code = strRightCatAlloc(node->code,"", 28,
					INDENT[node->scope[0]], cf1, "\n", cf2, "\n",
                    INDENT[node->scope[0]],"// START_OF_FOR\n",
                    INDENT[node->scope[0]], cass,
                    INDENT[node->scope[0]],"while (", codeGetAttrVal(etmp->bind, BOOL_T,node->line),
                    " ) {\n", fs->code,
                    INDENT[node->scope[0]], label,": {\n",
                    INDENT[node->scope[0]], cf3, "\n",
                    INDENT[node->scope[0]], cf2, "\n",
                    INDENT[node->scope[0]], cass, ";\n",
                    "}\n} \n",
					"//END_OF_FOR\n"
                );
				free(cf1);cf1=NULL;
				free(cf2);cf2=NULL;
				free(cf3);cf3=NULL;
				free(cass);cass=NULL;
            }
            free(label);
            break;
        }
        case AST_FOREACH :{
			lf = node->child[0]; sg = node->child[1]; rt = node->child[2];
            // break or continue is forbidden 
			codeGen(lf); codeGen(sg); codeGen(rt);
			int ltype = lf->child[1]->type, rtype = sg->type;
			if( ltype==VERTEX_T&&rtype==VLIST_T || ltype==EDGE_T&&rtype==ELIST_T ){
				char* ti = lf->child[1]->symbol->bind;
				char* tlen = strCatAlloc("", 1, tmpVab(INT_T, node->scope[1]));
				char* tc = strCatAlloc("", 1, tmpVab(INT_T, node->scope[1]));
            	node->code = codeFrontDecl(node->scope[0] );
				node->code = strRightCatAlloc(node->code, "" , 25,
					INDENT[node->scope[0]], sTypeName(ltype), " * ", ti, " = NULL;\n",
					INDENT[node->scope[0]], "int ", tlen, " = g_list_length(", sg->code, "->list);\n",
					INDENT[node->scope[0]], "int ", tc, ";\n",
					INDENT[node->scope[0]], "for (", tc, "=0; ", tc, "<", tlen, "; ", tc, "++) {\n");
				if(ltype == VERTEX_T)
					node->code = strRightCatAlloc(node->code, "", 8,
						INDENT[node->scope[0]], "assign_operator_vertex(&", ti, ", g_list_nth_data ( ", sg->code, "->list, ", tc, " ) );\n");
				else
					node->code = strRightCatAlloc(node->code, "", 8,
						INDENT[node->scope[0]], "assign_operator_edge(&", ti, ", g_list_nth_data ( ", sg->code, "->list, ", tc, " ) );\n");
				node->code = strRightCatAlloc(node->code, "", 3, 
						//INDENT[node->scope[0]], ti, " = g_list_nth_data ( ", sg->code, "->list, ", tc, " );\n",
						rt->code,
				    	INDENT[node->scope[0]], "} //END_OF_FOREACH\n");
				free(tlen);free(tc);
				/*if(sg->tmp[0]==REMOVE_DYN) {
					node->code = strRightCatAlloc(node->code,"", 5,
						INDENT[node->scope[0]], codeFreeFuncName(VLIST_T), " ( ", sg->code, " );\n");
				}*/
			}
			else {
				ERRNO = ErrorForeachType;
				errorInfo(ERRNO, node->line, "foreach has wrong type\n");
				return ERRNO;
			}

			break;
		}
/************************************************************************************/
        case AST_JUMP_BREAK :           // jump_statement
            if(inLoop==0) {
                ERRNO = ErrorCallBreakOutsideOfLoop;
                errorInfo(ERRNO, node->line, "call `break' outside of loop\n");   
                return ERRNO;
            } else {
                char * freecode = NULL, * bkcode = NULL;
                // get all scope ids from the Loopbody to self
                int found = 0;
                GList * allscope = getAllScopeIdInside(LoopBody, NULL, node, &found);
                if (found == 0) {
                    fprintf(stderr, "coding wrong for getAllScopeIdInside !!!!!\n");
                }
                // free code for GC
                int tl = g_list_length ( allscope );
                int i;
                for ( i=0; i<tl; i++ ) {
                    int * pi = g_list_nth_data ( allscope, i );
                    char * tcode = allFreeCodeInScope( *pi, NULL, node->scope[0] );
                    freecode = strRightCatAlloc( freecode, "", 1, tcode );
                    free(tcode);
                }
                g_list_free( allscope );
                // break code
                bkcode = strCatAlloc("", 2,INDENT[node->scope[0]], "break ;\n");
                // all
                node->code = strCatAlloc("",2 ,freecode, bkcode);
                free(freecode);
                free(bkcode);
            }
            break;
        case AST_JUMP_CONTINUE : {
            if(inLoop==0) {
                ERRNO = ErrorCallContinueOutsideOfLoop;
                errorInfo(ERRNO, node->line, "call `continue' outside of loop\n");
                return ERRNO;
            } else {
                char * freecode = NULL, * ctcode = NULL;
                // get all scope ids from the Loopbody to self
                int found = 0;
                GList * allscope = getAllScopeIdInside(LoopBody, NULL, node, &found);
                if (found == 0) {
                    fprintf(stderr, "coding wrong for getAllScopeIdInside !!!!!\n");
                }
                // free code for GC
                int tl = g_list_length ( allscope );
                int i;
                for ( i=0; i<tl; i++ ) {
                    int * pi = g_list_nth_data ( allscope, i );
                    char * tcode = allFreeCodeInScope( *pi, NULL, node->scope[0] );
                    freecode = strRightCatAlloc( freecode, "", 1, tcode );
                    free(tcode);
                }
                g_list_free( allscope );
                // continue code
                ctcode = strCatAlloc("", 4 , INDENT[node->scope[0]], "goto ", LoopGotoLabel,";\n");
                node->code = strCatAlloc("", 2,freecode, ctcode);
                free(ctcode);
                free(freecode);
            }
            break;
        }
        case AST_JUMP_RETURN : { 
            if(inFunc<0) {
                ERRNO = ErrorCallReturnOutsideOfFunc;
                errorInfo(ERRNO, node->line, "call `return' outside of function or function literal\n");
                return ERRNO;
            }
            else {
                int rtype = * (int *) g_list_nth_data ( returnList, inFunc );   // obtain return type
                (* (int *) g_list_nth_data ( noReturn, inFunc ) ) ++ ;          // count number of returns
                char * freecode = NULL, * rtcode = NULL;
                int iptr = (rtype == VLIST_T || rtype == ELIST_T || rtype == VERTEX_T || rtype == EDGE_T || rtype == GRAPH_T || rtype == STRING_T) ? 1 : 0;
                // type checking and return code
                if (node->nch == 0) {
                    if (rtype != VOID_T) {
                        ERRNO = ErrorInvalidReturnType;
                        errorInfo(ERRNO, node->line, "invalid return type.\n");
                        return ERRNO;
                    }
                    rtcode = strCatAlloc("", 2,INDENT[node->scope[0]], "return ;\n");
                } 
                else {
                    codeGen(node->child[0]);
                    node->code = codeFrontDecl( node->scope[0]);                    // collect front code
                    rtcode = codeFrontDecl(node->scope[0] );
                    if (rtype != node->child[0]->type && node->child[0]->type >= 0) {
                        ERRNO = ErrorInvalidReturnType;
                        errorInfo(ERRNO, node->line, "invalid return type.\n");
                        return ERRNO;
                    }
                    else if (rtype == node->child[0]->type && node->child[0]->type >= 0) {
                        char * tmp = tmpReturnTmp();
                        node->code = strRightCatAlloc(node->code, "", 7,
                            INDENT[node->scope[0]], sTypeName(rtype), iptr ? " * ":" ", tmp, " = ",
                                node->child[0]->code,";\n");
                        rtcode = strCatAlloc("",4, INDENT[node->scope[0]], "return ", tmp, ";\n");
                    }
                    else {
                        char * tmp = tmpReturnTmp();
                        node->code = strRightCatAlloc(node->code, "", 7,
                            INDENT[node->scope[0]], sTypeName(rtype), iptr ? " * ":" ", tmp, " = ",
                            codeGetAttrVal( node->child[0]->code, rtype, node->line ),
                            ";\n");
                        rtcode = strCatAlloc("",4, INDENT[node->scope[0]],"return ", tmp, ";\n");
                    }
                }
                // get all scope ids from the funcbody to this return
                int found = 0;
                GList * allscope = getAllScopeIdInside(FuncBody, NULL, node, &found);
                if (found == 0) {
                    fprintf(stderr, "coding wrong for getAllScopeIdInside !!!!!\n");
                }
                // freecode for GC
                int tl = g_list_length ( allscope );
                int i;
                for ( i=0; i<tl; i++ ) {
                    int * pi = g_list_nth_data ( allscope, i );
                    char * tcode = allFreeCodeInScope( *pi, FuncParaList, node->scope[0] );
                    freecode = strRightCatAlloc( freecode, "", 1, tcode );
                    free(tcode);
                }
                g_list_free( allscope );
                node->code = strRightCatAlloc( node->code, "", 2, freecode, rtcode ); 
            }
            break;
        // 1> break continue is in scope of loop    // DONE
        // 2> return is in scope of func, and return type is correct //DONE
        } 

/************************************************************************************/
        case AST_FUNC : {                // function_definition
            lf = node->child[0];            // return type
            sg = node->child[1];            // parameter_list
            rt = node->child[2];            // compound_statement
            codeGen(sg); 
            int zero = 0, nort;
            inFunc++; 
            // set para list to global
            FuncParaList = getAllParaInFunc(sg->child[1], NULL);
            // get all scope ids in the body of func
            FuncBody = rt;
            // get return type and number returns
            returnList = g_list_append(returnList, (gpointer) & (lf->lexval.ival) ); 
            noReturn = g_list_append( noReturn, (gpointer) &zero);
            codeGen(rt); 
            returnList = g_list_remove(returnList, g_list_nth_data(returnList, inFunc) );
            gpointer gp = g_list_nth_data( noReturn, inFunc );
            nort = *(int *) gp;
            noReturn = g_list_remove( noReturn, gp );
            // clean lists
            g_list_free( FuncParaList );
            FuncBody = NULL;
            inFunc--;
            if ( nort <= 0 && lf->lexval.ival != VOID_T ) {
                ERRNO = ErrorNoReturnInFunc;
                errorInfo(ERRNO, node->line, "missing return in function declaration.\n");
                return ERRNO;
            } 
            char * initcode = allInitTmpVabCodeInScope( rt->scope[1], NULL, 1 );
            int flag0 = 0;
            int type0 = lf->lexval.ival;
            if (type0 == VERTEX_T || type0 == EDGE_T || type0 == VLIST_T ||
                    type0 == ELIST_T ||type0 == STRING_T || type0 == GRAPH_T) flag0 = 1;
            node->code = strCatAlloc("",10,
                    sTypeName(lf->lexval.ival),(flag0)? " * ":" ",
                    node->symbol->bind,     // func_id
                    " ( ", sg->code," ) ", "{\n",
                    initcode,
                    rt->code, "}\n");
            free(initcode);
            //showASTandST(node,"Function Definition");
            break;
        }
        case FUNC_LITERAL :  {           // function_literal_declaration
            lf = node->child[1];            // return type
            sg = node->child[0];            // parameter_list
            rt = node->child[2];            // compound_statement
            codeGen(sg); 
            int zero = 0, nort;
            inFunc++;
            // set para list to global
            FuncParaList = getAllParaInFunc(sg->child[1], NULL);
            // set body pointer to global (used in AST_RETURN)
            FuncBody = rt;
            // get return type and number returns
            returnList = g_list_append(returnList, (gpointer) & (lf->lexval.ival) );
            noReturn = g_list_append( noReturn, (gpointer) &zero); 
            codeGen(rt); 
            returnList = g_list_remove(returnList, g_list_nth_data(returnList, inFunc) );
            gpointer gp = g_list_nth_data( noReturn, inFunc );
            nort = *(int *) gp;
            noReturn = g_list_remove( noReturn, gp );
            // clean lists
            g_list_free( FuncParaList );
            FuncBody = NULL;
            inFunc--;
            if ( nort <= 0 && lf->lexval.ival != VOID_T ) {
                ERRNO = ErrorNoReturnInFunc;
                errorInfo(ERRNO, node->line, "missing return in function literal declaration.\n");
                return ERRNO;
            }
            char * initcode = allInitTmpVabCodeInScope( rt->scope[1], NULL, 1 );
            int flag0 = 0;
            int type0 = lf->lexval.ival;
            if (type0 == VERTEX_T || type0 == EDGE_T || type0 == VLIST_T ||
                    type0 == ELIST_T ||type0 == STRING_T || type0 == GRAPH_T) flag0 = 1;
            //    put code to node->codetmp, as we need put all func_literals in
            // the external in target code.
            node->code = NULL;
            node->codetmp = strCatAlloc("", 11,
                    sTypeName(lf->lexval.ival),(flag0)? " * " : " ",
                    node->symbol->bind,  // func_id
                    " ( void * _obj, int _obj_type",
                    (sg->nch==1) ? "" : " , ",
                     sg->code, " ) ", "{\n",
                    initcode,
                    rt->code, "}  // END_OF_FUNC_LITERAL\n");
            free(initcode);
            break;
        }
/************************************************************************************/
        case AST_FUNC_DECLARATOR :
            // here only create parameter list
            if(node->nch==1) // empty list
                node->code = strCatAlloc("",1,"");
            else {
                sg = node->child[1];
                codeGen(sg);
                node->code = strCatAlloc("",1,sg->code);
            }
            break;
        case AST_PARA_DECLARATION :
            lf = node->child[0];            // declaration_specifiers
            rt = node->child[1];            // IDENTIFIER or attribute
            codeGen(rt);
            node->type = node->child[0]->lexval.ival;
            if (node->type == STRING_T || node->type == VLIST_T || node->type == ELIST_T ||
                node->type == VERTEX_T || node->type == EDGE_T || node->type == GRAPH_T)
                    node->code = strCatAlloc("",3,sTypeName(lf->lexval.ival)," * ", node->child[1]->code);
            else if (node->type == BOOL_T || node->type == INT_T || node->type == FLOAT_T)
                node->code = strCatAlloc("", 3, sTypeName(lf->lexval.ival)," ", node->child[1]->code );
            else {
                ERRNO = ErrorWrongArgmentType;
                errorInfo(ERRNO, node->line, "invalid argument type.\n");
                return ERRNO;
            }    
            break;
/************************************************************************************/
	    case AST_PRINT_STAT :
            codeGen(node->child[0]);
            node->code = codeFrontDecl(node->scope[0]);
            node->code = strRightCatAlloc(node->code,"", 1,  node->child[0]->code);
            break;
        case AST_PRINT_COMMA :
            codeGen(node->child[0]);
            codeGen(node->child[1]);
            node->code = strCatAlloc("", 2, node->child[0]->code, node->child[1]->code);
            break;
	    case AST_PRINT : {
            codeGen(node->child[0]);
            int tt = node->child[0]->type;
            if ( tt >= 0 ) {
                node->code = strCatAlloc("", 6,
                    INDENT[node->scope[0]], "print_", typeMacro(tt), " ( ",
                        node->child[0]->code, " );\n");
            }
            else if ( tt < 0 ) {
                node->code = strCatAlloc("", 4,
                    INDENT[node->scope[0]], "print_attr ( ", 
                        node->child[0]->code, " );\n");
            }
            break;
	    }
/************************************************************************************/
	    case AST_READ_GRAPH:        // FILE >> Graph
		    lf=node->child[0];
		    rt=node->child[1];
            codeGen(lf); codeGen(rt);
            if ( lf->type == STRING_T && rt->type == GRAPH_T ) {
                char * tg = tmpGraphVab();
                node->code = strCatAlloc("", 10,
                    INDENT[node->scope[0]], codeFreeFuncName(GRAPH_T), "( ", rt->code, " );\n",
                    INDENT[node->scope[0]], rt->code, " = readGraph( ", lf->code, "->str );\n"
                ); 
            }
            else {
                ERRNO = ErrorTypeMisMatch;
                errorInfo(ERRNO, node->line, "expected `FILE:STRING' to be fetched from `GRAPH' file location.\n" );	
            }
		    break;
	    case AST_WRITE_GRAPH:       // FILE << Graph
            lf=node->child[0];
            rt=node->child[1];
            codeGen(lf); codeGen(rt);
            if ( lf->type == STRING_T && rt->type == GRAPH_T ) {
                node->code = strCatAlloc("", 6,
                    INDENT[node->scope[0]], "saveGraph( ", rt->code, " , (", lf->code,")->str );\n"
                );
            }
            else {
                ERRNO = ErrorTypeMisMatch;
                errorInfo(ERRNO, node->line, "expected `FILE:STRING' to be written into `GRAPH' file location.\n");
                        
            }
            break;		
/************************************************************************************/
        default:
            if(node->code == NULL) {
#ifdef _DEBUG
                debugInfo("Warning: No code generated on ");
                astOutNode(node, DEBUGIO,"\n");
#endif
            }
    }
    return 0;
}

int codeAllGen(struct Node* node, char ** mainCode, char ** funCode) {
    if(node->token == AST_EXT_STAT_COMMA) {
        codeAllGen(node->child[0], mainCode, funCode);
        codeAllGen(node->child[1], mainCode, funCode);
    }
    else if(node->token == AST_FUNC) {  // merge in funCode
        codeGen(node);
        *funCode = strRightCatAlloc( *funCode, "", 2, node->code,"\n" );
    }
    else { // merge in mainCode
        codeGen(node);
        *mainCode = strRightCatAlloc( *mainCode, "", 1, node->code );
    }

    return 0;
}

void codeAllFuncLiteral(struct Node* node, char ** code) {
    // travel the entire tree, get all func_literals
    if (node==NULL) return;
    if (node->token == FUNC_LITERAL ||
            node->token == AST_MATCH ) {
        *code = strRightCatAlloc( *code, "", 2, node->codetmp, "\n");
        // DO NOT return, for nested Func_Literal
        //if (node->token == FUNC_LITERAL) return;
    }
    int i;
    for (i=0; i<node->nch; ++i) {
        codeAllFuncLiteral(node->child[i], code);
    }
    return;
}

void codeInclude(char ** code) {
    *code = strRightCatAlloc( *code, "" ,1,
        "#include \"nsbl.h\"\n");
}

void codeAllGlobal(struct Node* node, char ** code) {
    // travel the entire tree, get all declaration in scope level 0
    if (node==NULL) return;
    if (node->token == AST_DECLARATION) {
        if (node->scope[0] == 0)
            *code = strRightCatAlloc( *code, "", 1, node->codetmp);
        return;
    }
    int i;
    for (i=0; i<node->nch; ++i) {
        codeAllGlobal(node->child[i], code);
    }
    return;
}

char * wapperMainCode(char * mainBodyCode){
    char * head = "int main() {\n\n";
    char * GC1 = "gcInit();\n";
    char * initcode = allInitTmpVabCodeInScope( 0, NULL, 0);
    char * freecode = allFreeCodeInScope( 0, NULL, 0 );
#ifdef _DEBUG
    //debugInfo("MainFreeCode:\n");
    //debugInfoExt("%s",freecode);
#endif
    char * GC2 = "gcDel();\n";
    char * end = "\n} // END_OF_MAIN \n";
    return strCatAlloc("",7,head,GC1,initcode, mainBodyCode, freecode, GC2, end);
}

void exportCode(char * code){
    fprintf(OUTFILESTREAM,"%s",code);
}


