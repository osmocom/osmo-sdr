@echo off
mkdir obj
gcc -Wall -DWINDOWS -O2 -s -Werror -ffunction-sections -fdata-sections -c src/main.c -o obj/main.o
gcc -Wall -DWINDOWS -O2 -s -Werror -ffunction-sections -fdata-sections -c src/osmosdr.c -o obj/osmosdr.o
gcc -Wall -DWINDOWS -O2 -s -Werror -ffunction-sections -fdata-sections -c src/sam3u.c -o obj/sam3u.o
gcc -Wall -DWINDOWS -O2 -s -Werror -ffunction-sections -fdata-sections -c src/serial.c -o obj/serial.o
gcc -Wall -DWINDOWS -O2 -s -Werror -ffunction-sections -fdata-sections -c src/utils.c -o obj/utils.o
gcc -Wall -DWINDOWS -O2 -s -Werror -ffunction-sections -fdata-sections -c src/lattice/hardware.c -o obj/hardware.o
gcc -Wall -DWINDOWS -O2 -s -Werror -ffunction-sections -fdata-sections -c src/lattice/slim_pro.c -o obj/slim_pro.o
gcc -Wall -DWINDOWS -O2 -s -Werror -ffunction-sections -fdata-sections -c src/lattice/slim_vme.c -o obj/slim_vme.o
gcc -static -Wl,-Map=rumba.map -s -Wl,--gc-sections -o rumba.exe -Wl,--start-group obj/main.o obj/osmosdr.o obj/sam3u.o obj/serial.o obj/utils.o obj/hardware.o obj/slim_pro.o obj/slim_vme.o --Wl,--end-group
