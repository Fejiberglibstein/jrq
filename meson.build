project('jaq', 'c')

# .!find src/ -name "*.c" | grep -v "main.c" | xargs -I{} echo "'{}',"
files = [
  'src/json_serialize.c',
  'src/lexer.c',
  'src/json_deserialize.c',
  'src/parser.c',
  'src/utils.c',
]

executable('jaq', files + './src/main.c')

tests = [
  ['json', 'serialize', './tests/json/test_serialize.c'],
  ['json', 'deserialize', './tests/json/test_deserialize.c'],
  ['json', 'serde', './tests/json/test_both.c'],

  ['lang', 'lexer', './tests/lang/lexer.c'],
]
foreach test : tests
  exe = executable('test_' + test[1], files + test[2])
  test(test[1], exe, suite: test[0])
endforeach
