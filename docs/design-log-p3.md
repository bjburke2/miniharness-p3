# Project 3 Design Log
**Author:** Brendan Burke · **P2 code used:** my own
**Tools used:** Claude double checked the code and provided a draft for this design log. I used it for the git/WSL setup help too. 

## 1. Hash table
I used **open addressing with linear probing**. The table is one flat array of slots (`std::vector<std::unique_ptr<Tool>>`). A null slot is empty. P3 never removes a tool, so I don't need tombstones. The price is that clusters can grow, so I keep the table sparse.

The hash starts from a nonzero seed. For each byte it XORs the byte in, multiplies by the FNV prime, and folds the high bits down with `h ^= h >> 29`. A final avalanche step (shift, multiply, shift) finishes it. It uses every character, depends on order (`"ab"` and `"ba"` differ), and returns the full 64-bit value. The slot index is `hash & (slots - 1)`, so the table size is always a power of two, starting at 16. The mixing matters because linear probing suffers when similar keys land next to each other.

The **maximum load factor is 0.5**. Before inserting, if `(used + 1) * 2` would exceed the slot count, I double the table and re-insert everything. I chose 0.5 because open addressing must stay below 1 (a full table makes `find` loop forever on a missing name), and at 0.5 an unsuccessful search needs only a couple of probes on average.

| 10,000 tools `tool_0`…`tool_9999` | Result |
|---|---|
| `bucket_count()` | 32,768 |
| `load_factor()` | 0.3052 |
| `max_probe_length()` (my hash) | 14 |
| Longest chain, weak hash `s[0] + s.size()` | 9,000 |

The load factor ended at 0.31 because 10,000 sits just past the point where the table doubled (16,384 slots would be 0.61). The weak hash gives 9,000 because every name starts with `t` and the length takes only a few values, so nearly all keys share a few indexes.

`add()` is amortized O(1). Most inserts probe only a few slots. A doubling costs O(n) but happens only after the table has gained n/2 more entries, so spreading the total (n + n/2 + n/4 + …, under 2n) across all inserts leaves a constant cost per insert.

## 2. Pointer stability
The table stores `unique_ptr<Tool>`, so each `Tool` is a separate heap object that the slots only point to. `grow()` allocates a new slot array and moves each `unique_ptr` into it. That moves a pointer value, not the tool, so every `Tool*` handed out by `find()` still points to the same object. My 10,000-tool test records the first tool's pointer and checks that `find()` returns the same address after all the resizes.

## 3. Calculator trace
I use a `nums` stack and a `pending` operator stack. For `2 * (3 + 4) - 5` (stacks listed bottom to top):

- `2`: goes onto `nums` → `[2]`.
- `*`: nothing pending, so it is pushed → pending `[*]`.
- `(`: pushed → pending `[* (]`.
- `3`: `nums` `[2 3]`.
- `+`: the top is `(`, which ranks 0 and blocks all reduction, so `+` is pushed → pending `[* ( +]`.
- `4`: `nums` `[2 3 4]`.
- `)`: reduce down to the `(`. Apply `+` to 3 and 4 to get 7 → `nums` `[2 7]`, pending `[* (]`. Discard the `(` → pending `[*]`.
- `-`: it ranks 1 and `*` ranks 2, which is at least as high, so apply `*`: 2 × 7 = 14 → `nums` `[14]`. Then push `-`.
- `5`: `nums` `[14 5]`.
- End: apply the leftover `-` → 14 − 5 = 9.

The `>=` comparison is what makes `10 - 4 - 3` come out as 3: the first `-` is applied before the second one is pushed.

## 4. Errors as values
A failing tool call is an ordinary event in an agent loop: the model asks for a missing file, a forbidden path, or `1/0`. The model needs the message back as data so it can recover. Exceptions fit bugs and broken invariants, such as asking a JSON string for a number, where the program should stop and be fixed.

## 5. What I would design differently
`find()` and `grow()` call `tool->name()` often, and it returns a fresh `std::string` through a virtual call each time. Slots could hold a small entry with the name and a cached hash next to the `unique_ptr`, so probing could compare cached hashes first and growth would never recompute them.
