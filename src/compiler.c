//
// Hook Programming Language
// compiler.c
//

#include "compiler.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "builtin.h"
#include "error.h"

#define COMPILER_MAX_LOCALS 256
#define COMPILER_MAX_BREAKS 256

#define MATCH(s, t) ((s)->token.type == (t))

#define EXPECT(s, t) do \
  { \
    if (!MATCH(s, t)) \
      fatal_error_unexpected_token(s); \
    scanner_next_token(s); \
  } while(0)

typedef struct
{
  int depth;
  int length;
  char *start;
} local_t;

typedef struct loop
{
  struct loop *enclosing;
  int scope_depth;
  uint16_t jump;
  int num_offsets;
  int offsets[COMPILER_MAX_BREAKS];
} loop_t;

typedef struct
{
  scanner_t *scan;
  int scope_depth;
  int num_locals;
  local_t locals[COMPILER_MAX_LOCALS];
  loop_t *loop;
  function_t *fn;
} compiler_t;

static inline void fatal_error_unexpected_token(scanner_t *scan);
static inline long parse_long(char *chars);
static inline double parse_double(char *chars);
static inline void push_scope(compiler_t *comp);
static inline void pop_scope(compiler_t *comp);
static inline int discard_locals(compiler_t *comp, int depth);
static inline bool name_equal(token_t *tk, local_t *local);
static inline void add_local(compiler_t *comp, int length, char *start);
static inline void define_local(compiler_t *comp, token_t *tk);
static inline uint8_t resolve_variable(compiler_t *comp, token_t *tk, bool *is_local);
static inline int resolve_local(compiler_t *comp, token_t *tk);
static inline int emit_jump(chunk_t *chunk, opcode_t op);
static inline void patch_jump(chunk_t *chunk, int offset);
static inline void start_loop(compiler_t *comp, loop_t *loop);
static inline void add_break(compiler_t *comp);
static inline void end_loop(compiler_t *comp);
static void compile_statement(compiler_t *comp);
static void compile_block(compiler_t *comp);
static void compile_variable_declaration(compiler_t *comp);
static void compile_assignment(compiler_t *comp, token_t tk);
static void compile_call_statement(compiler_t *comp, token_t tk);
static void compile_function_declaration(compiler_t *comp);
static void compile_delete_statement(compiler_t *comp);
static void compile_if_statement(compiler_t *comp);
static void compile_loop_statement(compiler_t *comp);
static void compile_while_statement(compiler_t *comp);
static void compile_do_statement(compiler_t *comp);
static void compile_for_statement(compiler_t *comp);
static void compile_continue_statement(compiler_t *comp);
static void compile_break_statement(compiler_t *comp);
static void compile_return_statement(compiler_t *comp);
static void compile_expression(compiler_t *comp);
static void compile_and_expression(compiler_t *comp);
static void compile_equal_expression(compiler_t *comp);
static void compile_comp_expression(compiler_t *comp);
static void compile_add_expression(compiler_t *comp);
static void compile_mul_expression(compiler_t *comp);
static void compile_unary_expression(compiler_t *comp);
static void compile_prim_expression(compiler_t *comp);
static void compile_array_constructor(compiler_t *comp);
static void compile_anonymous_function(compiler_t *comp);
static void compile_subscript_or_call(compiler_t *comp);
static void compiler_init(compiler_t *comp, scanner_t *scan, string_t *name);

static inline void fatal_error_unexpected_token(scanner_t *scan)
{
  token_t *tk = &scan->token;
  if (tk->type == TOKEN_EOF)
    fatal_error("unexpected end of file at %d:%d", tk->line, tk->col);
  fatal_error("unexpected token '%.*s' at %d:%d", tk->length, tk->start,
    tk->line, tk->col);
}

static inline long parse_long(char *chars)
{
  errno = 0;
  long result = strtol(chars, NULL, 10);
  if (errno == ERANGE)
    fatal_error("integer number too large");
  return result;
}

static inline double parse_double(char *chars)
{
  errno = 0;
  double result = strtod(chars, NULL);
  if (errno == ERANGE)
    fatal_error("floating point number too large");
  return result;
}

static inline void push_scope(compiler_t *comp)
{
  ++comp->scope_depth;
}

static inline void pop_scope(compiler_t *comp)
{
  comp->num_locals -= discard_locals(comp, comp->scope_depth);
  --comp->scope_depth;
}

static inline int discard_locals(compiler_t *comp, int depth)
{
  local_t *locals = comp->locals;
  chunk_t *chunk = &comp->fn->chunk;
  int index = comp->num_locals - 1;
  for (; index > -1 && locals[index].depth >= depth; --index)
    chunk_emit_opcode(chunk, OP_POP);
  return comp->num_locals - index - 1;
}

