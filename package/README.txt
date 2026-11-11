reSplitFP 0.1.1 - TimeSplitters: Future Perfect (US Xbox), recompiled for the PS5
=================================================================================

A first test build. An unofficial fan port: the original Xbox game's code, statically recompiled into a native PS5
app. Not an emulator. No game files are included: it plays your own copy of the US (NTSC-U) Xbox disc.

What you need
-------------
- A PS5 that runs homebrew, with ShadowMountPlus (it registers apps in /data/homebrew) and an FTP server.
- Your own complete disc image (.iso) of TimeSplitters: Future Perfect, US Xbox release. A full dump and a trimmed
  one both work.
- Room on the console for the game's files (about 4 GB), and for the disc image itself until the first start has
  unpacked it (or put the image on a USB drive).

Install
-------
1. Unzip PPSA99065-<version>.zip on your computer. You get a folder named PPSA99065.
2. Over FTP, copy the whole PPSA99065 folder to /data/homebrew/ on the PS5.
   ShadowMountPlus adds a "reSplitFP" tile to the home screen (give it about 30 seconds).
3. Over FTP, create the folder /data/tsfp/ and copy your disc image into it, e.g. /data/tsfp/tsfp.iso.
   Or put it on a USB drive, in a folder named tsfp or at the top of the drive.
4. Start the reSplitFP tile. The first start unpacks the disc image into /data/tsfp/game/ (a screen shows how far
   it is; it takes a few minutes, longer from a USB drive), then the game starts. From then on the game starts from
   that folder straight away, and you can delete the .iso.

Already have the disc's files (default.xbe and the pak and xmv folders)? Copy them to /data/tsfp/game/ instead of an
.iso, or to tsfp/game/ on a USB drive. If there isn't room to unpack the image, the game reads the .iso directly (a
notification says so) and you keep the .iso there.

If something is wrong, a screen says what (game not found, a damaged image, another game or another release of it);
press Cross to close it.

To update, copy the new PPSA99065 folder over the old one. Your saves, settings and the game's files in /data/tsfp/
are kept.

Play
----
Start the reSplitFP tile. Player 1 is the user who started it. For split screen (up to four players), sign in more
users on the PS5 (guests are enough) and turn on their controllers.

  Xbox            DualSense
  A B X Y         Cross Circle Square Triangle
  Black / White   R1 / L1
  Triggers        L2 / R2
  Stick clicks    L3 / R3
  Start / Back    Options / touchpad click

The game runs in its own widescreen mode, full screen at 16:9.

Not yet in this build: the game's buttons and text are still the Xbox's, and the online modes (Xbox Live) are not
there.

In-game menu
------------
Press L3 + R3 (both sticks in) at any time. The game stands still behind it. Move with the d-pad or left stick,
Cross to choose, L1 / R1 to change page, and Circle, Options or L3 + R3 to go back to the game.

  Picture   render resolution (1080p, 1620p, 4K), V-Sync, frame rate counter, texture filtering (16x by default)
            and sharpness, smooth menus and HUD
  Controls  vibration, stick deadzone, trigger threshold

Changes apply at once and are saved when you close the menu.

Your files
----------
Everything the app writes is in /data/tsfp/:
  game/          the game's files, unpacked by the first start
  hdd/           your saves (the Xbox's hard disk)
  settings.toml  the menu's settings (you can edit it over FTP too)
  logs/          only when /data/tsfp/logging-on exists (see below)

Reporting a problem
-------------------
Create an empty file /data/tsfp/logging-on over FTP, start the game and do what goes wrong, then send the files in
/data/tsfp/logs/. Delete logging-on afterwards: without it nothing is logged, which keeps the game smooth.

Credits and licences: licenses/NOTICE.txt.
