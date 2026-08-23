#include "lexer.h"
#include "utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *name;
  int len;
  TokenType type;
} Keyword;

static const Keyword keywords[] = {
    {"function", 8, TOKEN_FUNCTION},
    {"local", 5, TOKEN_LOCAL},
    {"return", 6, TOKEN_RETURN},
    {"if", 2, TOKEN_IF},
    {"then", 4, TOKEN_THEN},
    {"else", 4, TOKEN_ELSE},
    {"elseif", 6, TOKEN_ELSEIF},
    {"end", 3, TOKEN_END},
    {"for", 3, TOKEN_FOR},
    {"while", 5, TOKEN_WHILE},
    {"do", 2, TOKEN_DO},
    {"in", 2, TOKEN_IN},
    {"repeat", 6, TOKEN_REPEAT},
    {"until", 5, TOKEN_UNTIL},
    {"break", 5, TOKEN_BREAK},
    {"continue", 8, TOKEN_CONTINUE},
    {"asm", 3, TOKEN_ASM},
    {"defer", 5, TOKEN_DEFER},
    {"struct", 6, TOKEN_STRUCT},
    {"true", 4, TOKEN_TRUE},
    {"false", 5, TOKEN_FALSE},
    {"nil", 3, TOKEN_NIL},
    {"and", 3, TOKEN_AND},
    {"or", 2, TOKEN_OR},
    {"not", 3, TOKEN_NOT},
    {"int32", 5, TOKEN_TYPE_INT32},
    {"int64", 5, TOKEN_TYPE_INT64},
    {"float32", 7, TOKEN_TYPE_FLOAT32},
    {"float64", 7, TOKEN_TYPE_FLOAT64},
    {"uint8", 5, TOKEN_TYPE_UINT8},
    {"uint64", 6, TOKEN_TYPE_UINT64},
    {"void", 4, TOKEN_TYPE_VOID},
    {"type", 4, TOKEN_TYPE_KW},
    {"export", 6, TOKEN_EXPORT},
    {"typeof", 6, TOKEN_TYPEOF},
    {"string", 6, TOKEN_TYPE_STRING},
    {"boolean", 7, TOKEN_TYPE_BOOLEAN},
    {"number", 6, TOKEN_TYPE_NUMBER},
    {"thread", 6, TOKEN_TYPE_THREAD},
    {"any", 3, TOKEN_TYPE_ANY},
    {"generic", 7, TOKEN_GENERIC},
    {"module", 6, TOKEN_MODULE},
    {"int", 3, TOKEN_TYPE_INT},
    {"enum", 4, TOKEN_ENUM},
    {"__c", 3, TOKEN_CBLOCK},
    {NULL, 0, TOKEN_EOF},
};

typedef struct {
  char c1;
  char c2;
  char c3;
  TokenType type;
  const char *text;
} DoubleCharToken;

static const DoubleCharToken dctokens[] = {
    {'=', '=', '\0', TOKEN_EQ, "=="},
    {'!', '=', '\0', TOKEN_NEQ, "!="},
    {'<', '=', '\0', TOKEN_LTE, "<="},
    {'>', '=', '\0', TOKEN_GTE, ">="},
    {'<', '<', '\0', TOKEN_SHL, "<<"},
    {'>', '>', '\0', TOKEN_SHR, ">>"},
    {'-', '>', '\0', TOKEN_ARROW, "->"},
    {':', ':', '\0', TOKEN_DBLCOLON, "::"},
    {':', '=', '\0', TOKEN_WALRUS, ":="},
    {'+', '=', '\0', TOKEN_PLUS_EQ, "+="},
    {'-', '=', '\0', TOKEN_MINUS_EQ, "-="},
    {'*', '=', '\0', TOKEN_STAR_EQ, "*="},
    {'/', '=', '\0', TOKEN_SLASH_EQ, "/="},
    {'%', '=', '\0', TOKEN_PERCENT_EQ, "%="},
    {'^', '=', '\0', TOKEN_BITXOR_EQ, "^="},
    {'&', '=', '\0', TOKEN_BITAND_EQ, "&="},
    {'|', '=', '\0', TOKEN_BITOR_EQ, "|="},
    {'.', '.', '\0', TOKEN_CONCAT, ".."},
    {'.', '.', '.', TOKEN_ELLIPSIS, "..."},
    {'/', '/', '\0', TOKEN_FLOOR_DIV, "//"},
    {'/', '/', '=', TOKEN_FLOOR_DIV_EQ, "//="},
    {'\0', '\0', '\0', TOKEN_EOF, NULL},
};

