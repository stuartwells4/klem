# klem Coding Standard

Applies to all new code in klem. When editing existing files, follow the style already present in that file.

---

## General Rules

- Do not introduce new external dependencies without approval.
- Do not add features, abstractions, or error handling beyond what the task requires.
- Prefer simple, readable code over clever abstractions.
- No comments that describe *what* the code does — only add a comment when the *why* is non-obvious.

---

## 1. Header Includes

Keep header `#include` lists minimal. Include only what the *declarations* in the header require — not what the `.c` implementation needs.

- **Prefer forward declarations.** Any struct used only as a pointer does not need its full definition in the header.
- Move implementation-only includes into the `.c`.
- Do not include headers preemptively.

```c
/* WRONG — pulls skbuff.h into every file that includes this header */
#include <linux/skbuff.h>
void klem80211Recv(void *pPtr, struct sk_buff *pSkb);

/* CORRECT */
struct sk_buff;               /* forward declare; full include goes in .c */
void klem80211Recv(void *pPtr, struct sk_buff *pSkb);
```

---

## 2. Naming

### Functions and file-scope data

| What | Convention | Example |
|---|---|---|
| Public function | `klem` + module + action, camelCase | `klemDataInit`, `klemNetConnect`, `klem80211Start` |
| File-scope static function | `priv` prefix, camelCase | `privCtrlProcess`, `privStart` |
| File-scope static const table | `privConst` prefix | `privConstChannels_2ghz` |
| File-scope static variable | type prefix + `Local` | `pLocalData` |

### Variables, parameters and struct members

Type prefixes apply to locals, parameters and struct members alike.

| Type | Prefix | Example |
|---|---|---|
| `bool` | `b` | `bActive`, `bConnected` |
| `int` | `i` | `iSize`, `iIfIndex` |
| `unsigned int` | `ui` | `uiVersion` |
| Other unsigned (`u16`, `u32`, `unsigned long`) | `u` | `uProtocol`, `uBeacons` |
| `unsigned char` | `ub` | `ubNode` |
| Pointer or `char` array | `p` | `pData`, `pDevName` |
| Enum | `e` | `eMode`, `eCmd` |
| Lock or wait queue | `s` | `sLock`, `sSpinLock`, `sListWait` |

A generic `void *` parameter that is cast to the module's data is named `pPtr`.

### Types, constants and guards

| What | Convention | Example |
|---|---|---|
| Per-module state typedef | name + `Data` | `KLEMData`, `mac80211Data` |
| Packed wire-format header typedef | `UPPER_SNAKE_CASE`, struct tag ends in `_DEF` | `KLEM_RAW_HEADER` |
| Other typedefs | `lower_snake_case` | `ctrl_data`, `raw_socket` |
| Enum value | `UPPER_SNAKE_CASE` | `LEMU`, `BRIDGE` |
| Constant / `#define` | `UPPER_SNAKE_CASE`; `KLEM_` prefix when project-wide | `KLEM_PROTOCOL`, `MAX_WIRELESS_NODE` |
| Include guard | `KLEM_<MODULE>_INCLUDE` | `KLEM_DATA_INCLUDE` |

Place compile-time constants on the left side of `==` and `!=` comparisons (Yoda style):

```c
if (NULL == pBuffer) { ... }
if (NULL != pMemory) { ... }
if (0 == iValue) { ... }
if (MAX_RETRIES == uiRetries) { ... }
```

Function definitions put the opening brace on its own line; control statements (`if`, `for`, `while`, `switch`) put it on the same line:

```c
void klemDataReset(KLEMData *pData)
{
  if (NULL == pData) {
    ...
  }
}
```

---

## 3. Error Handling

- Functions return `bool` (success/failure) or `int` (0 = ok, negative errno = error).
- Do not silently drop failures — log them and propagate the return value.

### Function Structure (MANDATORY)

- Functions shall have a single exit point.
- Use one `return` statement at the end of the function.
- Error handling shall use `goto` cleanup labels.
- Cleanup labels shall be placed immediately before the final `return`.
- Resources acquired within the function must be released in the cleanup path.
- Output parameters and return values shall be assigned before jumping to cleanup.

```c
/* CORRECT — single return, goto for all exits */
static int privCtrlRun(void)
{
  int iRc = 0;
  ctrl_data *pWork = NULL;

  pWork = kzalloc(sizeof(*pWork), GFP_KERNEL);
  if (NULL == pWork) {
    iRc = -ENOMEM;
    goto cleanup;
  }

  iRc = privCtrlDoWork(pWork);
  if (0 != iRc) {
    goto cleanup;
  }

cleanup:
  kfree(pWork);
  return iRc;
}

/* WRONG — multiple return statements */
static int privCtrlRun(void)
{
  ctrl_data *pWork = kzalloc(sizeof(*pWork), GFP_KERNEL);
  if (NULL == pWork) {
    return -ENOMEM;
  }
  ...
  return 0;
}
```

---

## 4. Memory

**Prefer stack allocation** for small temporary buffers. The kernel stack is small, so anything larger than a few hundred bytes goes on the heap.

**Every allocation must be freed on every exit path.** Use the `goto` cleanup pattern from §3 so the free happens in one place.

Use `GFP_ATOMIC` only where sleeping is not allowed (interrupt context or while holding a spinlock); otherwise use `GFP_KERNEL`.

---

## 5. Concurrency

### Shared state

Any variable written from one context and read from another must be either an `atomic_t` or protected by a lock on every access. Plain `bool` / `int` is not acceptable for shared state.

- Process context only → `struct mutex`
- Shared with interrupt or softirq context → `spinlock_t` with `spin_lock_irqsave`

Do not sleep while holding a spinlock — no `GFP_KERNEL` allocations, `mutex_lock`, or file system calls.

### Check-then-act (TOCTOU)

The check and the act must be in the same critical section:

```c
/* WRONG — another thread can drain the queue between the check and the lock */
if (!list_empty(&pData->listQueue)) {
  mutex_lock(&pData->sLock);
  pItem = list_first_entry(&pData->listQueue, struct item, list);
  list_del(&pItem->list);
  mutex_unlock(&pData->sLock);
}

/* CORRECT */
mutex_lock(&pData->sLock);
if (!list_empty(&pData->listQueue)) {
  pItem = list_first_entry(&pData->listQueue, struct item, list);
  list_del(&pItem->list);
}
mutex_unlock(&pData->sLock);
```

Capacity guards follow the same rule — the size check and the increment must be inside the same lock scope.

---

## 6. Logging

Use the `KLEM_LOG` / `KLEM_MSG` macros from `klemData.h`. They prefix each message with `klem::` and the function name. Do not call `printk` directly.

```c
KLEM_MSG("failed to allocate memory\n");
KLEM_LOG("free memory at %p\n", pData);
```
