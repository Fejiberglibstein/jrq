project('jrq', 'c')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# vi[:!find src/ -name "*.c" | grep -v "main.c" | xargs -I{} echo "'{}',"=i[
files = [
  'src/eval.c',
  'src/json_serialize.c',
  'src/lexer.c',
  'src/alloc.c',
  'src/json.c',
  'src/json_deserialize.c',
  'src/eval_functions.c',
  'src/eval_node.c',
  'src/parser.c',
  'src/errors.c',
  'src/json_iter.c',
]

executable('jrq', files + './src/main.c', dependencies: m_dep)

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
