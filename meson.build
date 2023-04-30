project('fml', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

exe = executable('fml',
  'src/main.c',
  'src/arena.c',
  'src/parser.c',
  'src/ast/ast_interpreter.c',
  'src/heap/heap.c',
  'src/bc/interpret/bc_interpreter.c',
  'src/utils/utils.c',
  'src/bc/compile/bc_compiler.c',
  'src/bc/bc_shared_globals.c',
  'src/bc/serialization.c',
  'src/gc/gc.c',
  'src/cmd_config.h',
  install : true)
