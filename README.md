# Mines

A simple TUI Minesweeper game made in C.

## Building

> [!NOTE]
> If you have [Nix](https://nixos.org) on your system, you can try out the game with
>
> ```bash
> nix run git+https://codeberg.org/Grazen0/mines
> ```

This project is managed with [Meson](https://mesonbuild.com). You can compile and run the game with the following commands:

```bash
meson setup build
meson compile -C build
build/mines
```
