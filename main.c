#include <stdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser.h"
#include "ast_interpreter.h"
#include "arena.h"

#define DEFAULT_HEAP_SIZE 4096
#define DEFAULT_HEAP_LOG_FILE "heap_log.csv"

enum { ACTION_AST_INTERPRET, ACTION_RUN } action = ACTION_AST_INTERPRET;
char *source_file = NULL;
size_t heap_size = DEFAULT_HEAP_SIZE;
char *heap_log_file = DEFAULT_HEAP_LOG_FILE;


/*
   FROM REFENRENCE IMPL
*/
static Str read_file(Arena *arena, const char *name) {
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



void usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options] <file>\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  ast_interpret          Interpret the source file as an abstract syntax tree\n");
    fprintf(stderr, "  run                    Run the source file as a program\n");
    fprintf(stderr, "  --heap-size <size>     Set the heap size in bytes (default: %zu)\n", DEFAULT_HEAP_SIZE);
    fprintf(stderr, "  --heap-log <filename>  Set the heap log file (default: %s)\n", DEFAULT_HEAP_LOG_FILE);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    size_t optind = 1;

    if (argc < 2) {
        usage(argv[0]);
    }

    if (strcmp(argv[optind], "ast_interpret") == 0) {
        action = ACTION_AST_INTERPRET;
        optind++;
    } else if (strcmp(argv[optind], "run") == 0) {
        action = ACTION_RUN;
        optind++;
    } else {
        usage(argv[0]);
    }
    for (; optind < argc && argv[optind][0] == '-'; optind++) {
        if (strcmp(argv[optind], "--heap-size") == 0) {
            if (optind + 1 >= argc) {
                usage(argv[0]);
            }
            heap_size = atoi(argv[optind + 1]);
            optind++;
        } else if (strcmp(argv[optind], "--heap-log") == 0) {
            if (optind + 1 >= argc) {
                usage(argv[0]);
            }
            heap_log_file = argv[optind + 1];
            optind++;
        } else {
            usage(argv[0]);
        }
    }
    if (optind + 1 != argc) {
        usage(argv[0]);
    }
    source_file = argv[optind];

	/*
    switch (action) {
        case ACTION_AST_INTERPRET:
            printf("Interpreting the source file '%s' as an abstract syntax tree\n", source_file);
            break;
        case ACTION_RUN:
            printf("Running the source file '%s' with heap size %zu and heap log file '%s'\n", source_file, heap_size, heap_log_file);
            break;
        default:
            fprintf(stderr, "Invalid action %d\n", action);
            exit(EXIT_FAILURE);
    }*/

	Arena arena;
	arena_init(&arena);

	Str src = read_file(&arena, source_file);

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

    IState *state = init_interpreter();
	interpret(ast, state);

	free_interpreter(state);	

	arena_destroy(&arena);

    return EXIT_SUCCESS;
}