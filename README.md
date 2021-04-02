# Game Engine

## Overview
A game engine to help with rendering objects, interfacing with a window, and keeping track of game state. I've been implementing things as needed so it's not really in a final state.

## Modules
### Window
This module handles drawing the actual window, and processing keyboard/mouse events.

### Engine
This module should probably be renamed to something like `gl` since it only provides wrappers around the raw OpenGL calls to make it harder to mess up.

### ECS
This is an entitiy component system implementation which makes things like state updating, rendering, and serialization fast and easy.