Lexer *lexer_create(const char *source) {
  Lexer *lex = calloc(1, sizeof(Lexer));
  lex->source = source;
  lex->pos = 0;
  lex->line = 1;
  lex->column = 1;
  return lex;
}

static inline char lexer_peek(Lexer *lex, int offset) {
  return lex->source[lex->pos + offset];
}

static inline char lexer_current(Lexer *lex) { return lex->source[lex->pos]; }

static inline void lexer_advance(Lexer *lex) {
  if (lex->source[lex->pos] == '\n') {
    lex->line++;
    lex->column = 1;
  } else {
    lex->column++;
  }
  lex->pos++;
}

static inline void lexer_skip_whitespace(Lexer *lex) {
  char c;
  while ((c = lexer_current(lex)) == ' ' || c == '\t' || c == '\n' ||
         c == '\r' || c == '\f' || c == '\v') {
    lexer_advance(lex);
  }
}

static void lexer_skip_comment(Lexer *lex) {
  char c = lexer_current(lex);

  if (c == ';') {
    lexer_advance(lex);
    while (lexer_current(lex) != '\n' && lexer_current(lex) != '\0')
      lexer_advance(lex);
    return;
  }

  if (c == '-') {
    if (lexer_peek(lex, 1) == '-') {
      lexer_advance(lex);
      lexer_advance(lex);
      if (lexer_current(lex) == '[' && lexer_peek(lex, 1) == '[') {
        int eq_count = 0;
        lexer_advance(lex);
        lexer_advance(lex);
        while (lexer_current(lex) == '=') {
          eq_count++;
          lexer_advance(lex);
        }
        if (lexer_current(lex) == '[') {
          lexer_advance(lex);
          while (lexer_current(lex) != '\0') {
            if (lexer_current(lex) == ']') {
              lexer_advance(lex);
              int close_eq = 0;
              while (lexer_current(lex) == '=' && close_eq < eq_count) {
                close_eq++;
                lexer_advance(lex);
              }
              if (close_eq == eq_count && lexer_current(lex) == ']') {
                lexer_advance(lex);
                break;
              }
            } else {
              lexer_advance(lex);
            }
          }
        }
      } else {
        while (lexer_current(lex) != '\n' && lexer_current(lex) != '\0')
          lexer_advance(lex);
      }
      return;
    }
    return;
  }

  if (c == '/') {
    if (lexer_peek(lex, 1) == '/') {
      lexer_advance(lex);
      lexer_advance(lex);
      while (lexer_current(lex) != '\n' && lexer_current(lex) != '\0')
        lexer_advance(lex);
      return;
    }
    if (lexer_peek(lex, 1) == '*' && lexer_peek(lex, 2) == '[') {
      int nesting = 1;
      lexer_advance(lex);
      lexer_advance(lex);
      lexer_advance(lex);
      while (lexer_current(lex) != '\0' && nesting > 0) {
        if (lexer_current(lex) == '[' && lexer_peek(lex, 1) == '*' &&
            lexer_peek(lex, 2) == '/') {
          lexer_advance(lex);
          lexer_advance(lex);
          lexer_advance(lex);
          nesting++;
        } else if (lexer_current(lex) == '/' && lexer_peek(lex, 1) == '*' &&
                   lexer_peek(lex, 2) == ']') {
          lexer_advance(lex);
          lexer_advance(lex);
          lexer_advance(lex);
          nesting--;
        } else {
          lexer_advance(lex);
        }
      }
    } else if (lexer_peek(lex, 1) == '*') {
      lexer_advance(lex);
      lexer_advance(lex);
      while (lexer_current(lex) != '\0') {
        if (lexer_current(lex) == '*' && lexer_peek(lex, 1) == '/') {
          lexer_advance(lex);
          lexer_advance(lex);
          break;
        }
        lexer_advance(lex);
      }
    }
  }
}

