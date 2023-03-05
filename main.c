#include <stdio.h>
#include <assert.h>

#include "parser.h"
#include "ast_interpreter.h"
#include "arena.h"


static Str
read_file(Arena *arena, const char *name)
{
	FILE *f = fopen(name, "rb");
	if (!f) {
		printf("failed to open file\n");
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		printf("failed to seek in file\n");
	}
	long tell = ftell(f);
	if (tell < 0) {
		printf("failed to ftell file\n");
	}
	size_t fsize = (size_t) tell;
	assert(fseek(f, 0, SEEK_SET) == 0);
	u8 *buf = arena_alloc(arena, fsize);
	size_t read;
	if ((read = fread(buf, 1, fsize, f)) != fsize) {
		if (feof(f)) {
			fsize = read;
		} else {
			printf("failed to read the file\n");
		}
	}
	assert(fclose(f) == 0);
	return (Str) { .str = buf, .len = fsize };
}


int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Error: expected at least one argument\n");
		return 1;
	}

	Arena arena;
	arena_init(&arena);

    printf("file: %s\n", argv[1]);

	Str src = read_file(&arena, argv[1]);

	if (src.str == NULL) {
		arena_destroy(&arena);
		return 1;
	}

	Ast *ast = parse_src(&arena, src);

	if (ast == NULL) {
		fprintf(stderr, "Failed to parse source\n");
		arena_destroy(&arena);
		return 1;
	}

    AstTop *top = (AstTop *) ast;
    IState *state = init_interpreter();
	Value val = interpret(ast, state);
	print_val(val);

	arena_destroy(&arena);

	return 0;
}