static inline bool name_equal(token_t *tk, local_t *local)
{
  return tk->length == local->length
    && !memcmp(tk->start, local->start, tk->length);
}

static inline void add_local(compiler_t *comp, int length, char *start)
{
  local_t *local = &comp->locals[comp->num_locals];
  local->depth = comp->scope_depth;
  local->length = length;
  local->start = start;
  ++comp->num_locals;
}

static inline void define_local(compiler_t *comp, token_t *tk)
{
  for (int i = comp->num_locals - 1; i > -1; --i)
  {
    local_t *local = &comp->locals[i];
    if (local->depth < comp->scope_depth)
      break;
    if (name_equal(tk, local))
      fatal_error("variable '%.*s' is already defined in this scope",
        tk->length, tk->start);
  }
  if (comp->num_locals == COMPILER_MAX_LOCALS)
    fatal_error("cannot declare more than %d variables in one scope",
      COMPILER_MAX_LOCALS);
  add_local(comp, tk->length, tk->start);
}

static inline uint8_t resolve_variable(compiler_t *comp, token_t *tk, bool *is_local)
{
  *is_local = true;
  int index = resolve_local(comp, tk);
  if (index == -1)
  {
    *is_local = false;
    index = resolve_global(tk->length, tk->start);
  }
  if (index == -1)
    fatal_error("variable '%.*s' is used but not defined", tk->length, tk->start);
  return (uint8_t) index;
}

static inline int resolve_local(compiler_t *comp, token_t *tk)
{
  int index = comp->num_locals - 1;
  for (; index > -1; --index)
    if (name_equal(tk, &comp->locals[index]))
      break;
  return index;
}

static inline int emit_jump(chunk_t *chunk, opcode_t op)
{
  chunk_emit_opcode(chunk, op);
  int offset = chunk->length;
  chunk_emit_word(chunk, 0);
  return offset;
}

static inline void patch_jump(chunk_t *chunk, int offset)
{
  int jump = chunk->length;
  if (jump > UINT16_MAX)
    fatal_error("code too large");
  *((uint16_t *) &chunk->bytes[offset]) = (uint16_t) jump;
}

static inline void start_loop(compiler_t *comp, loop_t *loop)
{
  loop->enclosing = comp->loop;
  loop->scope_depth = comp->scope_depth;
  loop->jump = comp->fn->chunk.length;
  loop->num_offsets = 0;
  comp->loop = loop;
}

static inline void add_break(compiler_t *comp)
{
  loop_t *loop = comp->loop;
  if (loop->num_offsets == COMPILER_MAX_BREAKS)
    fatal_error("too many breaks");
  int offset = emit_jump(&comp->fn->chunk, OP_JUMP);
  loop->offsets[loop->num_offsets++] = offset;
}

static inline void end_loop(compiler_t *comp)
{
  chunk_t *chunk = &comp->fn->chunk;
  loop_t *loop = comp->loop;
  for (int i = 0; i < loop->num_offsets; ++i)
    patch_jump(chunk, loop->offsets[i]);
  comp->loop = comp->loop->enclosing;
}

static void compile_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  if (MATCH(scan, TOKEN_LBRACE))
  {
    compile_block(comp);
    return;
  }
  if (MATCH(scan, TOKEN_VAR))
  {
    compile_variable_declaration(comp);
    EXPECT(scan, TOKEN_SEMICOLON);
    return;
  }
  if (MATCH(scan, TOKEN_NAME))
  {
    token_t tk = scan->token;
    scanner_next_token(scan);
    if (MATCH(scan, TOKEN_LPAREN))
    {
      compile_call_statement(comp, tk);
      return;
    }
    compile_assignment(comp, tk);
    EXPECT(scan, TOKEN_SEMICOLON);
    return;
  }
  if (MATCH(scan, TOKEN_FN))
  {
    compile_function_declaration(comp);
    return;
  }
  if (MATCH(scan, TOKEN_DELETE))
  {
    compile_delete_statement(comp);
    return;
  }
  if (MATCH(scan, TOKEN_IF))
  {
    compile_if_statement(comp);
    return;
  }
  if (MATCH(scan, TOKEN_LOOP))
  {
    compile_loop_statement(comp);
    return;
  }
  if (MATCH(scan, TOKEN_WHILE))
  {
    compile_while_statement(comp);
    return;
  }
  if (MATCH(scan, TOKEN_DO))
  {
    compile_do_statement(comp);
    return;
  }
  if (MATCH(scan, TOKEN_FOR))
  {
    compile_for_statement(comp);
    return;
  }
  if (MATCH(scan, TOKEN_CONTINUE))
  {
    compile_continue_statement(comp);
    return;
  }
  if (MATCH(scan, TOKEN_BREAK))
  {
    compile_break_statement(comp);
    return;
  }
  if (MATCH(scan, TOKEN_RETURN))
  {
    compile_return_statement(comp);
    return;
  }
  fatal_error_unexpected_token(scan);
}

