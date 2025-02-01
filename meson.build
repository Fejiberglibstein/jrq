project('jaq', 'c')

files = [
  './src/json_serialize.c',
  './src/json_deserialize.c',
  './src/lexer.c',
  './src/utils.c',
]

executable('jaq', files + './src/main.c')

tests = [
  ['json', 'test_serialize', './tests/json/test_serialize.c'],
  ['json', 'test_deserialize', './tests/json/test_deserialize.c'],
  ['json', 'test_both', './tests/json/test_both.c'],
  ['lexer', '_all', './tests/lang/lexer.c'],
]
foreach test : tests
  exe = executable(test[1], files + test[2])
  test(test[1], exe, suite : test[0])
endforeach
