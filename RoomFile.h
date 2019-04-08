#ifndef ROOMFILE_H
#define ROOMFILE_H

#include <assert.h>
#include <stddef.h>
#include "File.h"

ssize_t ARF_find_code_start(AF* f, size_t start);

#pragma RcB2 DEP "RoomFile.c"

#endif
