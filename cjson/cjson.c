#include "cjson.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef CJSON_ENABLE_TIMER
#define TIMER_INIT() clock_t timer_start, timer_end; const char* timer_function;
#define TIMER_BEGIN(fn) timer_start = clock(); timer_function = #fn;
#define TIMER_END() timer_end = clock(); printf("Timer %s took %f seconds to run.\n", timer_function, ((double)(timer_end - timer_start) / CLOCKS_PER_SEC));
#else
#define TIMER_INIT()
#define TIMER_BEGIN(fn)
#define TIMER_END()
#endif

cjson_settings* global_settings = 0;

typedef enum {
	initial_state,
	in_array,
	in_object,
} cjson_state_type;

typedef enum {
	parse_flag_after_value = 1 << 1,
	parse_flag_expecting_valuetype = 1 << 2,
} cjson_parse_flags;

typedef struct __cjson_state {
	struct __cjson_state* prev;
	struct __cjson_state* next;
	cjson_value* wip_value;
	cjson_state_type type;
	int parse_flags;
} cjson_state;



typedef struct __cjson_context {
	cjson_state* root_state;
	cjson_state* tail_state;
	cjson_settings* settings;
	const char* buf;
	size_t len;
	cjson_pos* pos;
} cjson_context;

cjson_value* cjson_parse_ex(cjson_settings* settings, const char* buffer);
cjson_value* cjson_parse_file_ex(cjson_settings* settings, const char* filename);

cjson_value* cjson_end(cjson_value* parent)
{
	cjson_value* child = parent->child;
	if (parent->childtail) {
		child = parent->childtail;
	}

	while (child != NULL) {
		if (child->next == NULL) {
			break;
		}
		child = child->next;
	}

	parent->childtail = child;
	return child;
}

int cjson_error_code()
{
	return global_settings->errc;
}

const char* cjson_error_string()
{
	switch (global_settings->errc) {
		case cjson_error_code_ok: return "ok";
		case cjson_error_code_oom: return "out of memory (increase settings->memory_limit)";
		case cjson_error_code_alloc: return "allocation failure (settings->mem_alloc() returned NULL)";
		case cjson_error_code_syntax_unexpected_eof: return "Syntax error: Unexpected end of file";
		case cjson_error_code_syntax_multiple_root_nodes: return "Syntax error: Multiple root values (i.e. attempting to parse '[1, 2][3]')";
		case cjson_error_code_syntax_invalid_number: return "Syntax error: Invalid number encountered (i.e. invalid punctuation, or too many negative signs)";
		case cjson_error_code_syntax_unexpected_comma: return "Syntax error: Unexpected comma in array or object";
		case cjson_error_code_syntax_expected_key: return "Syntax error: Expected key (string) in object";
		case cjson_error_code_syntax_expected_colon: return "Syntax error: Expected colon after key";
	}
}

void cjson_print_mem(void)
{
#ifdef CJSON_ENABLE_MEMORY_LOGGING
	printf("CJSON Memory stats: highest mem=%ld, in use right now=%ld\n", global_settings->highest_memory_usage, global_settings->used_memory);
#endif
}

void cjson_init(cjson_settings* settings) 
{
	if (!settings) {
		global_settings = malloc(sizeof(cjson_settings));
		global_settings->mem_alloc = &malloc;
		global_settings->mem_free = &free;
#ifdef CJSON_ENABLE_MULTITHREAD_SUPPORT
		global_settings->mtx = NULL;
		global_settings->multithreaded = 0;
#endif
#ifdef CJSON_ENABLE_MULTITHREAD_SUPPORT
		global_settings->memory_limit = INT_MAX;
		global_settings->used_memory = 0;
		global_settings->highest_memory_usage = 0;
#endif
		global_settings->errc = cjson_error_code_ok;
	}
	else {
		global_settings = malloc(sizeof(cjson_settings));
		global_settings->mem_alloc = settings->mem_alloc;
		global_settings->mem_free = settings->mem_free;
#ifdef CJSON_ENABLE_MULTITHREAD_SUPPORT
		global_settings->mtx = settings->mtx;
		global_settings->multithreaded = settings->multithreaded;
#endif
#ifdef CJSON_ENABLE_MEMORY_LOGGING
		global_settings->memory_limit = settings->memory_limit;
		global_settings->used_memory = 0;
		global_settings->highest_memory_usage = 0;
#endif
		global_settings->errc = cjson_error_code_ok;
	}
}

