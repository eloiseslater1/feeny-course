#ifndef FRAME_H
#define FRAME_H

#include <stddef.h>
#include "bytecode.h"

typedef struct {
    void** return_address;
    void** variables;
    void* parent;
} Frame;

Frame* make_frame(int size, Frame* current_frame, void** return_address);
void destroy_frame(Frame* frame);


#endif