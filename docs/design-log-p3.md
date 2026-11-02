# Project 3 Design Log
**Author:** Tanishqa Taywade Patil · **P2 code used:** instructor reference (`-DUSE_P2_REFERENCE=ON`)
**Tools used:** Claude (AI assistant) gave me step-by-step instructions for each piece, reviewed the code and tests I wrote, caught gaps in my test coverage against the spec tables, helped debug a macOS build error (`mkdtemp` needed `<unistd.h>` in `test_helpers.h`), and drafted this log, which I reviewed and edited.

## 1. Hash table
I used **separate chaining**: each bucket is a `std::vector<std::unique_ptr<Tool>>`. Chaining is simple, degrades gracefully as the table fills, and has no "full table" failure mode, so I could use a load-factor threshold of 1.0. Open addressing would be more cache-friendly, but it needs a threshold below 1 and produces much longer probe sequences on these names (around 30 versus 6). The registry only holds a few tools in real use, so cache behavior doesn't matter here.

**Hash function:** 64-bit FNV-1a. It XORs in every byte and then multiplies by the FNV prime, so every character affects the result and order matters (`"ab"` and `"ba"` hash differently). The function returns the full 64-bit value. The bucket index is computed separately as `hash & (bucket_count - 1)`, which equals `hash % bucket_count` because the bucket count is always a power of two (it starts at 8). Appendix A's offset basis, `1469598103934665603`, is missing the final digit, so I used the standard value `14695981039346656037`, which makes my hash match published FNV-1a outputs.

**Resize policy:** before inserting, if `(size + 1) / bucket_count` would exceed **1.0**, I double the bucket count and re-insert every entry using the new index. Checking *before* the insert guarantees `load_factor() <= 1.0` whenever `add()` returns.

**Measurements** (10,000 names `tool_0` … `tool_9999`):

| Hash | bucket_count | load_factor | longest chain |
|---|---|---|---|
| FNV-1a (mine) | 16384 | 0.6104 | **6** |
| `s[0] + s.size()` | 16384 | — | **9000** |

The weak hash only depends on the first character (always `'t'`) and the length (6 to 10), so it can produce only five distinct values. 9,000 of the names have the same length and land in one bucket, which turns lookup into an O(n) scan.

**Why `add()` is amortized O(1):** a resize from n to 2n buckets costs O(n), but it only happens after about n cheap inserts since the previous resize. Spreading that O(n) cost across those inserts adds O(1) to each.

## 2. Pointer stability
The buckets store `std::unique_ptr<Tool>`, not `Tool` objects. Each tool is allocated once on the heap and never moves. During a rehash I `std::move` the `unique_ptr`s into the new bucket array, which transfers only the pointer value. The `Tool*` that `find()` returns points at the heap object, so it stays valid through any number of resizes. My `reg_10000_tools` test checks this: the pointer for `tool_0` is unchanged after 9,999 more inserts and 11 resizes.

## 3. Calculator trace: `2 * (3 + 4) - 5`
- `2`: push → values `[2]`, ops `[]`
- `*`: ops is empty, so push → ops `[*]`
- `(`: push → ops `[*, (]`
- `3`: push → values `[2, 3]`
- `+`: the top of ops is `(`, which isn't an operator, so just push → ops `[*, (, +]`
- `4`: push → values `[2, 3, 4]`
- `)`: apply until `(`. Pop `+`, pop 4 (right) then 3 (left), push 7 → values `[2, 7]`. Then pop the `(` → ops `[*]`
- `-`: the top is `*` with precedence 2 ≥ 1, so apply it: 2 × 7 = 14 → values `[14]`, ops empty. Push `-` → ops `[-]`
- `5`: push → values `[14, 5]`
- End: apply `-`: 14 − 5 = 9 → values `[9]`

The answer is **9**. The `>=` comparison makes operators of equal precedence evaluate left to right (`10 - 4 - 3 = 3`), and popping the right operand first keeps subtraction and division in the correct order. My evaluator uses loops only, so 20,000 terms or 5,000 nested parentheses just grow the stacks, not the call stack. It also checks stack sizes before every `top()`, so it returns an error instead of crashing on malformed input.

## 4. Errors as values
In an agent loop, a tool failing is **expected**: the model asks for a missing file, a path outside the workspace, or a division by zero, and it should read the error and try something else. That is ordinary control flow, so `resolve`, `calc_evaluate`, and every tool report failure as a return value (`false` plus a message, or `ToolResult::failure`). The caller has to handle it explicitly, and nothing can unwind through the harness by accident. Exceptions are for **programmer errors**, bugs that shouldn't happen in correct code, such as calling `as_string()` on a JSON number. Those mean the C++ is wrong, not that the model made a bad request. The dispatcher catches them only as a last line of defense.

## 5. What I would design differently
I would store each tool's name inside its bucket entry, as a `std::pair<std::string, std::unique_ptr<Tool>>`. Right now `find()` and `rehash()` call the virtual `name()` on every entry they compare, and it returns a new `std::string` each time. Storing the key once would make lookups and resizes cheaper. It would also protect the table if a tool's `name()` ever returned something different after registration.