static void compile_block(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  scanner_next_token(scan);
  push_scope(comp);
  while (!MATCH(scan, TOKEN_RBRACE))
    compile_statement(comp);
  scanner_next_token(scan);
  pop_scope(comp);
}

static void compile_variable_declaration(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  if (MATCH(scan, TOKEN_NAME))
  {
    define_local(comp, &scan->token);
    scanner_next_token(scan);
    if (MATCH(scan, TOKEN_EQ))
    {
      scanner_next_token(scan);
      compile_expression(comp);
      return;
    }
    chunk_emit_opcode(chunk, OP_NULL);
    return;
  }
  if (MATCH(scan, TOKEN_LBRACKET))
  {
    scanner_next_token(scan);
    if (!MATCH(scan, TOKEN_NAME))
      fatal_error_unexpected_token(scan);
    define_local(comp, &scan->token);
    scanner_next_token(scan);
    uint8_t n = 1;
    while (MATCH(scan, TOKEN_COMMA))
    {
      scanner_next_token(scan);
      if (!MATCH(scan, TOKEN_NAME))
        fatal_error_unexpected_token(scan);
      define_local(comp, &scan->token);
      scanner_next_token(scan);
      ++n;
    }
    EXPECT(scan, TOKEN_RBRACKET);
    EXPECT(scan, TOKEN_EQ);
    compile_expression(comp);
    chunk_emit_opcode(chunk, OP_UNPACK);
    chunk_emit_byte(chunk, n);
    return;
  }
  fatal_error_unexpected_token(scan);
}

