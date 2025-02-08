#include "src/eval.h"
#include "src/parser.h"

static Json *eval_primary(ASTNode *node, Json *input);
static Json *eval_unary(ASTNode *node, Json *input);
static Json *eval_binary(ASTNode *node, Json *input);
static Json *eval_function(ASTNode *node, Json *input);
static Json *eval_access(ASTNode *node, Json *input);
static Json *eval_list(ASTNode *node, Json *input);
static Json *eval_json_field(ASTNode *node, Json *input);
static Json *eval_json_object(ASTNode *node, Json *input);
static Json *eval_grouping(ASTNode *node, Json *input);

Json *eval(ASTNode *node, Json *input) {
}
