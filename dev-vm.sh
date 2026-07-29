#!/bin/bash
# Rebuild HisokaOS and (re)launch the local VNC VM. Run after any change.
# Connect with: vnc://localhost:5900   password: hisoka
cd ~/Projects/hisoka-os || exit 1
pkill -f "qemu-system-i386 -kernel hisoka.elf" 2>/dev/null; sleep 0.5
rm -f /tmp/hisoka-mon.sock
make 2>&1 | tail -1 || exit 1
[ -f disk.img ] || qemu-img create -f raw disk.img 32M >/dev/null 2>&1

# host helpers HisokaOS streams from (Chromium browser + Alexis assistant). Start if down.
pgrep -f "chrome-server.py" >/dev/null || nohup python3 chrome-server.py >/tmp/chrome-server.log 2>&1 &
pgrep -f "alexis-server.py" >/dev/null || nohup python3 alexis-server.py >/tmp/alexis-server.log 2>&1 &

qemu-system-i386 -kernel hisoka.elf -m 256M -vnc 127.0.0.1:0,password=on -name "HisokaOS" \
  -drive file=disk.img,format=raw,if=ide \
  -netdev user,id=n0 -device rtl8139,netdev=n0 \
  -audiodev coreaudio,id=snd0 -machine pcspk-audiodev=snd0 \
  -serial file:/tmp/hisoka-serial.log \
  -monitor unix:/tmp/hisoka-mon.sock,server,nowait &
sleep 2
# set the VNC password so macOS Screen Sharing can connect
/usr/bin/python3 -c 'import socket,time; s=socket.socket(socket.AF_UNIX); s.connect("/tmp/hisoka-mon.sock"); time.sleep(0.3); s.recv(4096); s.sendall(b"set_password vnc hisoka\n"); time.sleep(0.3)'
echo "HisokaOS live on vnc://localhost:5900  (password: hisoka)"