static Token make_token(TokenType type, const char *text, int line, int col) {
  Token tok;
  tok.type = type;
  tok.text = string_copy(text);
  tok.line = line;
  tok.column = col;
  tok.int_value = 0;
  tok.float_value = 0.0;
  return tok;
}

static Token lexer_read_string(Lexer *lex) {
  int line = lex->line, col = lex->column;
  char quote = lexer_current(lex);
  lexer_advance(lex);

  if (lexer_current(lex) == '[') {
    int eq_count = 0;
    lexer_advance(lex);
    while (lexer_current(lex) == '=') {
      eq_count++;
      lexer_advance(lex);
    }
    if (lexer_current(lex) == '[') {
      lexer_advance(lex);
      int cap = 256, len = 0;
      char *buf = malloc(cap);

      while (lexer_current(lex) != '\0') {
        if (lexer_current(lex) == ']') {
          lexer_advance(lex);
          int close_eq = 0;
          while (lexer_current(lex) == '=' && close_eq < eq_count) {
            close_eq++;
            lexer_advance(lex);
          }
          if (close_eq == eq_count && lexer_current(lex) == ']') {
            lexer_advance(lex);
            break;
          }
          if (len + close_eq + 1 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
          }
          buf[len++] = ']';
          for (int i = 0; i < close_eq; i++)
            buf[len++] = '=';
          continue;
        }
        if (len + 1 >= cap) {
          cap *= 2;
          buf = realloc(buf, cap);
        }
        buf[len++] = lexer_current(lex);
        lexer_advance(lex);
      }

      buf[len] = '\0';
      Token tok = make_token(TOKEN_STRING, buf, line, col);
      free(buf);
      return tok;
    }
  }

  int cap = 256, len = 0;
  char *buf = malloc(cap);

  while (lexer_current(lex) != quote && lexer_current(lex) != '\0') {
    if (lexer_current(lex) == '\\') {
      lexer_advance(lex);
      if (len + 1 >= cap) {
        cap *= 2;
        buf = realloc(buf, cap);
      }
      switch (lexer_current(lex)) {
      case 'a':
        buf[len++] = '\a';
        break;
      case 'b':
        buf[len++] = '\b';
        break;
      case 'f':
        buf[len++] = '\f';
        break;
      case 'n':
        buf[len++] = '\n';
        break;
      case 'r':
        buf[len++] = '\r';
        break;
      case 't':
        buf[len++] = '\t';
        break;
      case 'v':
        buf[len++] = '\v';
        break;
      case '\\':
        buf[len++] = '\\';
        break;
      case '"':
        buf[len++] = '"';
        break;
      case '\'':
        buf[len++] = '\'';
        break;
      case '0':
        buf[len++] = '\0';
        break;
      case 'x': {
        lexer_advance(lex);
        char hex[3] = {lexer_current(lex), lexer_peek(lex, 1), '\0'};
        buf[len++] = (char)strtol(hex, NULL, 16);
        lexer_advance(lex);
        break;
      }
      case '\n':
        if (quote == '"')
          buf[len++] = '\n';
        break;
      default:
        buf[len++] = lexer_current(lex);
        break;
      }
    } else {
      if (len + 1 >= cap) {
        cap *= 2;
        buf = realloc(buf, cap);
      }
      buf[len++] = lexer_current(lex);
    }
    lexer_advance(lex);
  }

  buf[len] = '\0';
  if (lexer_current(lex) == quote)
    lexer_advance(lex);

  Token tok = make_token(TOKEN_STRING, buf, line, col);
  free(buf);
  return tok;
}

