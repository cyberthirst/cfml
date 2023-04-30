#include <stdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser.h"
#include "ast/ast_interpreter.h"
#include "bc/interpret/bc_interpreter.h"
#include "arena.h"
#include "bc/compile/bc_compiler.h"
#include "cmd_config.h"

//default heap size is 100MiB
#define DEFAULT_HEAP_SIZE (200 * 1024 * 1024)
#define DEFAULT_HEAP_LOG_FILE NULL

Config config = { .heap_size = DEFAULT_HEAP_SIZE,
                  .heap_log_file = DEFAULT_HEAP_LOG_FILE,
                  .source_file = NULL, .action = ACTION_AST_INTERPRET };

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
    fprintf(stderr, "  bc_interpret           Interpret the source file as bytecode\n");
    fprintf(stderr, "  run                    Run the source file as a program\n");
    fprintf(stderr, "  bc_compile             Compile the source file to bytecode\n");
    fprintf(stderr, "  --heap-size <size>     Set the heap size in bytes (default: %d)\n", DEFAULT_HEAP_SIZE);
    fprintf(stderr, "  --heap-log <filename>  Set the heap log file (default: %s)\n", DEFAULT_HEAP_LOG_FILE);
    exit(EXIT_FAILURE);
}

Ast * get_ast(Arena *arena){
 

    Str src = read_file(arena, config.source_file);

    if (src.str == NULL) {
        arena_destroy(arena);
        exit(1);
    }

    if (src.len == 0) {
        arena_destroy(arena);
        exit(0);
    }

    Ast *ast = parse_src(arena, src);

    if (ast == NULL) {
        fprintf(stderr, "Failed to parse source\n");
        arena_destroy(arena);
        exit(1);
    }
    return ast;
}

int main(int argc, char *argv[]) {
    int optind = 1;

    if (argc < 2) {
        usage(argv[0]);
    }

    if (strcmp(argv[optind], "ast_interpret") == 0) {
        config.action = ACTION_AST_INTERPRET;
        optind++;
    } else if (strcmp(argv[optind], "bc_interpret") == 0) {
        config.action = ACTION_BC_INTERPRET;
        optind++;
    } else if (strcmp(argv[optind], "run") == 0) {
        config.action = ACTION_RUN;
        optind++;
    } else if (strcmp(argv[optind], "bc_compile") == 0) {
        config.action = ACTION_BC_COMPILE;
        optind++;
    } else {
        usage(argv[0]);
    }
    for (; optind < argc && argv[optind][0] == '-'; optind++) {
        if (strcmp(argv[optind], "--heap-size") == 0) {
            if (optind + 1 >= argc) {
                usage(argv[0]);
            }
            config.heap_size = atoi(argv[optind + 1]);
            // convert to bytes as the input is in MiB
            config.heap_size = config.heap_size * 1024 * 1024;
            optind++;
        } else if (strcmp(argv[optind], "--heap-log") == 0) {
            if (optind + 1 >= argc) {
                usage(argv[0]);
            }
            config.heap_log_file = argv[optind + 1];
            optind++;
        } else {
            usage(argv[0]);
        }
    }
    if (optind + 1 != argc) {
        usage(argv[0]);
    }
    config.source_file = argv[optind];

    Arena arena;
    arena_init(&arena);

    switch (config.action) {
        case ACTION_AST_INTERPRET: {

	        Ast *ast = get_ast(&arena);

            IState *state = init_interpreter();
	        interpret(ast, state);

	        free_interpreter(state);

            break;
        }
        case ACTION_BC_INTERPRET: {
            //printf("Running the bc_interpreter on source file %s\n", source_file);
            deserialize_bc_file(config.source_file);
            bc_interpret();
            break;
        }
        case ACTION_BC_COMPILE: {
	        Ast *ast = get_ast(&arena);

            bc_compile(ast, true);

            break;
        }
        case ACTION_RUN: {
	        Ast *ast = get_ast(&arena);
            bc_compile(ast, false);
            bc_interpret();
            break;
        }

        default:
            fprintf(stderr, "Invalid action %d\n", config.action);
            exit(EXIT_FAILURE);
    }
    arena_destroy(&arena);
    return EXIT_SUCCESS;
}
