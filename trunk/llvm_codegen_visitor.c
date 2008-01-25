#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "llvm_codegen_visitor.h"

static int stack_size = -1;

static int _get_type_size(Type type);
static void _print_load(struct AstNode *node, Visitor *visitor);
static void _print_boolean(struct AstNode *node);

Visitor *
llvm_codegen_new()
{
    Visitor *visitor = (Visitor *) malloc (sizeof(Visitor));

    visitor->visit_program = &llvm_codegen_visit_program;
    visitor->visit_programdecl = &llvm_codegen_visit_programdecl;
    visitor->visit_vardecl_list = &llvm_codegen_visit_vardecl_list;
    visitor->visit_vardecl = &llvm_codegen_visit_vardecl;
    visitor->visit_identifier_list = &llvm_codegen_visit_identifier_list;
    visitor->visit_procfunc_list = &llvm_codegen_visit_procfunc_list;
    visitor->visit_procedure = &llvm_codegen_visit_procfunc;
    visitor->visit_function = &llvm_codegen_visit_procfunc;
    visitor->visit_param_list = &llvm_codegen_visit_param_list;
    visitor->visit_parameter = &llvm_codegen_visit_parameter;
    visitor->visit_statement_list = &llvm_codegen_visit_statement_list;
    visitor->visit_printint_stmt = &llvm_codegen_visit_printint_stmt;
    visitor->visit_printchar_stmt = &llvm_codegen_visit_printchar_stmt;
    visitor->visit_printbool_stmt = &llvm_codegen_visit_printbool_stmt;
    visitor->visit_printline_stmt = &llvm_codegen_visit_printline_stmt;
    visitor->visit_assignment_stmt = &llvm_codegen_visit_assignment_stmt;
    visitor->visit_if_stmt = &llvm_codegen_visit_if_stmt;
    visitor->visit_while_stmt = &llvm_codegen_visit_while_stmt;
    visitor->visit_for_stmt = &llvm_codegen_visit_for_stmt;
    visitor->visit_rel_expr = &llvm_codegen_visit_binary_expr;
    visitor->visit_add_expr = &llvm_codegen_visit_binary_expr;
    visitor->visit_mul_expr = &llvm_codegen_visit_binary_expr;
    visitor->visit_notfactor = &llvm_codegen_visit_notfactor;
    visitor->visit_call = &llvm_codegen_visit_call;
    visitor->visit_callparam_list = &llvm_codegen_visit_callparam_list;
    visitor->visit_identifier = &llvm_codegen_visit_identifier;
    visitor->visit_literal = &llvm_codegen_visit_literal;
    visitor->visit_add_op = &llvm_codegen_visit_binary_op;
    visitor->visit_mul_op = &llvm_codegen_visit_binary_op;
    visitor->visit_rel_op = &llvm_codegen_visit_binary_op;
    visitor->visit_not_op = &llvm_codegen_visit_not_op;

    return visitor;
}

void
llvm_codegen_visit_program(struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;

    printf("; Generated with toypasc\n");
    for (child = node->children;
         child != NULL && child->kind != STATEMENT_LIST;
         child = child->sibling) {
        ast_node_accept(child, visitor);
    }

    if (child != NULL) {
        printf("; Definition of main function\n");
        printf("define i32 @main () {\nentry:\n");
        ast_node_accept(child, visitor);
        printf(TAB"ret i32 0\n}\n\n");
    }
}

void
llvm_codegen_visit_programdecl(struct _Visitor *visitor, struct AstNode *node)
{
    printf("; program ");
    ast_node_accept(node->children, visitor);
    printf("\n\n");
    printf("; Declare the string constants as a global constants...\n");
    printf("@.true_str = internal constant [5 x i8] c\"true\\00\"\n");
    printf("@.false_str = internal constant [6 x i8] c\"false\\00\"\n");
    printf("@.int_fmt = internal constant [3 x i8] c\"%%d\\00\"\n");

    printf("\n; External declaration of functions\n");
    printf("declare i32 @puts(i8 *)\n");
    printf("declare i32 @putchar(i32)\n");
    printf("declare i32 @printf(i8*, ...)\n\n");
}

void
llvm_codegen_visit_vardecl_list (struct _Visitor *visitor, struct AstNode *node)
{
    ast_node_accept_children(node->children, visitor);
    if (node->parent->kind == PROGRAM)
        printf("\n");
}

