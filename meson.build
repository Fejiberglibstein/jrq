project('jrq', 'c')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

files = [
  'src/alloc.c',
  'src/errors.c',
  'src/eval/eval.c',
  'src/eval/function_declarations.c',
  'src/eval/functions.c',
  'src/eval/node.c',
  'src/json.c',
  'src/json_deserialize.c',
  'src/json_iter.c',
  'src/json_serialize.c',
  'src/lexer.c',
  'src/parser.c',
  'src/strings.c',
]

executable(
  'jrq',
  files + './src/main.c',
  dependencies: m_dep,
  install: true
)

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
  exe = executable('test_' + test[1], files + test[2], dependencies: m_dep)
  test(test[1], exe, suite: test[0])
endforeach
