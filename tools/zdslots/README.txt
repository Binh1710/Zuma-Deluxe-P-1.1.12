ZDSlots - 6 Gauntlet save slots

Files
  zdslots.c        DLL source (slot panel, save path per slot, delete dialog, hooks)
  patch_slots.py   adds LoadLibraryA("zdslots.dll") stub at the exe entry point
  make_slotbox.py  builds images/slotbox.jpg + images/_slotbox.gif from the dialog box
  zdis.py          small disassembler / xref helper (needs: pip install capstone)

Build DLL (needs zig: pip install ziglang)
  python -m ziglang cc -target x86-windows-gnu -shared -Os -s -o zdslots.dll zdslots.c -lkernel32 -luser32

Patch exe (run on an exe that does NOT have the stub yet)
  python patch_slots.py ZDPlus.exe.in ZDPlus.exe
  python patch_slots.py ZDPlusO.exe.in ZDPlusO.exe

Save files
  slot 1      userdata/prc<user>.sav       (same name as before, old save = slot 1)
  slot 2..6   userdata/prc<user>_<slot>.sav
  <save>.inf  1 byte: 1 = Survival, 0 = Practice
Log: zdslots.log in the game folder.

Special balls (also in zdslots.dll)
  make_balls.py    builds images/zdx_rainbow.png, zdx_skull.png, zdx_frog.png, zdx_minifrog.png (needs Pillow)
  zdballs.ini      (game folder) spawn rates, points, how many balls the skull frog eats
  Stored as Ball power types 5 rainbow / 6 skull / 7 frog (+0x60), so saves keep them.
  Hooks: Ball vtable Draw 0x4fe3c0, Board vtable Draw 0x4fe87c, CountGroup 0x405730 (jmp),
         call Board::ActivatePower at 0x4175e8.

Timeless mode (also in zdslots.dll)
  make_timeless.py builds images/gaunttimelessbutton.png (108x22 cells, sits on the ledge under the statue; needs Pillow)
  zdballs.ini      [Timeless] Seconds = time on the clock
  A Practice board plus a countdown; at 0 it calls Board::LoseLife 0x421430, the routine used
  when a ball reaches the hole. High scores use tables prefixed t_. Save slots: <save>.inf is
  now 5 bytes: mode (0 practice, 1 survival, 2 timeless) + frames left.
  Hooks: PracticeScreen vtable Added 0x502b08, Removed 0x502b0c, Resize 0x502b38, listener
  ButtonDepress 0x502af4; HUD cave 0x4fde90; label 0x439394; score prefix 0x42138c 0x43b713
  0x43b72b; refresh calls 0x43bee1 0x43bf83; save-score calls 0x421515 0x43553c.