static void compile_assignment(compiler_t *comp, token_t tk)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  bool is_local;
  uint8_t index = resolve_variable(comp, &tk, &is_local);
  if (!is_local)
    fatal_error("cannot assign to immutable variable '%.*s'",
      tk.length, tk.start);
  if (MATCH(scan, TOKEN_EQ))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    chunk_emit_opcode(chunk, OP_SET_LOCAL);
    chunk_emit_byte(chunk, index);
    return;
  }
  chunk_emit_opcode(chunk, OP_GET_LOCAL);
  chunk_emit_byte(chunk, index);
  if (MATCH(scan, TOKEN_PLUSEQ))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    chunk_emit_opcode(chunk, OP_ADD);
    chunk_emit_opcode(chunk, OP_SET_LOCAL);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_MINUSEQ))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    chunk_emit_opcode(chunk, OP_SUBTRACT);
    chunk_emit_opcode(chunk, OP_SET_LOCAL);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_STAREQ))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    chunk_emit_opcode(chunk, OP_MULTIPLY);
    chunk_emit_opcode(chunk, OP_SET_LOCAL);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_SLASHEQ))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    chunk_emit_opcode(chunk, OP_DIVIDE);
    chunk_emit_opcode(chunk, OP_SET_LOCAL);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_PERCENTEQ))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    chunk_emit_opcode(chunk, OP_MODULO);
    chunk_emit_opcode(chunk, OP_SET_LOCAL);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_PLUSPLUS))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_INT);
    chunk_emit_word(chunk, 1);
    chunk_emit_opcode(chunk, OP_ADD);
    chunk_emit_opcode(chunk, OP_SET_LOCAL);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_MINUSMINUS))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_INT);
    chunk_emit_word(chunk, 1);
    chunk_emit_opcode(chunk, OP_SUBTRACT);
    chunk_emit_opcode(chunk, OP_SET_LOCAL);
    chunk_emit_byte(chunk, index);
    return;
  }
  int n = 0;
  while (MATCH(scan, TOKEN_LBRACKET))
  {
    scanner_next_token(scan);
    if (MATCH(scan, TOKEN_RBRACKET))
    {
      scanner_next_token(scan);
      EXPECT(scan, TOKEN_EQ);
      compile_expression(comp);
      chunk_emit_opcode(chunk, n ? OP_APPEND : OP_INPLACE_APPEND);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
    compile_expression(comp);
    EXPECT(scan, TOKEN_RBRACKET);
    if (MATCH(scan, TOKEN_EQ))
    {
      scanner_next_token(scan);
      compile_expression(comp);
      chunk_emit_opcode(chunk, n ? OP_PUT_ELEMENT : OP_INPLACE_PUT_ELEMENT);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
    chunk_emit_opcode(chunk, OP_FETCH_ELEMENT);
    ++n;
    if (MATCH(scan, TOKEN_PLUSEQ))
    {
      scanner_next_token(scan);
      compile_expression(comp);
      chunk_emit_opcode(chunk, OP_ADD);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
    if (MATCH(scan, TOKEN_MINUSEQ))
    {
      scanner_next_token(scan);
      compile_expression(comp);
      chunk_emit_opcode(chunk, OP_SUBTRACT);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
    if (MATCH(scan, TOKEN_STAREQ))
    {
      scanner_next_token(scan);
      compile_expression(comp);
      chunk_emit_opcode(chunk, OP_MULTIPLY);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
    if (MATCH(scan, TOKEN_SLASHEQ))
    {
      scanner_next_token(scan);
      compile_expression(comp);
      chunk_emit_opcode(chunk, OP_DIVIDE);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
    if (MATCH(scan, TOKEN_PERCENTEQ))
    {
      scanner_next_token(scan);
      compile_expression(comp);
      chunk_emit_opcode(chunk, OP_MODULO);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
    if (MATCH(scan, TOKEN_PLUSPLUS))
    {
      scanner_next_token(scan);
      chunk_emit_opcode(chunk, OP_INT);
      chunk_emit_word(chunk, 1);
      chunk_emit_opcode(chunk, OP_ADD);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
    if (MATCH(scan, TOKEN_MINUSMINUS))
    {
      scanner_next_token(scan);
      chunk_emit_opcode(chunk, OP_INT);
      chunk_emit_word(chunk, 1);
      chunk_emit_opcode(chunk, OP_SUBTRACT);
      for (int i = 0; i < n; ++i)
        chunk_emit_opcode(chunk, OP_SET_ELEMENT);
      chunk_emit_opcode(chunk, OP_SET_LOCAL);
      chunk_emit_byte(chunk, index);
      return;
    }
  }
  fatal_error_unexpected_token(scan);
}

static void compile_call_statement(compiler_t *comp, token_t tk)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  bool is_local;
  uint8_t index = resolve_variable(comp, &tk, &is_local);
  chunk_emit_opcode(chunk, is_local ? OP_GET_LOCAL : OP_GLOBAL);
  chunk_emit_byte(chunk, index);
  if (MATCH(scan, TOKEN_RPAREN))
  {
    scanner_next_token(scan);
    EXPECT(scan, TOKEN_SEMICOLON);
    chunk_emit_opcode(chunk, OP_CALL);
    chunk_emit_byte(chunk, 0);
    chunk_emit_opcode(chunk, OP_POP);
    return;
  }
  compile_expression(comp);
  uint8_t nargs = 1;
  while (MATCH(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    ++nargs;
  }
  EXPECT(scan, TOKEN_RPAREN);
  EXPECT(scan, TOKEN_SEMICOLON);
  chunk_emit_opcode(chunk, OP_CALL);
  chunk_emit_byte(chunk, nargs);
  chunk_emit_opcode(chunk, OP_POP);
}

static void compile_function_declaration(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  token_t tk;
  if (!MATCH(scan, TOKEN_NAME))
    fatal_error_unexpected_token(scan);
  tk = scan->token;
  scanner_next_token(scan);
  define_local(comp, &tk);
  compiler_t fn_comp;
  compiler_init(&fn_comp, scan, string_from_chars(tk.length, tk.start));
  array_t *consts = comp->fn->consts;
  uint8_t index = (uint8_t) consts->length;
  array_inplace_add_element(consts, FUNCTION_VALUE(fn_comp.fn));
  chunk_emit_opcode(chunk, OP_CONSTANT);
  chunk_emit_byte(chunk, index);
  EXPECT(scan, TOKEN_LPAREN);
  if (MATCH(scan, TOKEN_RPAREN))
  {
    scanner_next_token(scan);
    if (!MATCH(scan, TOKEN_LBRACE))
      fatal_error_unexpected_token(scan);
    compile_block(&fn_comp);
    chunk_t *fn_chunk = &fn_comp.fn->chunk;
    chunk_emit_opcode(fn_chunk, OP_NULL);
    chunk_emit_opcode(fn_chunk, OP_RETURN);
    return;
  }
  if (!MATCH(scan, TOKEN_NAME))
    fatal_error_unexpected_token(scan);
  tk = scan->token;
  scanner_next_token(scan);
  define_local(&fn_comp, &tk);
  int arity = 1;
  while (MATCH(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    if (!MATCH(scan, TOKEN_NAME))
      fatal_error_unexpected_token(scan);
    tk = scan->token;
    scanner_next_token(scan);
    define_local(&fn_comp, &tk);
    ++arity;
  }
  fn_comp.fn->arity = arity;
  EXPECT(scan, TOKEN_RPAREN);
  if (!MATCH(scan, TOKEN_LBRACE))
    fatal_error_unexpected_token(scan);
  compile_block(&fn_comp);
  chunk_t *fn_chunk = &fn_comp.fn->chunk;
  chunk_emit_opcode(fn_chunk, OP_NULL);
  chunk_emit_opcode(fn_chunk, OP_RETURN);
}

static void compile_delete_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  if (!MATCH(scan, TOKEN_NAME))
    fatal_error_unexpected_token(scan);
  token_t tk = scan->token;
  scanner_next_token(scan);
  bool is_local;
  uint8_t index = resolve_variable(comp, &tk, &is_local);
  if (!is_local)
    fatal_error("cannot delete element from immutable variable '%.*s'",
      tk.length, tk.start);
  chunk_emit_opcode(chunk, OP_GET_LOCAL);
  chunk_emit_byte(chunk, index);
  EXPECT(scan, TOKEN_LBRACKET);
  compile_expression(comp);
  EXPECT(scan, TOKEN_RBRACKET);
  int n = 0;
  while (MATCH(scan, TOKEN_LBRACKET))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_FETCH_ELEMENT);
    compile_expression(comp);
    EXPECT(scan, TOKEN_RBRACKET);
    ++n;
  }
  EXPECT(scan, TOKEN_SEMICOLON);
  chunk_emit_opcode(chunk, n ? OP_DELETE : OP_INPLACE_DELETE);
  for (int i = 0; i < n; ++i)
    chunk_emit_opcode(chunk, OP_SET_ELEMENT);
  chunk_emit_opcode(chunk, OP_SET_LOCAL);
  chunk_emit_byte(chunk, index);
}

static void compile_if_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  EXPECT(scan, TOKEN_LPAREN);
  compile_expression(comp);
  EXPECT(scan, TOKEN_RPAREN);
  int offset1 = emit_jump(chunk, OP_JUMP_IF_FALSE);
  chunk_emit_opcode(chunk, OP_POP);
  compile_statement(comp);
  int offset2 = emit_jump(chunk, OP_JUMP);
  patch_jump(chunk, offset1);
  chunk_emit_opcode(chunk, OP_POP);
  if (MATCH(scan, TOKEN_ELSE))
  {
    scanner_next_token(scan);
    compile_statement(comp);
  }
  patch_jump(chunk, offset2);
}

static void compile_loop_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  if (!MATCH(scan, TOKEN_LBRACE))
    fatal_error_unexpected_token(scan);
  loop_t loop;
  start_loop(comp, &loop);
  compile_block(comp);
  chunk_emit_opcode(chunk, OP_JUMP);
  chunk_emit_word(chunk, loop.jump);
  end_loop(comp);
}

static void compile_while_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  EXPECT(scan, TOKEN_LPAREN);
  loop_t loop;
  start_loop(comp, &loop);
  compile_expression(comp);
  EXPECT(scan, TOKEN_RPAREN);
  int offset = emit_jump(chunk, OP_JUMP_IF_FALSE);
  chunk_emit_opcode(chunk, OP_POP);
  compile_statement(comp);
  chunk_emit_opcode(chunk, OP_JUMP);
  chunk_emit_word(chunk, loop.jump);
  patch_jump(chunk, offset);
  chunk_emit_opcode(chunk, OP_POP);
  end_loop(comp);
}

static void compile_do_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  loop_t loop;
  start_loop(comp, &loop);
  compile_statement(comp);
  EXPECT(scan, TOKEN_WHILE);
  EXPECT(scan, TOKEN_LPAREN);
  compile_expression(comp);
  EXPECT(scan, TOKEN_RPAREN);
  EXPECT(scan, TOKEN_SEMICOLON);
  int offset = emit_jump(chunk, OP_JUMP_IF_FALSE);
  chunk_emit_opcode(chunk, OP_POP);
  chunk_emit_opcode(chunk, OP_JUMP);
  chunk_emit_word(chunk, loop.jump);
  patch_jump(chunk, offset);
  chunk_emit_opcode(chunk, OP_POP);
  end_loop(comp);
}

static void compile_for_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  EXPECT(scan, TOKEN_LPAREN);
  push_scope(comp);
  if (MATCH(scan, TOKEN_SEMICOLON))
    scanner_next_token(scan);
  else
  {
    if (MATCH(scan, TOKEN_VAR))
    {
      compile_variable_declaration(comp);
      EXPECT(scan, TOKEN_SEMICOLON);
    }
    else if (MATCH(scan, TOKEN_NAME))
    {
      token_t tk = scan->token;
      scanner_next_token(scan);
      compile_assignment(comp, tk);
      EXPECT(scan, TOKEN_SEMICOLON);
    }
    else
      fatal_error_unexpected_token(scan);
  }
  uint16_t jump1 = chunk->length;
  if (MATCH(scan, TOKEN_SEMICOLON))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_TRUE);
  }
  else
  {
    compile_expression(comp);
    EXPECT(scan, TOKEN_SEMICOLON);
  }
  int offset1 = emit_jump(chunk, OP_JUMP_IF_FALSE);
  chunk_emit_opcode(chunk, OP_POP);
  int offset2 = emit_jump(chunk, OP_JUMP);
  uint16_t jump2 = chunk->length;
  loop_t loop;
  start_loop(comp, &loop);
  if (MATCH(scan, TOKEN_RPAREN))
    scanner_next_token(scan);
  else
  {
    if (!MATCH(scan, TOKEN_NAME))
      fatal_error_unexpected_token(scan);
    token_t tk = scan->token;
    scanner_next_token(scan);
    compile_assignment(comp, tk);
    EXPECT(scan, TOKEN_RPAREN);
  }
  chunk_emit_opcode(chunk, OP_JUMP);
  chunk_emit_word(chunk, jump1);  
  patch_jump(chunk, offset2);
  compile_statement(comp);
  chunk_emit_opcode(chunk, OP_JUMP);
  chunk_emit_word(chunk, jump2);
  patch_jump(chunk, offset1);
  chunk_emit_opcode(chunk, OP_POP);
  end_loop(comp);
  pop_scope(comp);
}

