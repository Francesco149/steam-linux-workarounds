works around various issues with steam and other programs running over nfs
or trying to access a >2TB drive.

flock doesn't work over NFSv4, so flock calls are translated to fcntl

based on DataBeaver's solution https://gist.github.com/DataBeaver/0aa46844c8e1788207fc882fc2a221f6

32-bit applications such as steam or 32-bit windows program running under
wine can't handle >2TB drives and will think there's no available space.
this makes all statfs/fstatfs calls always report 1099511627264 bytes
of free space, which is rougly 1TB if your block size matches 4096. even
if you have an actual block size of 512, it'll still be around 250GB
which is more than enough. this also makes the stat calls ignore failure if
errno is EOVERFLOW(75)

based on:
* ryao's solution: https://github.com/ValveSoftware/steam-for-linux/issues/3226#issuecomment-422869718
* nonchip's solution: https://github.com/nonchip/steam_workaround_fsoverflow

# usage:

compile and put the library somewhere (install gcc if you don't have it):

```sh
sudo gcc -shared flock_to_setlk.c -ldl -o /usr/lib/libfakeflock.so
sudo gcc -m32 -shared flock_to_setlk.c -ldl -o /usr/lib32/libfakeflock.so
```

export LD_PRELOAD with the two libs when running the desired programs

```sh
LD_PRELOAD="/usr/lib/libfakeflock.so /usr/lib32/libfakeflock.so" steam
```

note that some distros override LD_PRELOAD in the steam script, for
example void linux does this. in those cases, you'll have to edit the
script (```/usr/bin/steam``` in void's case) and change LD_PRELOAD there.

for example, the void linux line with my lib appended looks like this:

```
export  LD_PRELOAD='/usr/$LIB/libstdc++.so.6 /usr/$LIB/libgcc_s.so.1 /usr/$LIB/libxcb.so.1 /usr/$LIB/libfakeflock.so'
```
