# Changelog

## 0.1.0

The first test build: TimeSplitters: Future Perfect (US Xbox) statically recompiled into a native PS5 app.

- **The game on the PS5.** The Xbox game's code is recompiled to run natively, drawing through Vulkan (Mesa RADV)
  at up to 4K. Tested so far in a desktop test harness: the logo films, the menus, a new profile, Story's briefing,
  the opening cutscene and the first level.
- **Widescreen.** The game's own widescreen mode, full screen at 16:9; the 4:3 films keep their shape.
- **Install it as reSplit2 installs.** The first start unpacks your disc image into `/data/tsfp/game/`.
- **Up to four players**, and the in-game menu (L3 + R3).

What it took to run the game, for those following along (in the SDK, so the other ports can use it):
- The films: the XMV player shows each frame through the console's video overlay, which wrote hardware registers;
  the overlay is now drawn into the picture.
- Swap in the XDK's copy mode: Future Perfect presents with two Swap calls a frame, which showed every frame twice,
  the second time the one before.
- Vertex arrays the game sets itself with raw GPU commands (its models): they were dropped, and characters were
  invisible.
- Fog: the game's pixel shaders read the fog the other way round from the XDK's own; the level and the weapons
  were painted the colour of the fog.
- Library functions found by hand: the default vertex shader the console points the device at, BeginPush, the
  visibility tests, shader programs, sound stream positions.

## Thanks to

- Athelas by Shagg, the SDK this port builds on
- XboxXBE-Recomp by TGP482
- reUndr-ps5 by bystritskiy, the model for this port, reKing and reSplit2
- Cxbx-Reloaded's XbSymbolDatabase
- mihawk-99's PS5_Vulkan, PS5_Mesa and PS5_PayloadSDK
