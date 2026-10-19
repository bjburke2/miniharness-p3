# Project 3 Design Log
**Author:** Brendan Burke · **P2 code used:** my own
**Tools used:** Claude double checked the code and provided a draft for this design log. I used it for the git/WSL setup help too. I then ran the full test suite and revised this log so it reflects what I understand.

## 1. Hash table
I chose **separate chaining**: each bucket is a `std::vector<std::unique_ptr<Tool>>`. P3 never removes tools and the table stays small, so chaining is simple and its load factor can safely approach 1. Open addressing would need a threshold well below 1 or `find` on a missing name could loop forever.

The hash is **64-bit FNV-1a**. It XORs in each byte and then multiplies, so it uses every character and is order-sensitive (`"ab"` and `"ba"` differ). It returns the full-width value, and the index is `hash % bucket_count()`.

The table starts with 8 buckets. Before inserting, if `(size + 1) / buckets` would exceed **0.75**, I double the bucket count and re-insert every entry using the new count. I checked before inserting so the load factor never overshoots, and 0.75 keeps chains short at the cost of some empty buckets.

| 10,000 tools `tool_0`…`tool_9999` | Result |
|---|---|
| `bucket_count()` | 16,384 |
| `load_factor()` | 0.6104 |
| `max_probe_length()` (FNV-1a) | 6 |
| Longest chain, weak hash `s[0] + s.size()` | 9,000 |

The weak hash collapses all the names onto a handful of buckets because every name starts with `t` and has only a few possible lengths. That is why a real hash matters.

`add()` is amortized O(1). A single insert is O(1) on average, but a resize costs O(n). Because the size doubles each time, the total resize work over n inserts is about n + n/2 + n/4 + … < 2n, so each insert pays a constant amount on average.

## 2. Pointer stability
The buckets store `unique_ptr<Tool>`, so the `Tool` objects live on the heap, separate from the bucket arrays. During `rehash` I only `std::move` the smart pointers into the new array. The `Tool` objects themselves never move, so a `Tool*` from `find()` remains valid. My 10,000-tool test checks that the pointer from the first insert equals `find()` after the last insert.

## 3. Calculator trace
For `2 * (3 + 4) - 5`, with `values` and `ops` listed bottom to top:

1. `2`: push on `values`. values `[2]`, ops `[]`.
2. `*`: `ops` is empty, so push. values `[2]`, ops `[*]`.
3. `(`: push. ops `[* (]`.
4. `3`: push. values `[2 3]`.
5. `+`: the top is `(`, so nothing is applied; push. ops `[* ( +]`.
6. `4`: push. values `[2 3 4]`.
7. `)`: apply `+`. Pop 4 (right), then 3 (left), compute 7. values `[2 7]`. The top is now `(`, so pop it. ops `[*]`.
8. `-`: `*` has precedence 2, which is ≥ 1, so apply it: 2 × 7 = 14. values `[14]`, ops `[]`. Push `-`.
9. `5`: push. values `[14 5]`.
10. End of input: apply `-`: 14 − 5 = 9. values `[9]`.

The answer is 9. The "≥" rule makes equal-precedence operators evaluate left to right.

## 4. Errors as values
In an agent loop a tool failure is normal: the file is missing, the path is forbidden, or the expression divides by zero. The model is expected to read the error and try something else. Returning `ToolResult::failure(...)`, or `false` plus a message from `resolve` and `calc_evaluate`, treats that as ordinary data, and the caller can't forget to handle it the way it can forget a `catch`. Exceptions are the right choice for programmer errors, such as calling `as_string()` on a JSON number, where the code itself is wrong and should stop loudly. The dispatcher's catch-all is a last line of defense for those bugs.

## 5. What I would design differently
`add()` calls `find()` and then hashes the name again to pick a bucket, and `rehash` calls `tool->name()` for every entry. That builds a new `std::string` through a virtual call each time. In hindsight I would store a small node holding the name and its cached hash alongside the `unique_ptr`. That would hash each name only once and make resizing cheaper.
