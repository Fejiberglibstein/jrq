<div align="center">
  <h1>Jrq</h1>
  <i>The power of jq in a simpler syntax</i>
</div>

---

Jrq is a command line utility for mapping, filtering, and transforming JSON
data, similar to [jq](https://jqlang.org/). However, jrq uses a simpler syntax,
most similar to [iterators in rust](https://doc.rust-lang.org/rust-by-example/trait/iter.html).

For a full list of features and a tutorial, see the [wiki](https://github.com/Fejiberglibstein/jrq/wiki).

# Usage

```json
// Input
{
  "foo": [
    { "bar": 0 },
    { "bar": 1 },
    { "bar": 2 },
  ]
}
```

```bash
cat file.json | jrq '.foo.map(|v| v.bar)'
```

```json
// Output
[0, 1, 2]
```

---

```json
// Input
[
  {
    "foo": 10,
    "bar": 6                
  },                        
  {                            
    "foo": 8,
    "bar": 8
  },
  {
    "foo": 7,
    "bar": 8
  },
  {
    "foo": 21,
    "bar": 3
  },
  {
    "foo": 1,
    "bar": 11
  },
  {
    "foo": 7,
    "bar": 9
  }
]
```

```bash
cat file.json | jrq '
  .filter(|v| v.foo <= v.bar)
  .and_then(|h| {
    "foo": h.map(|v| v.foo),
    "bar": h.map(|v| v.bar)
  })'

```

```json
// Output
{
  "foo": [
    8, 
    7, 
    1, 
    7
  ], 
  "bar": [
    8, 
    8, 
    11, 
    9
  ]
}
```

# Installation

Prerequisites:
- ninja
- meson

```bash
meson setup build
ninja -C build
ninja -C build install
```


# Contributing

All contributions are welcome. If you want to contribute anything, like fixing a
bug, adding a feature, or adding more functions, you should follow the following
steps:

1. Fork the repository
2. Implement your changes
3. Open a pull request when you finish
