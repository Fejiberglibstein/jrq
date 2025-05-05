#include "src/eval/private.h"
#include "src/json_iter.h"

JsonIterator eval_func_map(Eval *e, ASTNode *node);
JsonIterator eval_func_filter(Eval *e, ASTNode *node);
JsonIterator eval_func_iter(Eval *e, ASTNode *node);
JsonIterator eval_func_keys(Eval *e, ASTNode *node);
JsonIterator eval_func_values(Eval *e, ASTNode *node);
JsonIterator eval_func_enumerate(Eval *e, ASTNode *node);
JsonIterator eval_func_zip(Eval *e, ASTNode *node);
JsonIterator eval_func_skip_while(Eval *e, ASTNode *node);
JsonIterator eval_func_take_while(Eval *e, ASTNode *node);

Json eval_func_collect(Eval *e, ASTNode *node);
Json eval_func_sum(Eval *e, ASTNode *node);
Json eval_func_product(Eval *e, ASTNode *node);
Json eval_func_flatten(Eval *e, ASTNode *node);
Json eval_func_join(Eval *e, ASTNode *node);
Json eval_func_length(Eval *e, ASTNode *node);