static Token lexer_read_number(Lexer *lex) {
  int line = lex->line, col = lex->column;
  char buf[128];
  int i = 0;
  bool is_float = false;
  bool is_hex = false;

  if (lexer_current(lex) == '0' &&
      (lexer_peek(lex, 1) == 'x' || lexer_peek(lex, 1) == 'X')) {
    buf[i++] = lexer_current(lex);
    lexer_advance(lex);
    buf[i++] = lexer_current(lex);
    lexer_advance(lex);
    is_hex = true;
  }

  while (is_hex ? isxdigit(lexer_current(lex))
                : (isdigit(lexer_current(lex)) || lexer_current(lex) == '.' ||
                   lexer_current(lex) == 'e' || lexer_current(lex) == 'E' ||
                   lexer_current(lex) == '+' || lexer_current(lex) == '-')) {
    if (lexer_current(lex) == '.') {
      if (is_float)
        break;
      is_float = true;
    }
    if (lexer_current(lex) == 'e' || lexer_current(lex) == 'E') {
      is_float = true;
    }
    if (i < 127)
      buf[i++] = lexer_current(lex);
    lexer_advance(lex);
  }

  if (lexer_current(lex) == '.' && !is_float && !is_hex) {
    if (isdigit(lexer_peek(lex, 1)) || lexer_peek(lex, 1) == 'e' ||
        lexer_peek(lex, 1) == 'E') {
      is_float = true;
      buf[i++] = lexer_current(lex);
      lexer_advance(lex);
      while (isdigit(lexer_current(lex)) || lexer_current(lex) == 'e' ||
             lexer_current(lex) == 'E' || lexer_current(lex) == '+' ||
             lexer_current(lex) == '-') {
        if (i < 127)
          buf[i++] = lexer_current(lex);
        lexer_advance(lex);
      }
    }
  }

  buf[i] = '\0';
  Token tok;
  tok.line = line;
  tok.column = col;
  tok.text = string_copy(buf);

  if (is_float) {
    tok.type = TOKEN_NUMBER_FLOAT;
    tok.float_value = strtod(buf, NULL);
    tok.int_value = 0;
  } else {
    tok.type = TOKEN_NUMBER_INT;
    tok.int_value = strtoll(buf, NULL, 0);
    tok.float_value = 0.0;
  }

  return tok;
}

static Token lexer_read_ident(Lexer *lex) {
  int line = lex->line, col = lex->column;
  char buf[256];
  int i = 0;

  while (isalnum(lexer_current(lex)) || lexer_current(lex) == '_') {
    if (i < 255)
      buf[i++] = lexer_current(lex);
    lexer_advance(lex);
  }
  buf[i] = '\0';

  for (int k = 0; keywords[k].name; k++) {
    if (i == keywords[k].len && memcmp(buf, keywords[k].name, i) == 0) {
      return make_token(keywords[k].type, buf, line, col);
    }
  }

  return make_token(TOKEN_IDENT, buf, line, col);
}