static void compile_continue_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  if (!comp->loop)
    fatal_error("cannot use 'continue' outside of a loop");
  EXPECT(scan, TOKEN_SEMICOLON);
  discard_locals(comp, comp->loop->scope_depth + 1);
  chunk_emit_opcode(chunk, OP_JUMP);
  chunk_emit_word(chunk, comp->loop->jump);
}

static void compile_break_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  scanner_next_token(scan);
  if (!comp->loop)
    fatal_error("cannot use 'break' outside of a loop");
  EXPECT(scan, TOKEN_SEMICOLON);
  discard_locals(comp, comp->loop->scope_depth + 1);
  add_break(comp);
}

static void compile_return_statement(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  if (MATCH(scan, TOKEN_SEMICOLON))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_NULL);
    chunk_emit_opcode(chunk, OP_RETURN);
    return;
  }
  compile_expression(comp);
  EXPECT(scan, TOKEN_SEMICOLON);
  chunk_emit_opcode(chunk, OP_RETURN);
}

static void compile_expression(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  compile_and_expression(comp);
  while (MATCH(scan, TOKEN_PIPEPIPE))
  {
    scanner_next_token(scan);
    int offset = emit_jump(chunk, OP_JUMP_IF_TRUE);
    chunk_emit_opcode(chunk, OP_POP);
    compile_and_expression(comp);
    patch_jump(chunk, offset);
  }
}

