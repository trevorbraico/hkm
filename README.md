# HKM
#### Hyprland Keyboard Mouse
*********

### Introduction

HKM is a lightweight desktop utility for Wayland that enables full keyboard-based on-screen navigation.

### Installation

* Install from AUR: `yay -S hkm`

### Configuration

### Usage

You'll want to configure your Wayland compositor to register HKM as a global keybind

* **Hyprland**: `echo "bind = ALT, h, execr, hkm -t  # toggle HKM" >> $XDG_CONFIG_HOME/hypr/hyprland.conf`