void cjson_shutdown()
{
	free(global_settings);
}

void* cjson_alloc(cjson_settings* settings, size_t size)
{
#ifdef CJSON_ENABLE_MEMORY_LOGGING
	if (settings->used_memory + size + sizeof(size_t) > settings->memory_limit) {
		settings->errc = cjson_error_code_oom;
		return NULL; // Not allowed to allocate more.
	}

	void* cjson_ptr = settings->mem_alloc(size + sizeof(size_t));

	if (!cjson_ptr) {
		settings->errc = cjson_error_code_alloc;
		return NULL;
	}

	*(size_t*)cjson_ptr = size;

	// Increase memory usage
	settings->used_memory += size + sizeof(size_t);
	if (settings->used_memory > settings->highest_memory_usage) {
		settings->highest_memory_usage = settings->used_memory;
	}

	return (void*)((size_t)cjson_ptr + sizeof(size_t));
#else
	void* ptr = settings->mem_alloc(size);
	if (!ptr) {
		settings->errc = cjson_error_code_alloc;
	}
	return ptr;
#endif
}

void cjson_free(cjson_settings* settings, void* ptr)
{
	if (ptr) {

#ifdef CJSON_ENABLE_MEMORY_LOGGING
		size_t* cjson_ptr = (size_t*)((size_t)ptr - sizeof(size_t));
		
		// Decrease used memory
		settings->used_memory -= (*cjson_ptr) + sizeof(size_t);
		
		settings->mem_free(cjson_ptr);
#else
		settings->mem_free(ptr);
#endif
	}
}

