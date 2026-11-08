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
