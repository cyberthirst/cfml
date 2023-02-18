#include <stdio.h>

#include "arena.h"
#include "parser.h"

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Error: expected one argument\n");
		return 1;
	}

	Arena arena;
	arena_init(&arena);

	Ast *ast = parse_src(&arena, (Str) { .str = (u8 *) argv[1], .len = strlen(argv[1]) });

	if (ast == NULL) {
		fprintf(stderr, "Failed to parse source\n");
		arena_destroy(&arena);
		return 1;
	}

	printf("Hello world!\n");

	arena_destroy(&arena);

	return 0;
}
