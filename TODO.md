
# Performance

- [ ] bulk allocator for initial file read
- [ ] Bulk allocator for ASTNode (should be easy)

- [ ] iter_size_hint() 
- Maybe remove type checking for lists? When an iterator is converted into a
  list only to be iterated over again (as the case with .sum()) it seems a bit
  pointless.
  - Alternatively, we could lean even harder into the type system and make
    everything typed. This would also fix the probkem since we could then make
    iterators have type inference.

# parser

- [ ] Make functions possible, not just methods
  - This would make something like `range(10)` allowed.
  - Right now, only methods (`.foo()`) are allowed.

- [ ] `.[]` syntax. This will be another way of accessing the input data, but
  will allow for expression indexing.
  - Right now, only `{"foo": 1} | .foo`, `[0, 1, 2] | .0` is allowed, this will
    make it possible to do `{"foo bar": 10} | .["foo bar"]`


# functions

- [x] `(string).split(string) -> Iterator<string>`

- [ ] `(object | list<list | object>).flat_map(|T| -> U) -> (object | list<any>)`
  - shorthand for `.map( ... ).flatten()`
    - `[1, 2, 3, 4].flatmap(|v| range(v)) -> [0, 0, 1, 0, 1, 2, 0, 1, 2, 3]`

- [ ] `(Iterator<any>).chain(Iterator<any>) -> Iterator<any>`
  - Links two iterators together like a chain
    - `[1, 2, 3].chain([4, 5, 6]) -> [1, 2, 3, 4, 5, 6]`

- [x] `(Iterator<T>).take_while(|T| -> bool) -> Iterator<T>`
- [x] `(Iterator<T>).skip_while(|T| -> bool) -> Iterator<T>`
- [x] `(Iterator<T>).take(number) -> Iterator<T>`
- [x] `(Iterator<T>).skip(number) -> Iterator<T>`



# improvements

`(string).length()` should be based on unicode codepoints
