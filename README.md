A gba project using the Angry Goose Engine.

To run -
1. git submodule update --init --recursive
2. mkdir build
3. cd build
4. cmake -S .. .
5. cmake --build . --config Release
6. cmake --install . --prefix package
7. Place roms in package/media/roms
8. ./package/gba
