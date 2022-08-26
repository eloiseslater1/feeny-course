#include "frame.h"
#include <stdlib.h>

Frame* make_frame(int size, Frame* current_frame, void** return_address) {
    void** variables = (void**)malloc(sizeof(void*) * size);
    Frame* frame = malloc(sizeof(Frame));
    frame->return_address = return_address;
    frame->variables = variables;
    frame->parent = current_frame;
    return frame;
}

void destroy_frame(Frame* frame) {
    free(frame->variables);
    free(frame);
    free(frame->return_address);
}