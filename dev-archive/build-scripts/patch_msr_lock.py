"""patch_msr_lock.py <generated pch> -- the game's atomic steps without the port-wide lock (2026-11-10).

The 360 code brackets every atomic step with "interrupts off / on" (mfmsr, mtmsrd r13,1 ... mtmsrd rX,1). The generated
header turns each of those into the port's ONE shared lock (rex::thread::global_critical_region): a lock + unlock for
the "are interrupts on" read, a lock for "off", an unlock for "on". The game does it constantly (its own lock routine
sub_820C87F8 / sub_820C88B0 and 112 other functions), so its threads and the port's graphics thread queue on that one
lock: measured 2026-11-10 in the alley, the graphics thread spent over a third of its time waiting for it and the
game's drawing thread about 80 % spinning until the graphics thread caught up.

Safe to drop, checked 2026-11-10 over the whole generated source: all 981 bracketed sections hold only one
lwarx / stwcx. (978) or ldarx / stdcx. (3) step with compares, sums and branches, no call and no other store; and the
generator already makes stwcx. a real compare-and-swap on the host, so the step is atomic by itself.

Run after the code generation step and before the compile (the generator rewrites the header). It only replaces the
three macro bodies; a second run changes nothing. DK_KEEP_MSR_LOCK defined at compile time keeps the old behaviour.
"""
import re
import sys

NEW = '''#ifdef DK_KEEP_MSR_LOCK
#define REX_CHECK_GLOBAL_LOCK()                                        \\
  ([&]() -> u64 {                                                      \\
    auto lock_ = rex::thread::global_critical_region::AcquireDirect(); \\
    return ppc_global_lock_count_().load() ? 0 : 0x8000;               \\
  }())

#define REX_ENTER_GLOBAL_LOCK()                          \\
  do {                                                   \\
    rex::thread::global_critical_region::mutex().lock(); \\
    ppc_global_lock_count_().fetch_add(1);               \\
  } while (0)

#define REX_LEAVE_GLOBAL_LOCK()                                                           \\
  do {                                                                                    \\
    auto old_count_ = ppc_global_lock_count_().fetch_sub(1);                              \\
    assert(old_count_ >= 1 && "LeaveGlobalLock called without matching EnterGlobalLock"); \\
    rex::thread::global_critical_region::mutex().unlock();                                \\
  } while (0)
#else
// patch_msr_lock.py (the-darkness-vr, 2026-11-10): the bracketed step is one compare-and-swap, atomic by itself
#define REX_CHECK_GLOBAL_LOCK() (u64{0x8000})
#define REX_ENTER_GLOBAL_LOCK() do { } while (0)
#define REX_LEAVE_GLOBAL_LOCK() do { } while (0)
#endif
'''


def main():
    path = sys.argv[1]
    s = open(path, encoding="utf-8").read()
    if "patch_msr_lock.py" in s:
        print("patch_msr_lock: already done")
        return 0
    m = re.search(r"#define REX_CHECK_GLOBAL_LOCK\(\).*?#define REX_LEAVE_GLOBAL_LOCK\(\).*?while \(0\)\n", s, re.S)
    if not m:
        print("patch_msr_lock: THE THREE MACROS WERE NOT FOUND, nothing changed")
        return 1
    open(path, "w", encoding="utf-8", newline="\n").write(s[:m.start()] + NEW + s[m.end():])
    print("patch_msr_lock: the three interrupt-lock macros are now lock-free")
    return 0


if __name__ == "__main__":
    sys.exit(main())