Token *lexer_tokenize(Lexer *lexer, int *out_count) {
  int cap = 512, cnt = 0;
  Token *tokens = malloc(sizeof(Token) * cap);

  while (lexer_current(lexer) != '\0') {
    lexer_skip_whitespace(lexer);
    lexer_skip_comment(lexer);
    lexer_skip_whitespace(lexer);

    if (lexer_current(lexer) == '\0')
      break;

    int line = lexer->line, col = lexer->column;
    char c = lexer_current(lexer);

    if (c == '"' || c == '\'') {
      if (cnt >= cap - 1) {
        cap *= 2;
        tokens = realloc(tokens, sizeof(Token) * cap);
      }
      tokens[cnt++] = lexer_read_string(lexer);
      continue;
    }

    if (isdigit(c) || (c == '.' && isdigit(lexer_peek(lexer, 1)))) {
      if (cnt >= cap - 1) {
        cap *= 2;
        tokens = realloc(tokens, sizeof(Token) * cap);
      }
      tokens[cnt++] = lexer_read_number(lexer);
      continue;
    }

    if (isalpha(c) || c == '_') {
      if (cnt >= cap - 1) {
        cap *= 2;
        tokens = realloc(tokens, sizeof(Token) * cap);
      }
      tokens[cnt++] = lexer_read_ident(lexer);
      continue;
    }

    bool matched = false;
    for (int k = 0; dctokens[k].text; k++) {
      if (c == dctokens[k].c1 && lexer_peek(lexer, 1) == dctokens[k].c2 &&
          (dctokens[k].c3 == '\0' || lexer_peek(lexer, 2) == dctokens[k].c3)) {
        int advance = dctokens[k].c3 != '\0' ? 3 : 2;
        for (int a = 0; a < advance; a++)
          lexer_advance(lexer);
        if (cnt >= cap - 1) {
          cap *= 2;
          tokens = realloc(tokens, sizeof(Token) * cap);
        }
        tokens[cnt++] =
            make_token(dctokens[k].type, dctokens[k].text, line, col);
        matched = true;
        break;
      }
    }

    if (matched)
      continue;

    lexer_advance(lexer);
    TokenType type = TOKEN_ERROR;
    const char *text = "?";

    switch (c) {
    case '{':
      type = TOKEN_LBRACE;
      text = "{";
      break;
    case '}':
      type = TOKEN_RBRACE;
      text = "}";
      break;
    case '(':
      type = TOKEN_LPAREN;
      text = "(";
      break;
    case ')':
      type = TOKEN_RPAREN;
      text = ")";
      break;
    case '[':
      type = TOKEN_LBRACK;
      text = "[";
      break;
    case ']':
      type = TOKEN_RBRACK;
      text = "]";
      break;
    case ';':
      type = TOKEN_SEMICOLON;
      text = ";";
      break;
    case ':':
      type = TOKEN_COLON;
      text = ":";
      break;
    case ',':
      type = TOKEN_COMMA;
      text = ",";
      break;
    case '.':
      type = TOKEN_DOT;
      text = ".";
      break;
    case '=':
      type = TOKEN_EQUALS;
      text = "=";
      break;
    case '<':
      type = TOKEN_LT;
      text = "<";
      break;
    case '>':
      type = TOKEN_GT;
      text = ">";
      break;
    case '+':
      type = TOKEN_PLUS;
      text = "+";
      break;
    case '-':
      type = TOKEN_MINUS;
      text = "-";
      break;
    case '*':
      type = TOKEN_STAR;
      text = "*";
      break;
    case '/':
      type = TOKEN_SLASH;
      text = "/";
      break;
    case '%':
      type = TOKEN_PERCENT;
      text = "%";
      break;
    case '^':
      type = TOKEN_BITXOR;
      text = "^";
      break;
    case '&':
      type = TOKEN_BITAND;
      text = "&";
      break;
    case '|':
      type = TOKEN_BITOR;
      text = "|";
      break;
    case '~':
      type = TOKEN_TILDE;
      text = "~";
      break;
    case '!':
      type = TOKEN_BANG;
      text = "!";
      break;
    case '#':
      type = TOKEN_SHARP;
      text = "#";
      break;
    case '?':
      type = TOKEN_QUESTION;
      text = "?";
      break;
    case '@':
      type = TOKEN_AT;
      text = "@";
      break;
    case '$':
      type = TOKEN_DOLLAR;
      text = "$";
      break;
    case '`':
      type = TOKEN_BACKTICK;
      text = "`";
      break;
    case '\\':
      type = TOKEN_BACKSLASH;
      text = "\\";
      break;
    }

    if (cnt >= cap - 1) {
      cap *= 2;
      tokens = realloc(tokens, sizeof(Token) * cap);
    }
    tokens[cnt++] = make_token(type, text, line, col);
  }

  if (cnt >= cap) {
    cap++;
    tokens = realloc(tokens, sizeof(Token) * cap);
  }
  tokens[cnt++] = make_token(TOKEN_EOF, "", lexer->line, lexer->column);

  *out_count = cnt;
  return tokens;
}

void lexer_destroy(Lexer *lexer) { free(lexer); }

