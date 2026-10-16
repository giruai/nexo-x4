# DualShock 4 input

`controller.c` initializes Pad and UserService, reads controller state on the main thread, and tracks both held buttons and press transitions. Read errors produce neutral input while the application retries opening a controller.

`gamepad.h` defines the portable cloud input report, and the streaming path sends current game state when a session is active. The PSBox UI consumes navigation and in-stream menu actions locally so menu input is not forwarded as game input. Rumble is disabled in the v0.1.0 release configuration.
