#ifndef INPUT_H
#define INPUT_H

#include "dfa.h"
#include "context.h"

typedef enum INPUT_STATE {
    NORMAL_NOP,
    NORMAL_NAV_HOME, // 0
    NORMAL_NAV_END, // $
    NORMAL_NAV_LFT, // h
    NORMAL_NAV_RGT, // l
    NORMAL_NAV_UP,  // k
    NORMAL_NAV_DOWN, // j
    NORMAL_NAV_FILEEND, // G
    NORMAL_ENTER_CMD, // :
    NORMAL_ENTER_INSERT,      // i
    NORMAL_ENTER_INSERT_END,  // A
    NORMAL_ENTER_INSERT_HOME, // I
    NORMAL_ENTER_INSERT_DOWN, // o
    NORMAL_ENTER_INSERT_UP,   // O
    NORMAL_SCROLL_UP,         // Ctrl+y
    NORMAL_SCROLL_DOWN,       // Ctrl+e
    NORMAL_PAGE_DOWN,         // Ctrl+d
    NORMAL_PAGE_UP,           // Ctrl+u
    NORMAL_D, // 'd'
    NORMAL_D_LINE, // 'dd'

    NORMAL_G, // 'g'
    NORMAL_G_HOME, // 'gg'

    NORMAL_LEADER, // ' '
    NORMAL_EDIT_1,
    NORMAL_EDIT_2,
    NORMAL_EDIT_3,
    NORMAL_EDIT_4,
    NORMAL_EDIT_5,
    NORMAL_EDIT_6,
    NORMAL_EDIT_7,
    NORMAL_EDIT_8,
    NORMAL_EDIT_9,

    NORMAL_OPEN_TREE,
    NORMAL_CLOSE_TREE,
} INPUT_STATE;

extern dfa_t dfa_normal;

void init_inputs();

void handle_input_normal(context_t* ctx, int c);

void handle_input_insert(context_t *ctx, int c);

void handle_input_command(context_t *ctx, int c);

#endif // INPUT_H