const char *token_type_name(TokenType type) {
  static const char *names[] = {
      [TOKEN_EOF] = "EOF",
      [TOKEN_ERROR] = "ERROR",
      [TOKEN_IDENT] = "IDENT",
      [TOKEN_NUMBER_INT] = "INT",
      [TOKEN_NUMBER_FLOAT] = "FLOAT",
      [TOKEN_STRING] = "STRING",
      [TOKEN_FUNCTION] = "function",
      [TOKEN_LOCAL] = "local",
      [TOKEN_RETURN] = "return",
      [TOKEN_IF] = "if",
      [TOKEN_THEN] = "then",
      [TOKEN_ELSE] = "else",
      [TOKEN_ELSEIF] = "elseif",
      [TOKEN_END] = "end",
      [TOKEN_FOR] = "for",
      [TOKEN_WHILE] = "while",
      [TOKEN_DO] = "do",
      [TOKEN_IN] = "in",
      [TOKEN_REPEAT] = "repeat",
      [TOKEN_UNTIL] = "until",
      [TOKEN_BREAK] = "break",
      [TOKEN_CONTINUE] = "continue",
      [TOKEN_ASM] = "asm",
      [TOKEN_DEFER] = "defer",
      [TOKEN_STRUCT] = "struct",
      [TOKEN_TRUE] = "true",
      [TOKEN_FALSE] = "false",
      [TOKEN_NIL] = "nil",
      [TOKEN_AND] = "and",
      [TOKEN_OR] = "or",
      [TOKEN_NOT] = "not",
      [TOKEN_TYPE_INT32] = "int32",
      [TOKEN_TYPE_INT64] = "int64",
      [TOKEN_TYPE_FLOAT32] = "float32",
      [TOKEN_TYPE_FLOAT64] = "float64",
      [TOKEN_TYPE_UINT8] = "uint8",
      [TOKEN_TYPE_UINT64] = "uint64",
      [TOKEN_TYPE_VOID] = "void",
      [TOKEN_TYPE_KW] = "type",
      [TOKEN_EXPORT] = "export",
      [TOKEN_TYPEOF] = "typeof",
      [TOKEN_TYPE_STRING] = "string",
      [TOKEN_TYPE_BOOLEAN] = "boolean",
      [TOKEN_TYPE_NUMBER] = "number",
      [TOKEN_TYPE_THREAD] = "thread",
      [TOKEN_TYPE_ANY] = "any",
      [TOKEN_GENERIC] = "generic",
      [TOKEN_MODULE] = "module",
      [TOKEN_LBRACE] = "{",
      [TOKEN_RBRACE] = "}",
      [TOKEN_LPAREN] = "(",
      [TOKEN_RPAREN] = ")",
      [TOKEN_LBRACK] = "[",
      [TOKEN_RBRACK] = "]",
      [TOKEN_SEMICOLON] = ";",
      [TOKEN_COLON] = ":",
      [TOKEN_COMMA] = ",",
      [TOKEN_DOT] = ".",
      [TOKEN_EQUALS] = "=",
      [TOKEN_EQ] = "==",
      [TOKEN_NEQ] = "!=",
      [TOKEN_LT] = "<",
      [TOKEN_GT] = ">",
      [TOKEN_LTE] = "<=",
      [TOKEN_GTE] = ">=",
      [TOKEN_PLUS] = "+",
      [TOKEN_MINUS] = "-",
      [TOKEN_STAR] = "*",
      [TOKEN_SLASH] = "/",
      [TOKEN_PERCENT] = "%",
      [TOKEN_SHARP] = "#",
      [TOKEN_BITAND] = "&",
      [TOKEN_BITOR] = "|",
      [TOKEN_BITXOR] = "^",
      [TOKEN_BANG] = "!",
      [TOKEN_TILDE] = "~",
      [TOKEN_SHL] = "<<",
      [TOKEN_SHR] = ">>",
      [TOKEN_ARROW] = "->",
      [TOKEN_DBLCOLON] = "::",
      [TOKEN_WALRUS] = ":=",
      [TOKEN_PLUS_EQ] = "+=",
      [TOKEN_MINUS_EQ] = "-=",
      [TOKEN_STAR_EQ] = "*=",
      [TOKEN_SLASH_EQ] = "/=",
      [TOKEN_PERCENT_EQ] = "%=",
      [TOKEN_BITXOR_EQ] = "^=",
      [TOKEN_BITAND_EQ] = "&=",
      [TOKEN_BITOR_EQ] = "|=",
      [TOKEN_CONCAT] = "..",
      [TOKEN_ELLIPSIS] = "...",
      [TOKEN_FLOOR_DIV] = "//",
      [TOKEN_FLOOR_DIV_EQ] = "//=",
      [TOKEN_QUESTION] = "?",
      [TOKEN_AT] = "@",
      [TOKEN_DOLLAR] = "$",
      [TOKEN_BACKTICK] = "`",
      [TOKEN_BACKSLASH] = "\\",
      [TOKEN_CBLOCK] = "__c",
  };

  if (type >= 0 && type < (TokenType)(sizeof(names) / sizeof(names[0]))) {
    return names[type] ? names[type] : "?";
  }
  return "?";
}
