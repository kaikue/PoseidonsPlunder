# Poseidon's Plunder
Final game for 15-466 Computer Game Programming

Server movement validation:
- Client sends unique ID (random int?), new position, shooting & other moves
- Server validates new position against old position- if not too far away (some multiple of max speed), update player's position
- Server sends players' positions to client- if self position is too far away, snap to that?
	- Could cause occasional jumping but will largely prevent teleporting with hacked clients