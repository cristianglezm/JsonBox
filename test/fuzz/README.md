# Fuzzing JsonBox

libFuzzer harnesses for the parser and the writer, built with Clang, AddressSanitizer and
UndefinedBehaviorSanitizer.

- `fuzz_parse`: `Value::loadFromString()` on arbitrary bytes. Malformed input has to parse or throw a
  `std::exception`; it must not crash, hang or read memory it does not own.
- `fuzz_roundtrip`: whatever the parser accepts is written, parsed again and written again.

## Build

```bash
CXX=clang++ CC=clang cmake -S . -B build -GNinja -DJsonBox_BUILD_FUZZERS=ON -DJsonBox_BUILD_EXAMPLES=OFF
cmake --build build
```

The binaries are in `build/fuzz/bin`.

## Run

libFuzzer adds what it finds to the corpus directory it is given, so run on a copy:

```bash
cp -r test/fuzz/corpus /tmp/corpus
build/fuzz/bin/fuzz_parse -max_total_time=60 -timeout=10 /tmp/corpus
```

Replay a crash with the crash file as the only argument:

```bash
build/fuzz/bin/fuzz_parse crash-<hash>
```

The `fuzz` workflow runs both harnesses for `smoke_seconds` (default 60) from the Actions tab and
uploads any crash, timeout or out-of-memory input as an artifact.

## Corpus

- `seed_*.json`: valid documents to start from.
- `regression_*.json`: inputs that broke a released version. Keep them: they are replayed with the
  rest of the corpus.

## Found so far

- **A stream that fails without reaching its end made the parser spin forever.** Every parsing loop only
  tested for end of input, so a read error (or a truncated nested value) left it calling `get()` on a
  failed stream. `regression_array_stream_failure.json` timed out in a downstream harness that links
  JsonBox; `ValueParseFailingStream` in `test/ValueTest.cpp` reproduces it deterministically.
- **A number at the very end of the input had its last character read twice** (`12` parsed as `122`), because
  the loop used the character from a `get()` that had failed.
- **A one-character input was rejected as UTF-16** (`7` threw), because the encoding probe used a byte that
  was never read.
