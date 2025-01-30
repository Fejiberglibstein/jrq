project('jaq', 'c')

files = ['./src/serialize.c', './src/deserialize.c']

executable('jaq', files + './src/main.c')

tests = [
  ['test_serialize', './tests/json/test_serialize.c'],
  ['test_deserialize', './tests/json/test_deserialize.c'],
  ['test_both', './tests/json/test_both.c'],
]
foreach test : tests
    exe = executable(test[0], files + test[1])
    test(test[0], exe)
endforeach
