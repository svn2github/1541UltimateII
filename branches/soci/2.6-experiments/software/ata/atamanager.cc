/*
    ATA interface emulation
    Copyright (C) 2016 Kajtar Zsolt (soci@c64.rulez.org)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#include <stdio.h>
extern "C" {
    #include "small_printf.h"
}
#include "atamanager.h"

ATAManager atamanager;

BlockDevice * ATAManager :: select(void) {
    if (!current && !children.is_empty()) current = children[children.get_elements() - 1];
    return current;
}

void ATAManager :: attach(BlockDevice *b) {
    children.append(b);
}

void ATAManager :: detach(BlockDevice *b) {
    children.remove(b);
    if (current == b) {
        current = NULL;
    }
}
