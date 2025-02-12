#ifndef _JSON_ITER_H
#define _JSON_ITER_H

#include "json.h"

typedef struct JsonIterator *JsonIterator;

JsonIterator iter_obj_keys(Json j);

#endif // _JSON_ITER_H
