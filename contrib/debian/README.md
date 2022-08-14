
Debian
====================
This directory contains files used to package jgcd/jgc-qt
for Debian-based Linux systems. If you compile jgcd/jgc-qt yourself, there are some useful files here.

## dash: URI support ##


jgc-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install jgc-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your jgc-qt binary to `/usr/bin`
and the `../../share/pixmaps/dash128.png` to `/usr/share/pixmaps`

jgc-qt.protocol (KDE)