static void compile_and_expression(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  compile_equal_expression(comp);
  while (MATCH(scan, TOKEN_AMPAMP))
  {
    scanner_next_token(scan);
    int offset = emit_jump(chunk, OP_JUMP_IF_FALSE);
    chunk_emit_opcode(chunk, OP_POP);
    compile_equal_expression(comp);
    patch_jump(chunk, offset);
  }
}

static void compile_equal_expression(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  compile_comp_expression(comp);
  for (;;)
  {
    if (MATCH(scan, TOKEN_EQEQ))
    {
      scanner_next_token(scan);
      compile_comp_expression(comp);
      chunk_emit_opcode(chunk, OP_EQUAL);
      continue;
    }
    if (MATCH(scan, TOKEN_BANGEQ))
    {
      scanner_next_token(scan);
      compile_comp_expression(comp);
      chunk_emit_opcode(chunk, OP_EQUAL);
      chunk_emit_opcode(chunk, OP_NOT);
      continue;
    }
    break;
  }
}

static void compile_comp_expression(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  compile_add_expression(comp);
  for (;;)
  {
    if (MATCH(scan, TOKEN_GT))
    {
      scanner_next_token(scan);
      compile_add_expression(comp);
      chunk_emit_opcode(chunk, OP_GREATER);
      continue;
    }
    if (MATCH(scan, TOKEN_GTEQ))
    {
      scanner_next_token(scan);
      compile_add_expression(comp);
      chunk_emit_opcode(chunk, OP_LESS);
      chunk_emit_opcode(chunk, OP_NOT);
      continue;
    }
    if (MATCH(scan, TOKEN_LT))
    {
      scanner_next_token(scan);
      compile_add_expression(comp);
      chunk_emit_opcode(chunk, OP_LESS);
      continue;
    }
    if (MATCH(scan, TOKEN_LTEQ))
    {
      scanner_next_token(scan);
      compile_add_expression(comp);
      chunk_emit_opcode(chunk, OP_GREATER);
      chunk_emit_opcode(chunk, OP_NOT);
      continue;
    }
    break;
  }
}

