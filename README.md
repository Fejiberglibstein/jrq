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

Access a field:
```bash
$echo '{"a": 2, "b": 6}' | jrq '.a'
2
```

Map over an iterable
```bash
$echo '[1, 2, 3, 4]' | jrq '.map(|v| v * 2)'
[2, 4, 6, 8]
```

Apply multiple functions
```bash
$echo '"Hello World"' | jrq '.split("").filter(|v| v != "o" && v != "l").join("")'
"He Wrd"
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
