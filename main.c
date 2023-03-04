#include <stdio.h>

#include "parser.h"
#include "ast_interpreter.h"
#include "arena.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Error: expected at least one argument\n");
		return 1;
	}

	Arena arena;
	arena_init(&arena);

    printf("file: %s\n", argv[1]);
	Ast *ast = parse_src(&arena, (Str) { .str = (u8 *) argv[1], .len = strlen(argv[1]) });

	if (ast == NULL) {
		fprintf(stderr, "Failed to parse source\n");
		arena_destroy(&arena);
		return 1;
	}

    IState *state = init_interpreter();
	interpret(ast, state);

	arena_destroy(&arena);

	return 0;
}