static void compile_add_expression(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  compile_mul_expression(comp);
  for (;;)
  {
    if (MATCH(scan, TOKEN_PLUS))
    {
      scanner_next_token(scan);
      compile_mul_expression(comp);
      chunk_emit_opcode(chunk, OP_ADD);
      continue;
    }
    if (MATCH(scan, TOKEN_MINUS))
    {
      scanner_next_token(scan);
      compile_mul_expression(comp);
      chunk_emit_opcode(chunk, OP_SUBTRACT);
      continue;
    }
    break;
  }
}

static void compile_mul_expression(compiler_t *comp)
{
  compile_unary_expression(comp);
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  for (;;)
  {
    if (MATCH(scan, TOKEN_STAR))
    {
      scanner_next_token(scan);
      compile_unary_expression(comp);
      chunk_emit_opcode(chunk, OP_MULTIPLY);
      continue;
    }
    if (MATCH(scan, TOKEN_SLASH))
    {
      scanner_next_token(scan);
      compile_unary_expression(comp);
      chunk_emit_opcode(chunk, OP_DIVIDE);
      continue;
    }
    if (MATCH(scan, TOKEN_PERCENT))
    {
      scanner_next_token(scan);
      compile_unary_expression(comp);
      chunk_emit_opcode(chunk, OP_MODULO);
      continue;
    }
    break;
  }
}

static void compile_unary_expression(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  if (MATCH(scan, TOKEN_PLUS))
  {
    scanner_next_token(scan);
    compile_unary_expression(comp);
    return;
  }
  if (MATCH(scan, TOKEN_MINUS))
  {
    scanner_next_token(scan);
    compile_unary_expression(comp);
    chunk_emit_opcode(chunk, OP_NEGATE);
    return;
  }
  if (MATCH(scan, TOKEN_BANG))
  {
    scanner_next_token(scan);
    compile_unary_expression(comp);
    chunk_emit_opcode(chunk, OP_NOT);
    return;
  }
  compile_prim_expression(comp);
}

static void compile_prim_expression(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  array_t *consts = comp->fn->consts;
  if (MATCH(scan, TOKEN_NULL))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_NULL);
    return;
  }
  if (MATCH(scan, TOKEN_FALSE))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_FALSE);
    return;
  }
  if (MATCH(scan, TOKEN_TRUE))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_TRUE);
    return;
  }
  if (MATCH(scan, TOKEN_INT))
  {
    long data = parse_long(scan->token.start);
    scanner_next_token(scan);
    if (data <= UINT16_MAX)
    {
      chunk_emit_opcode(chunk, OP_INT);
      chunk_emit_word(chunk, (uint16_t) data);
      return;
    }
    uint8_t index = (uint8_t) consts->length;
    array_inplace_add_element(consts, NUMBER_VALUE(data));
    chunk_emit_opcode(chunk, OP_CONSTANT);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_FLOAT))
  {
    double data = parse_double(scan->token.start);
    scanner_next_token(scan);
    uint8_t index = (uint8_t) consts->length;
    array_inplace_add_element(consts, NUMBER_VALUE(data));
    chunk_emit_opcode(chunk, OP_CONSTANT);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_STRING))
  {
    token_t *tk = &scan->token;
    string_t *str = string_from_chars(tk->length, tk->start);
    scanner_next_token(scan);
    uint8_t index = (uint8_t) consts->length;
    array_inplace_add_element(consts, STRING_VALUE(str));
    chunk_emit_opcode(chunk, OP_CONSTANT);
    chunk_emit_byte(chunk, index);
    return;
  }
  if (MATCH(scan, TOKEN_LBRACKET))
  {
    compile_array_constructor(comp);
    return;
  }
  if (MATCH(scan, TOKEN_FN))
  {
    compile_anonymous_function(comp);
    return;
  }
  if (MATCH(scan, TOKEN_NAME))
  {
    compile_subscript_or_call(comp);
    return;
  }
  if (MATCH(scan, TOKEN_LPAREN))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    EXPECT(scan, TOKEN_RPAREN);
    return;
  }
  fatal_error_unexpected_token(scan);
}