void
llvm_codegen_visit_identifier_list (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;

    // FIXME: porque nunca entra aqui?
    printf("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
    for (child = node->children; child != NULL; child = child->sibling) {
        ast_node_accept(child, visitor);
        if (child->sibling != NULL)
            printf(", ");
    }
}

void
llvm_codegen_visit_procfunc_list (struct _Visitor *visitor, struct AstNode *node)
{
    ast_node_accept_children(node->children, visitor);
}

void
llvm_codegen_visit_procfunc (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;

    printf("define ");
    PRINT_TYPE(node->type);
    printf(" ");

    child = node->children; // Identifier
    ast_node_accept(child, visitor);

    printf(" ( ");

    child = child->sibling;
    if (child->kind == PARAM_LIST) {
        ast_node_accept(child, visitor);
        child = child->sibling;
    }

    printf(" ) {\n");
    printf("entry:\n");

    if (child->kind == VARDECL_LIST) {
        ast_node_accept(child, visitor);
        child = child->sibling;
    }

    ast_node_accept(child, visitor);

    printf(TAB"ret ");
    PRINT_TYPE(node->type);

    if (node->kind == FUNCTION)
        printf(" %%%d", node->children->symbol->stack_index);

    printf("\n}\n\n");
}

void
llvm_codegen_visit_param_list (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;

    for (child = node->children; child != NULL; child = child->sibling) {
        ast_node_accept(child, visitor);
        if (child->sibling != NULL)
            printf(", ");
    }
}

void
llvm_codegen_visit_statement_list (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;

    stack_size = -1;

    for (child = node->children; child != NULL; child = child->sibling) {
        ast_node_accept(child, visitor);
        printf("\n");
    }
}

void
llvm_codegen_visit_binary_expr (struct _Visitor *visitor, struct AstNode *node)
{
    int lindex = -1;
    int rindex = -1;
    struct AstNode *lnode = node->children;
    struct AstNode *op = lnode->sibling;
    struct AstNode *rnode = op->sibling;

    /* FIXME */ //printf("; [BinaryExpr] (%d)\n", stack_size);
    int __process_binexpr_node(struct AstNode *node) {
        if (node->kind == IDENTIFIER) {
            if (node->symbol->is_global) {
                if (node->symbol->is_procfunc)
                    return node->symbol->stack_index;

                _print_load(node, visitor);
                return stack_size;
            }

        } else if (!IS_LITERAL(node->kind)) {
            ast_node_accept(node, visitor);
            return stack_size;

        }
        return -1;
    }

    void __print_operand(struct AstNode *node, int index) {
        if (index > -1)
            printf("%%%d", index);
        else if (node->symbol != NULL && node->symbol->is_procfunc)
            printf("0");
        else
            ast_node_accept(node, visitor);
    }

    /* Construcao mais simples */
    if (IS_LITERAL(lnode->kind) && IS_LITERAL(rnode->kind)) {
        printf(TAB);
        ast_node_accept(op, visitor);
        printf(" ");
        PRINT_TYPE(node->type);
        printf(" ");
        ast_node_accept(lnode, visitor);
        printf(", ");
        ast_node_accept(rnode, visitor);
        printf("\n");

    /* Construcoes complexas */
    } else {
        lindex = __process_binexpr_node(lnode);
        rindex = __process_binexpr_node(rnode);

        printf(TAB);
        ast_node_accept(op, visitor);
        printf(" ");
        PRINT_TYPE(node->type);
        printf(" ");

        __print_operand(lnode, lindex);
        printf(", ");
        __print_operand(rnode, rindex);

        printf("\n");
    }

    stack_size++;
    /* FIXME */ //printf("; [BinaryExpr] (%d)\n", stack_size);
}

void
llvm_codegen_visit_callparam_list (struct _Visitor *visitor, struct AstNode *node)
{
    ast_node_accept(node->children, visitor);
}

void
llvm_codegen_visit_identifier (struct _Visitor *visitor, struct AstNode *node)
{
    Symbol *sym = node->symbol;

    /* WARNING: o if abaixo esta 'awkward' */

    if (sym->is_global || node->parent->kind == CALL)
        printf("@%s", sym->name);

    else if (!sym->is_global && sym->is_parameter &&
             sym->stack_index == -1)
        printf("%%%s", sym->name);

    else if (sym->stack_index > -1)
        printf("%%%d", sym->stack_index);

    else
        printf("0");
}

void
llvm_codegen_visit_literal (struct _Visitor *visitor, struct AstNode *node)
{
    printf("%d", node->value.integer);
}

void
llvm_codegen_visit_vardecl (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;

    child = node->children;

    for (child = child->children; child != NULL; child = child->sibling) {
        if (node->parent->parent->kind == PROGRAM) {
            ast_node_accept(child, visitor);
            printf(" = global i%d 0\n", _get_type_size(child->type));
        } else {
            child->symbol->stack_index = -1;
        }
    }
}

void
llvm_codegen_visit_parameter (struct _Visitor *visitor, struct AstNode *node)
{
    PRINT_TYPE(node->type);
    printf(" ");
    ast_node_accept(node->children, visitor);
}

void
llvm_codegen_visit_printint_stmt (struct _Visitor *visitor, struct AstNode *node)
{
    int index = -1;
    struct AstNode *child = node->children;
    bool is_literal = IS_LITERAL(child->kind);

    if (child->kind == IDENTIFIER) {
        if (child->symbol->is_global &&
            !child->symbol->is_procfunc) {
            _print_load(child, visitor);
            index = stack_size;

        } else
            index = child->symbol->stack_index;

    } else if (!is_literal) {
        ast_node_accept(child, visitor);
        index = stack_size;
    }

    printf(TAB"call i32 (i8* noalias , ...)* bitcast (i32 (i8*, ...)* \n");
    printf(TAB TAB"@printf to i32 (i8* noalias, ...)*)\n");
    printf(TAB TAB"( i8* getelementptr ");
    printf("([3 x i8]* @.int_fmt, i32 0, i32 0) noalias ,\n");
    printf(TAB TAB"i32 ");
    if (is_literal)
        ast_node_accept(child, visitor);
    else
        printf("%%%d", index);
    printf(" )\n");
    stack_size++;
}

void
llvm_codegen_visit_printchar_stmt (struct _Visitor *visitor, struct AstNode *node)
{
    printf(TAB"call i32 @putchar( i32 ");
    ast_node_accept(node->children, visitor);
    printf(" )\n");
    stack_size++;
}

void
llvm_codegen_visit_printbool_stmt (struct _Visitor *visitor, struct AstNode *node)
{
    ast_node_accept(node->children, visitor);
    printf(TAB"call i32 @puts( i8 * %bool_str )\n");
    stack_size++;
}

void
llvm_codegen_visit_printline_stmt (struct _Visitor *visitor, struct AstNode *node)
{
    printf(TAB"call i32 @putchar( i32 10 )\n");
    stack_size++;
}

void
llvm_codegen_visit_assignment_stmt (struct _Visitor *visitor, struct AstNode *node)
{
    int rindex = -1;
    struct AstNode *lnode = node->children;
    struct AstNode *rnode = lnode->sibling;

    void __print_rvalue(struct AstNode *node) {
        if (rindex > -1)
            printf(" %%%d, ", rindex);
        else if (node->kind == IDENTIFIER)
            printf(" %%%d, ", node->symbol->stack_index);
        else if (IS_LITERAL(node->kind))
            printf(" %d, ", node->value.integer);
        else
            printf(" %%%d, ", stack_size);
    }

    /* FIXME */ printf("; [Assignment] %s(%d/%d) = %s\n",
                       lnode->symbol->name, lnode->symbol->stack_index,
                       stack_size, rnode->name);

    /* rnode */
    if (!IS_LITERAL(rnode->kind)) {
        if (rnode->kind == IDENTIFIER) {
        printf(";rnode: %s, symbol: %s (line: %d)",
                rnode->name, rnode->symbol->name, rnode->linenum);
            if (rnode->symbol->is_global) {
                printf(" --- is_global");
                _print_load(rnode, visitor);
                rindex = stack_size;
            } else
                rindex = rnode->symbol->stack_index;
            printf("\n");
        } else {
            ast_node_accept(rnode, visitor);
            rindex = stack_size;
        }


    } else {
        /* FIXME */printf("%%%d = ", stack_size + 1);
        printf(TAB"add ");
        PRINT_TYPE(lnode->type);
        __print_rvalue(rnode);
        printf("0\n", stack_size);
        stack_size++;
        rindex = stack_size;
    }

    /* lnode */
    if (!lnode->symbol->is_global) {
        lnode->symbol->stack_index = rindex;
    } else if (!lnode->symbol->is_procfunc) {
        printf(TAB"store ");
        PRINT_TYPE(lnode->type);
        __print_rvalue(rnode);
        PRINT_TYPE(lnode->type);
        printf("* ");
        ast_node_accept(lnode, visitor);
        printf(", align 4\n");
    } else
        lnode->symbol->stack_index = stack_size;

    /* FIXME */ printf("; [Assignment] %s(%d/%d) = %s\n",
                       lnode->symbol->name, lnode->symbol->stack_index,
                       stack_size, rnode->name);
}

void
llvm_codegen_visit_if_stmt (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;
    const char *var;

    printf("if (");
    child = node->children; // Expression
    ast_node_accept(child, visitor);
    printf(") {\n");

    child = child->sibling; // If Statements
    ast_node_accept(child, visitor);

    printf("\n");
    printf("}");

    child = child->sibling; // Else Statements

    if (child != NULL) {
        printf(" else {\n");
        ast_node_accept(child, visitor);
        printf("\n");
        printf("}");
    }
    printf("\n");
}

void
llvm_codegen_visit_while_stmt (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;
    const char *var;

    printf("while (");
    child = node->children; // Expression
    ast_node_accept(child, visitor);
    printf(") {\n");

    child = child->sibling; // Statements
    ast_node_accept(child, visitor);

    printf("}\n");
}

void
llvm_codegen_visit_for_stmt (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child;
    const char *var;

    printf("for (");
    child = node->children; // Assignment
    ast_node_accept(child, visitor);

    var = child->children->symbol->name;
    printf(" %s < ", var);

    child = child->sibling; // Stop condition
    ast_node_accept(child, visitor);

    printf("; %s++) {\n", var);

    child = child->sibling; // Statements
    ast_node_accept_children(child, visitor);

    printf("\n");
    printf("}\n");
}

void
llvm_codegen_visit_notfactor (struct _Visitor *visitor, struct AstNode *node)
{
    ast_node_accept_children(node->children, visitor);
}

void
llvm_codegen_visit_call (struct _Visitor *visitor, struct AstNode *node)
{
    struct AstNode *child = node->children;

    printf(TAB"call ");
    PRINT_TYPE(child->symbol->type);
    printf(" ");
    ast_node_accept(child, visitor);
    printf("( ");
    for (child = child->sibling->children; child != NULL;
         child = child->sibling) {
        PRINT_TYPE(child->type);
        printf(" ");
        ast_node_accept(child, visitor);
        if (child->sibling != NULL)
            printf(", ");
    }
    printf(" )\n");
    stack_size++;
}

void
llvm_codegen_visit_simplenode (struct _Visitor *visitor, struct AstNode *node)
{
    ast_node_accept_children(node->children, visitor);
}

void
llvm_codegen_visit_binary_op (struct _Visitor *visitor, struct AstNode *node)
{
    /* FIXME */printf("%%%d = ", stack_size + 1);
    switch (node->kind) {
        case T_OR:
            printf("or");
            break;
        case T_AND:
            printf("and");
            break;
        /*case T_EQUAL:
            printf(" == ");
            break;
        case T_NOTEQUAL:
            printf(" != ");
            break;
        case T_LESSER:
            printf(" < ");
            break;
        case T_GREATER:
            printf(" > ");
            break;
        case T_LESSEREQUAL:
            printf(" <= ");
            break;
        case T_GREATEREQUAL:
            printf(" >= ");
            break;*/
        case T_PLUS:
            printf("add");
            break;
        case T_MINUS:
            printf("sub");
            break;
        case T_STAR:
            printf("mul");
            break;
        /*case T_SLASH:
            printf(" %s ", node->name);*/
    }
}

void
llvm_codegen_visit_not_op (struct _Visitor *visitor, struct AstNode *node)
{
    printf(" !", node->name);
}

static void
_print_boolean(struct AstNode *node)
{
    printf("%bool_str = getelementptr [");
    if (node->value.boolean)
        printf("5 x i8]* @.true");
    else
        printf("6 x i8]* @.false");
    printf("_str, i32 0, i32 0\n");
}

static void
_print_load(struct AstNode *node, Visitor *visitor)
{
    /* FIXME */printf("%%%d = ", stack_size + 1);
    printf(TAB"load ");
    PRINT_TYPE(node->type);
    printf("* ");
    ast_node_accept(node, visitor);
    printf(", align 4\n");
    stack_size++;
}

static int
_get_type_size(Type type)
{
    switch (type) {
        case INTEGER:
            return 32;
        case BOOLEAN:
            return 1;
        case CHAR:
            return 8;
        default:
            return 0;
    }
}
