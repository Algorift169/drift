#ifndef DRIFT_COMMENTS_H
#define DRIFT_COMMENTS_H

#include "drift/lexer.h"

int lexer_skip_comments(Lexer *lexer);
int is_block_comment_open(const char *source);
void comments_reset_state(void);

#endif
