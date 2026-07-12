#ifndef WOSH_LEXER_H
#define WOSH_LEXER_H

#include <string.h>
#include <stdlib.h>

typedef enum
{
	TOK_WORD,
  TOK_PIPE,
  TOK_REDIRECT_IN, 
  TOK_REDIRECT_OUT,
  TOK_REDIRECT_APPEND,
  TOK_SEMICOLON,
  TOK_NEWLINE,
  TOK_EOF,
	TOK_ERROR
} TokenType;

typedef struct 
{
	TokenType type;
	char *lexeme;
	int lexeme_len;
} Token;

Token *wosh_lex_line (char *line);

#endif