// Returns NULL on failure, if nonnull return then the ptr must be freed with cjson_free
char* cjson_read_file(cjson_settings* settings, const char* filename)
{
	FILE* file = fopen(filename, "r");

	if (!file) {
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	size_t len = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* buf = cjson_alloc(settings, len + 1);
	if (!buf) {
		fclose(file);
		return NULL;
	}

	if (fread(buf, sizeof(char), len, file) != len) {
		cjson_free(settings, buf);
		fclose(file);
		return NULL;
	}

	fclose(file);
	buf[len] = 0;
	return buf;
}

cjson_value* cjson_parse_file(const char* filename)
{
	if (!global_settings) {
		cjson_init(NULL);
	}

	return cjson_parse_file_ex(global_settings, filename);
}

cjson_value* cjson_parse(const char* buffer)
{
	if (!global_settings) {
		cjson_init(NULL);
	}

	return cjson_parse_ex(global_settings, buffer);
}

cjson_value* cjson_parse_file_ex(cjson_settings* settings, const char* filename)
{
	TIMER_INIT();

	TIMER_BEGIN("cjson_read_file");
	char* buf = cjson_read_file(settings, filename);
	TIMER_END();
	if (!buf) {
		return NULL;
	}
	
	cjson_value* value = cjson_parse_ex(settings, buf);
	cjson_free(settings, buf);
	return value;
}

cjson_pos* cjson_make_pos(cjson_settings* settings, size_t row, size_t col, size_t ofs)
{
	cjson_pos* pos = cjson_alloc(settings, sizeof(cjson_pos));
	if (!pos) {
		return NULL;
	}
	pos->row = row;
	pos->col = col;
	pos->ofs = ofs;
	return pos;
}

cjson_state* cjson_push_state(cjson_context* ctx, cjson_state_type type, cjson_value* wip, int parse_flags)
{
	cjson_state* state = cjson_alloc(ctx->settings, sizeof(cjson_state));
	if (!state) {
		return NULL;
	}
	state->next = NULL;
	state->prev = NULL;
	state->type = type;
	state->wip_value = wip;
	state->parse_flags = parse_flags;

	if (ctx->tail_state == NULL) {
		ctx->root_state = state;
		ctx->tail_state = state;
	}
	else {
		state->prev = ctx->tail_state;
		ctx->tail_state->next = state;
		ctx->tail_state = state;
	}

	return state;
}

void cjson_pop_state(cjson_context* ctx)
{
	cjson_state* old_tail = ctx->tail_state;
	ctx->tail_state = old_tail->prev;

	if (ctx->tail_state) {
		ctx->tail_state->next = NULL;
	}
	cjson_free(ctx->settings, old_tail);
}

void cjson_free_remaining_states(cjson_context* ctx)
{
	while (ctx->tail_state) {
		cjson_pop_state(ctx);
	}
}

int cjson_eof(cjson_context* ctx) 
{
	return ctx->pos->ofs >= ctx->len;
}

char cjson_peek(cjson_context* ctx, int offset)
{
	if (ctx->pos->ofs + offset > ctx->len) {
		return 0;
	}

	return ctx->buf[ctx->pos->ofs + offset];
}

char cjson_curc(cjson_context* ctx) 
{
	return cjson_peek(ctx, 0);
}

char cjson_consume(cjson_context* ctx)
{
	if (cjson_eof(ctx)) {
		return 0;
	}

	char c = cjson_curc(ctx);

	++ctx->pos->col;
	++ctx->pos->ofs;
	if (c == '\n') {
		++ctx->pos->row;
		ctx->pos->col = 0;
	}

	return c;
}

char* cjson_consume_digits(cjson_context* ctx, int* punctflag) // 1.23 etc
{
	int num_dots = 0;
	int is_neg = 0;
	size_t s_ofs = ctx->pos->ofs;

	if (cjson_curc(ctx) == '-') {
		is_neg = 1;
	}

	while (!cjson_eof(ctx) && (isdigit(cjson_curc(ctx)) || cjson_curc(ctx) == '.' || cjson_curc(ctx) == '-')) {
		char c = cjson_curc(ctx);
		if (c == '.') {
			++num_dots;
			if (punctflag) *punctflag = 1;

			if (num_dots > 1) {
				ctx->settings->errc = cjson_error_code_syntax_invalid_number;
				return NULL;
			}
		}

		if (is_neg && c == '-' && ctx->pos->ofs > s_ofs) {
			// error
			ctx->settings->errc = cjson_error_code_syntax_invalid_number;
			return NULL;
		}

		cjson_consume(ctx);
	}
	size_t e_ofs = ctx->pos->ofs;
	
	size_t len = e_ofs - s_ofs;
	char* buf = cjson_alloc(ctx->settings, len + 1);
	if (!buf) {
		return NULL;
	}
	memcpy(buf, ctx->buf + s_ofs, len);
	buf[len] = 0;
	return buf;
}

char* cjson_consume_ident(cjson_context* ctx) // null, true, false
{
	size_t s_ofs = ctx->pos->ofs;
	while (!cjson_eof(ctx) && isalnum(cjson_curc(ctx))) {
		cjson_consume(ctx);
	}
	size_t e_ofs = ctx->pos->ofs;

	size_t len = e_ofs - s_ofs;
	char* buf = cjson_alloc(ctx->settings, len + 1);
	if (!buf) {
		return NULL;
	}

	memcpy(buf, ctx->buf + s_ofs, len);
	buf[len] = 0;
	return buf;
}

// TODO: Take cjson_pos** as out parameter
char* cjson_consume_str(cjson_context* ctx) // "string"
{
	if (cjson_curc(ctx) != '"') {
		return NULL;
	}
	cjson_consume(ctx); // "

	size_t s_ofs = ctx->pos->ofs;
	while (!cjson_eof(ctx)) {
		char c = cjson_curc(ctx);

		if (c == '\\') {
			// next character is escaped as wel
			cjson_consume(ctx);
			cjson_consume(ctx);
		}
		else if (c == '"') {
			break; // Break out here.
		}
		else {
			cjson_consume(ctx);
		}
	}
	size_t e_ofs = ctx->pos->ofs;

	if (cjson_eof(ctx)) {
		ctx->settings->errc = cjson_error_code_syntax_unexpected_eof;
		return NULL;
	}
	cjson_consume(ctx); // "

	// TODO: escape!
	size_t len = e_ofs - s_ofs;
	char* buf = cjson_alloc(ctx->settings, len + 1);
	memcpy(buf, ctx->buf + s_ofs, len);
	buf[len] = 0;

	return buf;
}

int cjson_consume_comments(cjson_context* ctx) 
{
	if (cjson_curc(ctx) == '/' && cjson_peek(ctx, 1) == '/') {
		while (!cjson_eof(ctx) && cjson_curc(ctx) != '\n') {
			cjson_consume(ctx);
		}

		cjson_consume(ctx); // consume final newline
		return 1;
	}
	
	if (cjson_curc(ctx) == '/' && cjson_peek(ctx, 1) == '*') {
		while (!cjson_eof(ctx) && cjson_curc(ctx) != '*' && cjson_peek(ctx, 1) != '/') {
			cjson_consume(ctx);
		}

		// Remove trailing */
		if (cjson_curc(ctx) == '*' && cjson_peek(ctx, 1) == '/') {
			cjson_consume(ctx);
			cjson_consume(ctx);
		}

		return 1;
	}

	return 0;
}

void cjson_consume_spaces(cjson_context* ctx) {
	while (!cjson_eof(ctx) && isspace(cjson_curc(ctx))) {
		cjson_consume(ctx);
	}
}

cjson_value* cjson_value_create(cjson_settings* settings)
{
	cjson_value* value = cjson_alloc(settings, sizeof(cjson_value));
	if (!value) {
		return NULL;
	}

	value->prev = NULL;
	value->next = NULL;
	value->child = NULL;
	value->childtail = NULL;
	value->flags = cjson_invalid;
	value->string = NULL;
	value->doubleval = 0;
	value->intval = 0;

	return value;
}

void cjson_free_value(cjson_value *v)
{
	if (v) {
		cjson_value* next = v->next;
		while (next != NULL) {
			cjson_value* tmp = next;
			next = next->next;

			tmp->next = NULL;
			cjson_free_value(tmp);
		}

		cjson_free_value(v->child);
		cjson_free(global_settings, v->string);
		cjson_free(global_settings, v);
	}
}

int stricmp(const char* a, const char* b)
{
	for (;; a++, b++) {
		int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
		if (d != 0 || !*a)
			return d;
	}

}

int cjson_partial_parse(cjson_context* ctx, cjson_value** out)
{
	if (!out) {
		return 0;
	}
	
	const char *reason = 0;
	int was_allocated = 0;
	if (!*out) {
		*out = cjson_value_create(ctx->settings);
		was_allocated = 1;
	}

	if (!*out) {
		fprintf(stderr, "Panic! memory failure\n");
		exit(1);
	}

	char c = cjson_curc(ctx);

	if (c == '{') {
		(*out)->flags = cjson_object;
		cjson_consume(ctx);
	}
	else if (c == '[') {
		(*out)->flags = cjson_array;
		cjson_consume(ctx);
	}
	else if (c == '"') {
		(*out)->flags = cjson_string;
		(*out)->string = cjson_consume_str(ctx);

		if (!(*out)->string) {
			reason = "string parse failed";
			goto error;
		}
	}
	else if (isdigit(c)) {
		(*out)->flags = cjson_number;
		int punctflag = 0;
		char* buf = cjson_consume_digits(ctx, &punctflag);

		if (!buf) {
			reason = "digits parse failed";
			goto error;
		}

		if (punctflag) {
			(*out)->flags |= cjson_double;
			(*out)->doubleval = strtod(buf, NULL);
		}
		else {
			(*out)->flags |= cjson_integer;
			(*out)->intval = strtol(buf, NULL, 0);
		}

		cjson_free(ctx->settings, buf);
	}
	else if (isalnum(c)) {
		char* buf = cjson_consume_ident(ctx);

		if (!buf) {
			reason = "identifier parse failed";
			goto error;
		}

		if (stricmp(buf, "null") == 0) {
			(*out)->flags = cjson_null;
		}
		else if (stricmp(buf, "true") == 0 || stricmp(buf, "false") == 0) {
			(*out)->flags = cjson_boolean;
			(*out)->intval = tolower(buf[0]) == 't';
		}
		else {
			printf("could not find anything from ident: %s\n", buf);
			cjson_free(ctx->settings, buf);
			goto error;
		}

		cjson_free(ctx->settings, buf);
	}
	else {
		reason = "Unknown character";
		goto error;
	}

	return 1;

	error:
	printf("partial parse failed with reason '%s' for character '%c'\n", reason, c);
	cjson_free(ctx->settings, *out);
	*out = NULL;
	return 0;
}

cjson_value* cjson_parse_impl(cjson_context* ctx)
{
	while (1) {
		// Whitespace and comments
		do {
			cjson_consume_spaces(ctx);
		} while(cjson_consume_comments(ctx));

		if (cjson_eof(ctx)) {
			break;
		}

		char c = cjson_curc(ctx);

		switch (ctx->tail_state->type) {
			case initial_state:
			{
				// Has a root node already been discovered?
				if (ctx->root_state->wip_value != NULL) {
					// TODO dealloc
					ctx->settings->errc = cjson_error_code_syntax_multiple_root_nodes;
					return NULL;
				}
				if (!cjson_partial_parse(ctx, &ctx->root_state->wip_value)) {
					return NULL;
				}

				if (!ctx->root_state->wip_value) {
					return NULL;
				}

				if (cjson_is_array(ctx->root_state->wip_value)) {
					cjson_push_state(ctx, in_array, ctx->root_state->wip_value, parse_flag_expecting_valuetype);
				}
				else if (cjson_is_object(ctx->root_state->wip_value)) {
					cjson_push_state(ctx, in_object, ctx->root_state->wip_value, parse_flag_expecting_valuetype);
				}
			}
			break;
			case in_array:
			{
				cjson_state* state = ctx->tail_state;

				if (c == ',') {
					if (state->parse_flags & parse_flag_after_value) {
						cjson_consume(ctx);
						state->parse_flags &= ~parse_flag_after_value;
						state->parse_flags |= parse_flag_expecting_valuetype;
						continue;
					}
					else {
						ctx->settings->errc = cjson_error_code_syntax_unexpected_comma;
						return NULL; // error
					}
				}

				if (c == ']') {
					// End of array.
					cjson_consume(ctx);
					cjson_pop_state(ctx);
					continue;
				}

				// Otherwise we are expecting a value.
				cjson_value* child = NULL;
				if (!cjson_partial_parse(ctx, &child)) {
					return NULL;
				}
				cjson_append(state->wip_value, child);
				
				if (cjson_is_array(child)) {
					cjson_push_state(ctx, in_array, child, parse_flag_expecting_valuetype);
				}
				else if (cjson_is_object(child)) {
					cjson_push_state(ctx, in_object, child, parse_flag_expecting_valuetype);
				}

				// Remove expecting value type and add after value parse flags
				state->parse_flags &= ~parse_flag_expecting_valuetype;
				state->parse_flags |= parse_flag_after_value;
			}
			break;
			case in_object:
			{
				cjson_state* state = ctx->tail_state;

				if (c == ',') {
					if (state->parse_flags & parse_flag_after_value) {
						cjson_consume(ctx);
						state->parse_flags &= ~parse_flag_after_value;
						state->parse_flags |= parse_flag_expecting_valuetype;
						continue;
					}
					else {
						ctx->settings->errc = cjson_error_code_syntax_unexpected_comma;
						return NULL; // error
					}
				}

				if (c == '}') {
					// End of array.
					cjson_consume(ctx);
					cjson_pop_state(ctx);
					continue;
				}

				if (c != '"') {
					ctx->settings->errc = cjson_error_code_syntax_expected_key;
					return NULL;
				}

				// Key
				char* key = cjson_consume_str(ctx);
				if (!key) {
					return NULL;
				}

				cjson_consume_spaces(ctx); // consume ws
				if (cjson_curc(ctx) != ':') {
					ctx->settings->errc = cjson_error_code_syntax_expected_colon;
					cjson_free(ctx->settings, key);
					return NULL;
				}
				cjson_consume(ctx);
				cjson_consume_spaces(ctx); // consume ws

				cjson_value* val = 0;
				if (!cjson_partial_parse(ctx, &val)) {
					cjson_free(ctx->settings, key);
					return NULL;
				}

				cjson_insert(state->wip_value, key, val);
				cjson_free(ctx->settings, key);

				if (cjson_is_array(val)) {
					cjson_push_state(ctx, in_array, val, parse_flag_expecting_valuetype);
				}
				else if (cjson_is_object(val)) {
					cjson_push_state(ctx, in_object, val, parse_flag_expecting_valuetype);
				}

				// Remove expecting value type and add after value parse flags
				state->parse_flags &= ~parse_flag_expecting_valuetype;
				state->parse_flags |= parse_flag_after_value;
			}
			break;
		}
	}

	return ctx->root_state != NULL ?
		ctx->root_state->wip_value : NULL;
}

cjson_value* cjson_parse_ex(cjson_settings* settings, const char* buffer)
{
	TIMER_INIT();

	if (!settings) {
		return NULL;
	}
	if (!buffer) {
		return NULL;
	}

	if (global_settings != settings) {
		free(global_settings);
		global_settings = settings;
	}

	size_t len = strlen(buffer);
	if (!len) {
		return NULL;
	}

	cjson_context ctx;
	ctx.root_state = NULL;
	ctx.tail_state = NULL;
	ctx.settings = settings;
	ctx.buf = buffer;
	ctx.len = len;
	ctx.pos = cjson_make_pos(ctx.settings, 0, 0, 0); 
	cjson_push_state(&ctx, initial_state, NULL, 0);
	cjson_value* val = cjson_parse_impl(&ctx);
	cjson_free(settings, ctx.pos);
	cjson_free_remaining_states(&ctx);
	return val;
}

cjson_value* cjson_create_empty()
{
	if (!global_settings) cjson_init(NULL);

	cjson_value* empty = cjson_value_create(global_settings);
	if (!empty) {
		return NULL;
	}

	return empty;
}

cjson_value* cjson_create_object()
{
	cjson_value* obj = cjson_create_empty();
	if (!obj) return NULL;

	obj->flags = cjson_object;
	return obj;
}
cjson_value* cjson_create_array()
{
	cjson_value* arr = cjson_create_empty();
	if (!arr) return NULL;

	arr->flags = cjson_array;
	return arr;
}
cjson_value* cjson_create_null()
{
	cjson_value* nil = cjson_create_empty();
	if (!nil) return NULL;

	nil->flags = cjson_null;
	return nil;
}
cjson_value* cjson_create_boolean(int value)
{
	cjson_value* b = cjson_create_empty();
	if (!b) return NULL;

	b->flags = cjson_boolean;
	b->intval = value;
	return b;
}
cjson_value* cjson_create_int(int value)
{
	cjson_value* i = cjson_create_empty();
	if (!i) return NULL;

	i->flags = cjson_number | cjson_integer;
	i->intval = value;
	return i;
}
cjson_value* cjson_create_double(double value)
{
	cjson_value* d = cjson_create_empty();
	if (!d) return NULL;

	d->flags = cjson_number | cjson_double;
	d->doubleval = value;
	return d;
}
cjson_value* cjson_create_string(const char* string)
{
	cjson_value* s = cjson_create_empty();
	if (!s) return NULL;

	s->flags = cjson_string;
	cjson_set_string(s, string);
	return s;
}

int cjson_is_string(cjson_value* v) { return v->flags & cjson_string; }
int cjson_is_number(cjson_value* v) { return v->flags & cjson_number; }
int cjson_is_double(cjson_value* v) { return v->flags & (cjson_number | cjson_double); }
int cjson_is_integer(cjson_value* v) { return v->flags & (cjson_number | cjson_integer); }
int cjson_is_object(cjson_value* v) { return v->flags & cjson_object; }
int cjson_is_array(cjson_value* v) { return v->flags & cjson_array; }
int cjson_array_length(cjson_value* v) { return v->intval; }
cjson_value* cjson_array_at(cjson_value* v, int i)
{
	int j = 0;
	v = v->child;
	while (v != NULL) {
		if (j == i) {
			return v;
		}

		++j;
		v = v->next;
	}

	return NULL;
}
int cjson_is_boolean(cjson_value* v) { return v->flags & cjson_boolean; }
int cjson_is_null(cjson_value* v) { return v->flags & cjson_null; }

const char* cjson_get_string(cjson_value* v) { return v->string; }
double cjson_get_double(cjson_value* v) { return v->doubleval; }
int cjson_get_integer(cjson_value* v) { return v->intval; }

int cjson_true(cjson_value* v)
{
	return v && v->intval == 1;
}
int cjson_false(cjson_value* v)
{
	return v && v->intval == 0;
}

void cjson_set_string(cjson_value* v, const char* str)
{
	if (!cjson_is_string(v)) {
		return;
	}

	if (v->string) {
		cjson_free(global_settings, v->string);
	}

	size_t len = strlen(str);
	v->string = cjson_alloc(global_settings, len + 1);

	if (v->string) {
		strcpy(v->string, str);
	}
}

void cjson_set_double(cjson_value* v, double d)
{
	if (!cjson_is_number(v)) {
		return;
	}

	v->flags &= ~cjson_integer;
	v->flags |= cjson_double;
	v->doubleval = d;
	v->intval = 0;
}

void cjson_set_integer(cjson_value* v, int i)
{
	if (!cjson_is_number(v)) {
		return;
	}

	v->flags &= ~cjson_double;
	v->flags |= cjson_integer;
	v->intval = i;
	v->doubleval = 0;
}


int cjson_replaceidx(cjson_value* p, int idx, cjson_value* replacement, cjson_value** old_value)
{
		int j = 0;
		cjson_value* c = p->child;
		while (c != NULL) {
			if (j == idx) {
				// replace
				if (c->prev) c->prev->next = replacement;
				if (c->next) c->next->prev = replacement;
				if (c == p->child) p->child = replacement;

				replacement->next = c->next;
				replacement->prev = c->prev;
				c->next = NULL;
				c->prev = NULL;

				if (old_value) {
					*old_value = replacement;
				}
				else {
					cjson_free_value(c);
				}

				return 1;
			}	

			++j;
			c = c->next;
		}

		return 0;
}
int cjson_eraseidx(cjson_value* p, int idx)
{
	int j = 0;
	cjson_value* c = p->child;
	while (p != NULL) { 
		if (j == idx) {
			if (c->prev) c->prev->next = c->next;
			if (c->next) c->next->prev = c->prev;
			if (c == p->child) p->child = NULL;
			if (p->child == NULL && c->next) p->child = c->next;
			c->next = NULL;
			c->prev = NULL;
			
			cjson_free_value(c);
			--p->intval;
			return 1;
		}
		
		j++;
		c = c->next;
	}	
	return 0;
}

void cjson_append(cjson_value* p, cjson_value *c)
{
	++p->intval;
	if (!p->child) {
		p->child = c;
	}
	else {
		cjson_value* insertion_point = cjson_end(p);
		insertion_point->next = c;
		c->prev = insertion_point;		 
	}
}

int cjson_object_size(cjson_value* p)
{
	if (cjson_is_object(p)) {
		return p->intval;
	}

	return 0;
}

int cjson_empty(cjson_value* p)
{
	if (p == NULL) return -1; // Maybe this should be true?

	if (cjson_is_string(p)) {
		return p->string == NULL || strlen(p->string) == 0;
	}

	if (cjson_is_array(p) || cjson_is_object(p)) {
		return p->intval == 0;
	}

	return -1; // not applicable
}

cjson_value* cjson_search_kv(cjson_value* p, const char* k)
{
	cjson_value* c = p->child;
	while (c != NULL) {
		if (strcmp(c->string, k) == 0) {
			return c; // Return the KV
		}

		c = c->next;
	}
	return NULL;
}

cjson_value* cjson_searchi_kv(cjson_value* p, const char* k)
{
	cjson_value* c = p->child;
	while (c != NULL) {
		if (stricmp(c->string, k) == 0) {
			return c; // Return the KV
		}

		c = c->next;
	}
	return NULL;
}

int cjson_erase_kv_from_tree(cjson_value* p, cjson_value* kv)
{
	if (!kv) return 0;

	// remove item from tree
	if (kv->prev) kv->prev->next = kv->next;
	if (kv->next) kv->next->prev = kv->prev;
	if (p->child == kv) p->child = NULL;
	if (p->child == NULL && kv->next) p->child = kv->next;

	kv->next = NULL;
	kv->prev = NULL;
	cjson_free_value(kv);
	--p->intval;
	return 1;
}

int cjson_erase(cjson_value* p, const char* k)
{
	return cjson_erase_kv_from_tree(p, cjson_search_kv(p, k));
}

int cjson_erasei(cjson_value* p, const char* k)
{
	return cjson_erase_kv_from_tree(p, cjson_searchi_kv(p, k));
}

int cjson_replace(cjson_value* p, const char* k, cjson_value* replacement, cjson_value** old_value)
{
	cjson_value* kv = cjson_search_kv(p, k);
	if (!kv) return 0;

	if (old_value != NULL) {
		*old_value = kv->child;
	}
	else {
		cjson_free_value(kv->child);
	}

	// Set the replacement
	kv->child = replacement;
	return 1;
}

void cjson_insert(cjson_value* p, const char* k, cjson_value* v)
{
	++p->intval;

	cjson_value *c = cjson_value_create(global_settings);

	if (!c) {
		return; // Error maybe?
	}
	
	c->flags = cjson_kv;
	
	size_t len = strlen(k);
	c->string = cjson_alloc(global_settings, len + 1);

	if (!c->string) {
		cjson_free_value(c);
		return;
	}

	strcpy(c->string, k);
	c->child = v;

	if (!p->child) {
		p->child = c;
	}
	else {
		cjson_value* insertion_point = cjson_end(p);
		insertion_point->next = c;
		c->prev = insertion_point;
	}
}

cjson_value* cjson_search_item(cjson_value* p, const char* k)
{
	cjson_value* kv = cjson_search_kv(p, k);
	if (!kv) return NULL;
	return kv->child;
}

cjson_value* cjson_searchi_item(cjson_value* p, const char* k)
{
	cjson_value* kv = cjson_searchi_kv(p, k);
	if (!kv) return NULL;
	return kv->child;
}

const char* cjson_type_string(cjson_value* v)
{
	if (v->flags & cjson_kv == cjson_kv) return "cjson_kv";
	if (v->flags & cjson_string == cjson_string) return "cjson_string";
	if (v->flags & cjson_number == cjson_number) {
		if (v->flags & cjson_integer == cjson_integer) return "cjson_number (int)";
		if (v->flags & cjson_double == cjson_double) return "cjson_number (double)";
		return "cjson_number (unk)";
	}
	if (v->flags & cjson_object == cjson_object) return "cjson_object";
	if (v->flags & cjson_boolean == cjson_boolean) return "cjson_boolean";
	if (v->flags & cjson_null == cjson_null) return "cjson_null";

	return "invalid";
}

int cjson_stringify_internal(char* out_buf, size_t buffer_count, cjson_value* v)
{
	if (v->flags & cjson_string) 
	{
		return snprintf(out_buf, buffer_count, "\"%s\"", v->string);
	}
	if (v->flags & cjson_number)
	{
		if (v->flags & cjson_integer) {
			return snprintf(out_buf, buffer_count, "%d", v->intval);
		}
		return snprintf(out_buf, buffer_count, "%g", v->doubleval);
	} 
	if (v->flags & cjson_object)
	{
		size_t len = 0;
		if (out_buf) {
			*out_buf = '{';
		}
		++len;

		cjson_value* c = v->child;
		while (c != NULL) {
			if (out_buf) {
				len += snprintf(&out_buf[len], buffer_count, "\"%s\":", c->string); // key
				len += cjson_stringify_internal(&out_buf[len], buffer_count, c->child); // value
			}
			else {
				len += snprintf(NULL, 0, "\"%s\":", c->string);
				len += cjson_stringify_internal(NULL, 0, c->child);
			}

			if (c->next) {
				if (out_buf) out_buf[len] = ',';
				len += 1;
			}

			c = c->next;
		}
		if (out_buf) out_buf[len] = '}';
		return ++len;
	}
	if (v->flags & cjson_array)
	{
		size_t len = 0;
		if (out_buf) {
			*out_buf = '[';
		}
		++len;

		cjson_value* c = v->child;
		while (c != NULL) {
			if (out_buf) {
				len += cjson_stringify_internal(&out_buf[len], buffer_count, c);
			}
			else {
				len += cjson_stringify_internal(NULL, 0, c);
			}

			if (c->next) {
				if (out_buf) out_buf[len] = ',';
				len += 1;
			}

			c = c->next;
		}

		if (out_buf) out_buf[len] = ']';
		return ++len;
	}
	if (v->flags & cjson_boolean)
	{
		const char* true_constant = "true";
		const char* false_constant = "false";
		size_t out_len = v->intval ? strlen(true_constant) : strlen(false_constant);

		if (out_buf) {
			memcpy(out_buf, v->intval ? true_constant : false_constant, out_len);
		}

		return out_len;
	} 
	if (v->flags & cjson_null)
	{
		const char* null_constant = "null";
		size_t len = strlen(null_constant);

		if (out_buf) {
			memcpy(out_buf, null_constant, len);
		}

		return len;
	}

	return 0;
}

char* cjson_stringify(cjson_value *v)
{ 
	int len = cjson_stringify_internal(NULL, 0, v);

	if (!len) {
		return NULL;
	}

	char* buf = malloc(len + 1);
	if (!buf) {
		return NULL;
	}

	cjson_stringify_internal(buf, len + 1, v);
	buf[len] = 0;
	return buf;
}
