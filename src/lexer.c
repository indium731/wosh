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

Token create_token(TokenType type, char *lexeme, int lexeme_len)
{

	Token token;
	token.type = type;
	token.lexeme = lexeme;
	token.lexeme_len = lexeme_len;
	return token;

}

#define WOSH_TOKEN_TYPES "|<;>\n\t "

int get_lexeme_len (char *start)
{
	int count = 0;

	for (;;)
	{
		count++;

		if (strchr(WOSH_TOKEN_TYPES, *(start+count)))
			return count;
	}
}


Token scan_token (char *start, char **curr)
{
	Token token;
	token = create_token (TOK_ERROR, "", -1);
	//(*curr)++;

	if (**curr == '\0')
	{
		return token;
	}
	switch (**curr)
	{
		case '|': 
			token.type = TOK_PIPE;
			return token;
		case '<':
			token.type = TOK_REDIRECT_IN;
			return token;
		case ';':
			token.type = TOK_SEMICOLON;
			return token;
		case '\n':
			token.type = TOK_NEWLINE;
			return token;

		case '>':
			token.type = (*(*curr + 1)) ? TOK_REDIRECT_OUT : TOK_REDIRECT_APPEND;
			return token;

		case '\"':
		case '\'':
			token.type = TOK_WORD;
			token.lexeme = *curr + 1;
			token.lexeme_len = (int)(strchr(*curr + 1, **curr) - (*curr + 1));
			*curr = *curr + token.lexeme_len + 2;
			return token;

		default:
			token.type = TOK_WORD;
			token.lexeme = start;
			token.lexeme_len = get_lexeme_len(start);
			*curr = *curr + token.lexeme_len+1;
			return token;

	}
}


Token *wosh_lex_line (char *line)
{
	Token *tokens = (Token*)malloc(sizeof(Token));
	Token *tokens_backup;
	int tokens_counter = 0;

	char *curr = line;
	char *start;
	Token token;
	while (*curr != '\0')
	{
		start = curr;
		token = scan_token (start, &curr);
		if (token.type == TOK_ERROR)
			break;

		tokens_backup = tokens;
		tokens = (Token*)realloc(tokens, (tokens_counter + 1) * sizeof (Token));

		if (!tokens)
		{
			free (tokens_backup);
			exit (EXIT_FAILURE);
		}
		tokens[tokens_counter] = token;
		tokens_counter++;

	}

	tokens_backup = tokens;
	tokens = (Token*)realloc(tokens, (tokens_counter + 1) * sizeof (Token));

	if (!tokens)
	{
		free (tokens_backup);
		exit (EXIT_FAILURE);
	}
	tokens[tokens_counter].type = TOK_EOF;
	tokens[tokens_counter].lexeme = "";
	return tokens;
}
