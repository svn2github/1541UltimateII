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
#ifndef ATAMANAGER_H
#define ATAMANAGER_H

#include "blockdev.h"
#include "indexed_list.h"

class ATAManager
{
    IndexedList<BlockDevice *>children;
public:
    BlockDevice *current;
    ATAManager(void) : children(4, NULL) {
        current = NULL;
    }

    BlockDevice *select(void);
    void attach(BlockDevice *b);
    void detach(BlockDevice *b);
};

extern ATAManager atamanager;

#endif
