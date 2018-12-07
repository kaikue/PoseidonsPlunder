# Poseidon's Plunder
Final game for 15-466 Computer Game Programming

Server movement validation:
- Client sends unique ID (random int?), new position, shooting & other moves
- Server validates new position against old position- if not too far away (some multiple of max speed), update player's position
- Server sends players' positions to client- if self position is too far away, snap to that?
	- Could cause occasional jumping but will largely prevent teleporting with hacked clients

- Build instructions (windows):
	- Needs kit-lib-win and bullet built in local directory
	- Run jam

## Building bullet
- Clone from https://github.com/bulletphysics/bullet3
- Run build_visual_studio_without_pybullet_vr.bat (you can edit this to be a more current visual studio version, but it shouldn't matter)
- Open the generated project in VS 2017
- Open config manager
	- Create new solution platform x64
- Build > Build Solutions (ctrl shift B)
	- you might have to repeat this a few times, since the dependencies are bad
	- I think you only really need to build: BulletCollision, BulletDynamics, LinearMath
		- Need to set /md flags on each project
- Put the built files into PoseidonsPlunder/bullet/build, and the source (headers) into PoseidonsPlunder/bullet/src
- Or just use Kai's prebuilt bullet: https://www.dropbox.com/s/9bh7d5jfm2hbtxq/bullet.zip?dl=0

## Credits
- Developed by Edward Terry, Eric Fang, I-Chen Jwo, and Kai Kuehner
- Harpoon sound by Bird_man: https://freesound.org/people/Bird_man/sounds/275151/
- Swim sound by monica137142: https://freesound.org/people/monica137142/sounds/211389/
