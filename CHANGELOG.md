# Changelog

## 0.1.6

- **L2 aims, R2 fires.** The Xbox game threw a grenade with the left trigger and aimed with a click of the right
  stick. Now L2 aims and R2 fires, as reSplit2 does for TimeSplitters 2; clicking the right stick throws the grenade,
  so nothing is lost. The game's Aim Mode stands: Toggle (its default) turns aiming on and off with each press of L2,
  Hold aims while L2 is held. The settings menu's **Left trigger aims** (Controls page) turns it off, for the Xbox
  layout. The game's own Controls screen shows its layout as it always did (Throw grenade on the left trigger, Aim on
  the right stick's click); those two lines are R3 and L2 now.
- What reSplit2 needed and Future Perfect does not: the game already waits for the next 60 Hz blank before each
  frame (its own frame timer), so it keeps its speed on any TV or with V-Sync off, and it has no one-pixel soft filter
  to take out.

## 0.1.5

- **Textures that flickered and changed with the player's movement.** Future Perfect draws its models with vertex
  arrays it pushes itself; the port makes a shader for each such draw from the selected program, and kept those by
  the address of the selection they were made from. Since 0.1.3 a selection is made anew whenever a load changes the
  slots it runs from, and a new one often took the memory of one just freed: the draw then got the shader made for
  the old one, with its program. Walls and floor were drawn with the reflective debris' program now and then, a
  different set of draws every few frames, worst in split screen. A shader made from another is now checked to come
  from that same selection still.

## 0.1.4

- **Invisible players, broken hands, lighting that changes as you move.** Future Perfect picks "the vertex program
  at slot 0" first and loads the program into slot 0 afterwards. The GPU draws with whatever the slots hold when it
  draws; the port took the program at the moment it was picked, so it drew with the one loaded there before. For the
  animated models (the other players, the enemies, your own hands) that was a program with no skeleton: the
  players vanished (only their guns showed), the hands came apart into floating shapes, and lit models took the
  previous model's lighting. The picked program is now taken again whenever a load changes the slots it runs from.

## 0.1.3

- **PlayStation buttons.** The game's Xbox buttons are drawn as PlayStation ones everywhere it shows them (menus,
  prompts, the weapon tips): A as Cross, B Circle, X Square, Y Triangle, White L1, Black R1, the triggers L2 and R2,
  and Start and Back as Options and the touch pad. reSplitFP's own drawings (tools/mkbuttons.py, game/art).
- **A PlayStation controller in Controls.** The Controls menu's picture of the Xbox controller, and the glow behind
  it, are a white PlayStation-style controller. Your own picture can go in its place for your own builds
  (`python3 tools/mkbuttons.py --image your-controller.png`; it stays out of the repository).
- **The PS5 in the game's text.** "Xbox Hard Disk" is "PS5 Storage", thumbsticks are sticks, the triggers L2 / R2,
  and the console is named a PS5, in every language the game carries. The text is in the front end's level file;
  it is served from memory rewritten, the game's files are never changed.
- **Models drawn with no program.** Future Perfect builds some of its vertex programs in pieces: a long one at the
  start of the GPU's program memory, its end then overwritten by small fragments. Program memory was kept as whole
  programs, so the first fragment took the long one away and those models were drawn with nothing (the log's
  "nothing loaded there"). It is now kept as the GPU's 136 instruction slots, a program running from its slot to its
  last instruction. Program uploads the library makes with its own GPU commands are taken too.

## 0.1.2

- **Past the logo films: the black screen.** After the films the game starts a thread that draws its loading
  screen. On the console that thread never ran, and the game waited for it on a black screen (0.1.1's log: "the game
  has stopped presenting"). Every game and system thread was given a 64 MB stack; a PS5 title's thread stacks come
  out of its own few hundred MB of memory, and the sixth stack didn't fit. Stacks are 16 MB on the PS5 now (the game
  plays its first level in the test harness with 2 MB), a failed thread is retried with a smaller one, and a thread
  that still can't be made is written to the log instead of vanishing.

## 0.1.1

- **The first start no longer says there is no room.** Once the disc image was unpacked, the app checked the folder
  for TimeSplitters 2's files instead of this game's, called the unpacked game incomplete, and played from the .iso
  (unpacking again at every start). An unpacked `/data/tsfp/game/` is now found and used.
- **The picture after the logo films.** Each frame cleared the screen to black and then drew the game over it, with
  nothing between the two to keep them in that order; a PS5's GPU may run them the other way round, leaving black.
  The clear now finishes first, and a picture that fills the screen isn't cleared at all.
- **A black or frozen screen now says why in the log.** When no frame has been shown for 5 seconds the log names
  what the renderer is waiting on (the GPU, the screen) or that the game itself stopped presenting.
- Two Vulkan images made with the uses their views and layouts need (found by Vulkan's validation layers).

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
