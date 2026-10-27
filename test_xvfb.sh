#!/bin/bash
rm -f /tmp/meinos_debug.log
sed -i 's/load_a3d("\/opt\/meinos\/model_0.glb");/load_a3d("\/opt\/meinos\/model_10.glb");/g' linux_main.cpp

sed -i 's/Material& mat = materials\[prim.material_idx\];/Material\& mat = materials[prim.material_idx]; if (prim.material_idx == 0) printf("DRAW mat.texture_id = %u\\n", mat.texture_id);/' avatar_anim.cpp

export DISPLAY=:99
Xvfb :99 -screen 0 1024x768x24 &
XVFB_PID=$!
sleep 1
make -f Makefile.linux clean
make -f Makefile.linux
./meinos_v2.elf > /tmp/draw_log.txt &
MEIN_PID=$!
sleep 10
kill -INT $MEIN_PID
kill -INT $XVFB_PID
cat /tmp/draw_log.txt | grep "DRAW mat" | head -n 10

sed -i 's/load_a3d("\/opt\/meinos\/model_10.glb");/load_a3d("\/opt\/meinos\/model_0.glb");/g' linux_main.cpp
git checkout avatar_anim.cpp
