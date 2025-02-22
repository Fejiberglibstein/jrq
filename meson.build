project('jrq', 'c')

# .!find src/ -name "*.c" | grep -v "main.c" | xargs -I{} echo "'{}',"
files = [
  'src/eval.c',
  'src/json_serialize.c',
  'src/lexer.c',
  'src/alloc.c',
  'src/json.c',
  'src/json_deserialize.c',
  'src/eval_functions.c',
  'src/parser.c',
  'src/json_iter.c',
]

executable('jrq', files + './src/main.c')

tests = [
  ['json', 'serialize', './tests/json/serialize.c'],
  ['json', 'deserialize', './tests/json/deserialize.c'],
  ['json', 'serde', './tests/json/serde.c'],
  ['json', 'iter', './tests/json/iter.c'],

  ['lang', 'lexer', './tests/lang/lexer.c'],
  ['lang', 'parser', './tests/lang/parser.c'],
  ['lang', 'eval', './tests/lang/eval.c'],
]
foreach test : tests
  exe = executable('test_' + test[1], files + test[2])
  test(test[1], exe, suite: test[0])
endforeach