static void compile_array_constructor(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  if (MATCH(scan, TOKEN_RBRACKET))
  {
    scanner_next_token(scan);
    chunk_emit_opcode(chunk, OP_ARRAY);
    chunk_emit_byte(chunk, 0);
    return;
  }
  compile_expression(comp);
  uint8_t length = 1;
  while (MATCH(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    compile_expression(comp);
    ++length;
  }
  EXPECT(scan, TOKEN_RBRACKET);
  chunk_emit_opcode(chunk, OP_ARRAY);
  chunk_emit_byte(chunk, length);
  return;
}

static void compile_anonymous_function(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  scanner_next_token(scan);
  compiler_t fn_comp;
  compiler_init(&fn_comp, scan, string_from_chars(-1, "anonymous"));
  array_t *consts = comp->fn->consts;
  uint8_t index = (uint8_t) consts->length;
  array_inplace_add_element(consts, FUNCTION_VALUE(fn_comp.fn));
  chunk_emit_opcode(chunk, OP_CONSTANT);
  chunk_emit_byte(chunk, index);
  EXPECT(scan, TOKEN_LPAREN);
  if (MATCH(scan, TOKEN_RPAREN))
  {
    scanner_next_token(scan);
    if (!MATCH(scan, TOKEN_LBRACE))
      fatal_error_unexpected_token(scan);
    compile_block(&fn_comp);
    chunk_t *fn_chunk = &fn_comp.fn->chunk;
    chunk_emit_opcode(fn_chunk, OP_NULL);
    chunk_emit_opcode(fn_chunk, OP_RETURN);
    return;
  }
  token_t tk;
  if (!MATCH(scan, TOKEN_NAME))
    fatal_error_unexpected_token(scan);
  tk = scan->token;
  scanner_next_token(scan);
  define_local(&fn_comp, &tk);
  int arity = 1;
  while (MATCH(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    if (!MATCH(scan, TOKEN_NAME))
      fatal_error_unexpected_token(scan);
    tk = scan->token;
    scanner_next_token(scan);
    define_local(&fn_comp, &tk);
    ++arity;
  }
  fn_comp.fn->arity = arity;
  EXPECT(scan, TOKEN_RPAREN);
  if (!MATCH(scan, TOKEN_LBRACE))
    fatal_error_unexpected_token(scan);
  compile_block(&fn_comp);
  chunk_t *fn_chunk = &fn_comp.fn->chunk;
  chunk_emit_opcode(fn_chunk, OP_NULL);
  chunk_emit_opcode(fn_chunk, OP_RETURN);
}

static void compile_subscript_or_call(compiler_t *comp)
{
  scanner_t *scan = comp->scan;
  chunk_t *chunk = &comp->fn->chunk;
  bool is_local;
  uint8_t index = resolve_variable(comp, &scan->token, &is_local);
  scanner_next_token(scan);
  chunk_emit_opcode(chunk, is_local ? OP_GET_LOCAL : OP_GLOBAL);
  chunk_emit_byte(chunk, index);
  for (;;)
  {
    if (MATCH(scan, TOKEN_LBRACKET))
    {
      scanner_next_token(scan);
      compile_expression(comp);
      EXPECT(scan, TOKEN_RBRACKET);
      chunk_emit_opcode(chunk, OP_GET_ELEMENT);
      continue;
    }
    if (MATCH(scan, TOKEN_LPAREN))
    {
      scanner_next_token(scan);
      if (MATCH(scan, TOKEN_RPAREN))
      {
        scanner_next_token(scan);
        chunk_emit_opcode(chunk, OP_CALL);
        chunk_emit_byte(chunk, 0);
        return;
      }
      compile_expression(comp);
      uint8_t nargs = 1;
      while (MATCH(scan, TOKEN_COMMA))
      {
        scanner_next_token(scan);
        compile_expression(comp);
        ++nargs;
      }
      EXPECT(scan, TOKEN_RPAREN);
      chunk_emit_opcode(chunk, OP_CALL);
      chunk_emit_byte(chunk, nargs);
      continue;
    }
    break;
  }
}

static void compiler_init(compiler_t *comp, scanner_t *scan, string_t *name)
{
  comp->scan = scan;
  comp->scope_depth = -1;
  comp->num_locals = 0;
  comp->loop = NULL;
  comp->fn = function_new(name, 0);
  add_local(comp, name->length, name->chars);
}

function_t *compile(scanner_t *scan)
{
  compiler_t comp;
  compiler_init(&comp, scan, string_from_chars(-1, "main"));
  while (!MATCH(comp.scan, TOKEN_EOF))
    compile_statement(&comp);
  chunk_t *chunk = &comp.fn->chunk;
  chunk_emit_opcode(chunk, OP_NULL);
  chunk_emit_opcode(chunk, OP_RETURN);
  return comp.fn;
}
