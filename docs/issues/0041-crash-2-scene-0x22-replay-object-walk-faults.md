---
id: 41
title: "Crash 2: a replay's object walk faults in scene 0x22 after a checkpoint restore"
status: open
symptom: the product aborts in scene 0x22 shortly after a spawn; a replayed Obj_Update faults in guest code
state_items: S007
tags: crash2,fps60,interpolation,crash
created: 2026-11-14
updated: 2026-11-14
---

Reproduce: unpaced, fps60, ires 0, `PSXPORT_DEBUG=cut`, `warp scene 0x22`, then `spawn 1` and `spawn 2` 4 s apart, no
input (`scratch/cuts/sweep.py s22.log 22`). Scene 0x22's spawn points put an idle Crash into a death loop: a
checkpoint restore (`Sys_LoadCheckpoint`, declared cut) about every 97 frames. After spawn 2, the first replay after a
restore reached the CD DMA channel's CHCR (`device-read 0x1F8010B8`) inside `Obj_UpdateAll` and was refused. On the
next frame `crash2::ObjectWalk`'s `callGuestNow(Obj_Update)` in the replay exited as a Lightrec memory fault at
0x80042FD0 (a GTE matrix load, 149 cycles in), and the required-return check aborted the process (exit 139). Seen twice
in two runs of the full sweep's timing; a rerun whose spawn landed at another point of the death loop did not fault.
The spawn request is timed by the wall clock. Two further runs that spawned 1, 2 and 3 six times each and
saved a state every 0.4 s (`scratch/cuts/catch22.py`, with and without the `cw` store watch) did not fault, though
each had three of the same CD DMA refusals; the refusal alone does not lead to the fault. The next step is still a
saved state just before the fault.

Not yet traced: which object the replay updated and whether it was live. Candidates to check from that state are
an object `ObjectWalk` kept from a walk the restore invalidated (killed, or its GOOL code paged out by the load the
refused replay was reaching for), and a guest fault inside a replay that is fatal where a device touch is a refusal.
