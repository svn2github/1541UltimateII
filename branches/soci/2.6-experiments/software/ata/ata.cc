/*
    ATA interface emulation
    Copyright (C) 2015 Kajtar Zsolt (soci@c64.rulez.org)

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
#include <stdlib.h>
#include <string.h>

extern "C" {
	#include "itu.h"
	#include "dump_hex.h"
    #include "small_printf.h"
}
#include "ata.h"
#include "atamanager.h"
#include "blockdev.h"

#define putw(a,b) {buf[(a) * 2] = (b);buf[(a) * 2 + 1] = (b) >> 8;}

#define ATA_SERIAL_NUMBER "1"
#define ATA_REVISION "2"
#define ATA_COPYRIGHT "KAJTAR ZSOLT (SOCI/SINGULAR)"

//--------------------------------------------------------------
// ATA Drive Class
//--------------------------------------------------------------

void ATA :: diag(struct drive_s *drv)
{
    DWORD size = 0;
    BlockDevice *device = *drv->device;
    if (device) device->ioctl(GET_SECTOR_COUNT, &size);
    calc_geometry(drv, (int)size);

    drv->cmd = 0;
    drv->registers[ATA_ERROR] = device != NULL;
    drv->registers[ATA_COUNT] = device != NULL;
    drv->registers[ATA_SECTOR] = device != NULL;
    drv->registers[ATA_CYLINDER_LOW] = 0;
    drv->registers[ATA_CYLINDER_HIGH] = 0;
    drv->registers[ATA_HEAD] = 0;
    drv->write_cache = 0;
    drv->write_error = 0;
}

void ATA :: set_string(volatile BYTE *b, char *s, int n)
{
    int i;

    for (i = 0; i < n; i += 2) {
        b[i | 1] = *s ? *s++ : 0x20;
        b[i] = *s ? *s++ : 0x20;
    }
}

int ATA :: get_lba(struct drive_s *drv)
{
    int lba;
    BYTE head = drv->registers[ATA_HEAD];
    int cylinder = (drv->registers[ATA_CYLINDER_HIGH] << 8) | drv->registers[ATA_CYLINDER_LOW];
    BYTE sector = drv->registers[ATA_SECTOR];
    if (head & 0x40) {
        head &= 15;
        lba = (head << 24) | (cylinder << 8) | sector;
    } else {
        head &= 15;
        if (sector == 0 || sector > drv->actual.sectors || head >= drv->actual.heads ||
                cylinder >= drv->actual.cylinders) {
            return -1;
        }
        lba = (cylinder * drv->actual.heads + head) * drv->actual.sectors + sector - 1;
    }
    return lba < drv->geometry.size ? lba : -1;
}

void ATA :: set_lba(struct drive_s *drv, int lba)
{
    int tmp;
    BYTE head = drv->registers[ATA_HEAD] & 0xf0;
    if (head & 0x40) {
        drv->registers[ATA_SECTOR] = lba;
        drv->registers[ATA_CYLINDER_LOW] = lba >> 8;
        drv->registers[ATA_CYLINDER_HIGH] = lba >> 16;
        drv->registers[ATA_HEAD] = head | (lba >> 24);
        return;
    } 
    if (!drv->actual.sectors || drv->actual.heads) return;
    tmp = lba / drv->actual.sectors;
    drv->registers[ATA_SECTOR] = lba - tmp * drv->actual.sectors + 1;
    lba = tmp / drv->actual.heads;
    drv->registers[ATA_HEAD] = head | (tmp - lba * drv->actual.heads);
    drv->registers[ATA_CYLINDER_LOW] = lba;
    drv->registers[ATA_CYLINDER_HIGH] = lba >> 8;
}

void ATA :: calc_geometry(struct drive_s *drv, int size)
{
    int i, c, h, s;

    drv->geometry.size = size;
    if (size > 16514064) size = 16514064;
    h = 1; s = 1; i = 63; c = size;
    while (i > 1 && c > 1) {
        if ((c % i) == 0) {
            if (s * i <= 63) {
                s *= i; c /= i;
                continue;
            }
            if (h * i <= 16) {
                h *= i; c /= i;
                continue;
            }
        }
        i--;
    }
    for (;;) {
        if (size <= 1032192) {
            if (c <= 1024) break;
        } else {
            if (h < 5 && c < 65536) break;
            if (h < 9 && c < 32768) break;
            if (c < 16384) break;
        }
        if (s == 63 && h < 16) h++;
        if (s < 63) s++;
        c = size / (h * s);
    }
    drv->geometry.cylinders = c;
    drv->geometry.heads = h;
    drv->geometry.sectors = s;
    drv->actual.cylinders = c;
    drv->actual.heads = h;
    drv->actual.sectors = s;
}

ATA :: ATA(volatile BYTE *regs, volatile BYTE *buf, volatile BYTE *hwreg)
{
    hwregister = hwreg;
    *hwregister = ATA_BSY;
    drives[0].registers = regs;
    drives[0].buffer = buf;
    drives[0].device = &atamanager.current;
    drives[1].registers = regs + 16;
    drives[1].buffer = buf + 8192;
    drives[1].device = &atamanager.current2;
    *hwregister = ATA_BSY | ATA_DRDY;
}

ATA :: ~ATA()
{
    *hwregister = ATA_BSY | ATA_DRDY;
}

void ATA :: poll(Event &e)
{
    int i;
    BYTE result, error;
    volatile BYTE *buf;
    BYTE hw = *hwregister;
    BlockDevice *device;
    struct drive_s *drv = &drives[hw & ATA_SEL];

    if (hw & ATA_RST) {
        if (hw & ATA_DRDY) return;
        *hwregister = ATA_BSY | ATA_DRDY;
        return;
    }
    if (hw & ATA_DRDY) {
        atamanager.select();
        diag(&drives[0]);
        atamanager.select2();
        diag(&drives[1]);
        *hwregister = 0;
        return;
    }
    if (hw & ATA_CMD) {
        *hwregister = ATA_BSY;
        result = 0; error = 0; drv->cmd = 0;
        buf = drv->buffer;
        switch (drv->registers[ATA_COMMAND]) {
        case 0x20: // READ SECTORS
        case 0x21:
            drv->lba = get_lba(drv);
            if (drv->lba < 0) {
                error = ATA_IDNF;
                break;
            }
            device = *drv->device;
            if (device && device->read((BYTE *)buf, drv->lba, 1) == RES_OK) {
                drv->count = drv->registers[ATA_COUNT] - 1;
                drv->cmd = 1;
                drv->fill = drv->pos = 1;
                drv->lba++;
                result = ATA_DRQ;
                break;
            }
            error = ATA_ABRT | ATA_UNC;
            break;
        case 0x30: // WRITE SECTORS
        case 0x31:
            drv->lba = get_lba(drv);
            if (drv->lba < 0) {
                error = ATA_IDNF;
                break;
            }
            drv->count = drv->registers[ATA_COUNT];
            drv->fill = 0;
            drv->cmd = 3;
            result = ATA_DRQ;
            break;
        case 0x40: // VERIFY SECTORS
        case 0x41:
            drv->lba = get_lba(drv);
            if (drv->lba < 0) {
                error = ATA_IDNF;
                break;
            }
            drv->count = drv->registers[ATA_COUNT];
            device = *drv->device;
            while (--drv->count) {
                if (device && device->read((BYTE *)buf, drv->lba, 1) != RES_OK) {
                    set_lba(drv, drv->lba);
                    error = ATA_ABRT | ATA_UNC;
                    break;
                }
                drv->lba++;
            }
            break;
        case 0x70: // SEEK
            drv->lba = get_lba(drv);
            if (drv->lba < 0) {
                error = ATA_IDNF;
                break;
            }
            break;
        case 0x90: // EXECUTE DEVICE DIAGNOSTIC
            atamanager.select();
            diag(&drives[0]);
            atamanager.select2();
            diag(&drives[1]);
            *hwregister = 0;
            return;
        case 0x91: // INITIALIZE DEVICE PARAMETERS
            drv->actual.heads = (drv->registers[ATA_HEAD] & 15) + 1;
            drv->actual.sectors = drv->registers[ATA_COUNT];
            if (drv->actual.sectors < 1 || drv->actual.sectors > 63) {
                drv->actual.cylinders = 0;
            } else {
                int size = drv->geometry.size;
                if (size > 16514064) size = 16514064;
                size /= drv->actual.heads * drv->actual.sectors;
                drv->actual.cylinders = (size > 65535) ? 65535 : size;
            }
            if (drv->actual.cylinders == 0) {
                drv->actual.heads = 0;
                drv->actual.sectors = 0;
                error = ATA_ABRT;
                break;
            }
            break;
        case 0xe7: // FLUSH CACHE
            if (drv->write_error) {
                error = drv->write_error;
                set_lba(drv, drv->error_lba);
                drv->write_error = 0;
            }
            break;
        case 0xec: // IDENTIFY DEVICE
            memset((void *)buf, 0, 512);
            putw(0, 0x0040);
            putw(1, drv->geometry.cylinders);
            putw(3, drv->geometry.heads);
            putw(6, drv->geometry.sectors);
            putw(7, drv->geometry.size >> 16);
            putw(8, drv->geometry.size);
            set_string(buf + 20, ATA_SERIAL_NUMBER, 20);
            putw(21, ATA_BUF + 1);
            set_string(buf + 46, ATA_REVISION, 8);
            set_string(buf + 54, "ATA-BR " ATA_COPYRIGHT, 40);
            putw(49, 1 << 9); /* LBA support */
            if (drv->actual.sectors) {
                putw(53, 1 << 0);
                putw(54, drv->actual.cylinders);
                putw(55, drv->actual.heads);
                putw(56, drv->actual.sectors);
                i = drv->actual.cylinders * drv->actual.heads * drv->actual.sectors;
                if (i > drv->geometry.size) i = drv->geometry.size;
                putw(57, i);
                putw(58, i >> 16);
            }
            putw(60, drv->geometry.size);
            putw(61, drv->geometry.size >> 16);
            i  = 1 << 5;  /* write cache */
            i |= 1 << 12; /* write buffer */
            i |= 1 << 13; /* read buffer */
            putw(82, i);
            i  = 1 << 12; /* flush cache */
            i |= 1 << 14;
            putw(83, i);
            putw(84, 1 << 14);
            i  = drv->write_cache << 5; /* write cache */
            i |= 1 << 12; /* write buffer */
            i |= 1 << 13; /* read buffer */
            putw(85, i);
            putw(86, 1 << 12); /* flush cache */
            putw(87, 1 << 14);
            putw(255, 0xa5);
            for (i = 0; i < 511; i++) {
                buf[511] -= buf[i];
            }
            result = ATA_DRQ;
            break;
        case 0xe4: // READ BUFFER
        case 0xe8: // WRITE BUFFER
            result = ATA_DRQ;
            break;
        case 0xef: // SET FEATURES
            switch (drv->registers[ATA_FEATURE]) {
            case 0x02: // SET ENABLE WRITE CACHE
                drv->write_cache = 1;
                break;
            case 0x03: // SET TRANSFER MODE
                i = drv->registers[ATA_COUNT];
                if (i > 1 && i != 8) {
                    error = ATA_ABRT;
                }
                break;
            case 0x82: // SET DISABLE WRITE CACHE
                drv->write_cache = 0;
                break;
            default:
                error = ATA_ABRT;
                break;
            }
            break;
        default:
            error = ATA_ABRT;
            break;
        }
        drv->registers[ATA_ERROR] = error;
        *hwregister = error ? ATA_ERR : result;
        return;
    }
    if (drv->cmd == 1 && drv->count && drv->fill < ATA_BUF && !((hw & ATA_DATA) && drv->fill > 1)) {// READ SECTORS
        device = *drv->device;
        if (device && device->read((BYTE *)drv->buffer + (drv->pos << 9), drv->lba, 1) == RES_OK) {
            drv->count--;
            drv->fill++;
            drv->lba++;
            drv->pos = (drv->pos + 1) & ATA_BUF;
        } else drv->cmd = 2;
    }
    if (hw & ATA_DATA) {
        *hwregister = ATA_BSY;
        result = 0; error = 0;
        switch (drv->cmd) {
        case 1: // READ SECTORS
        case 2:
            if (--drv->fill) {
                result = ATA_DRQ;
                break;
            }
            if (drv->cmd == 2) {
                set_lba(drv, drv->lba);
                error = ATA_ABRT | ATA_UNC;
            }
            drv->cmd = 0;
            break;
        case 3: // WRITE SECTORS
            drv->fill++;
            if (--drv->count) {
                result = ATA_DRQ;
                if (drv->fill <= ATA_BUF) break;
            } else {
                drv->cmd = 0;
                if (drv->write_cache) {
                    drv->registers[ATA_ERROR] = 0;
                    *hwregister = 0; // Early response, assume all will go fine...
                }
            }
            device = *drv->device;
            if (device && device->write((BYTE *)drv->buffer, drv->lba, drv->fill) == RES_OK) {
                drv->lba += drv->fill; drv->fill = 0;
                if (!drv->count && drv->write_cache) return; // And all went fine
                break;
            }
            drv->cmd = 0;
            error = ATA_ABRT | ATA_UNC;
            if (drv->write_cache) { // Too late for error...
                drv->write_error = error;
                drv->error_lba = drv->lba;
                return; 
            }
            set_lba(drv, drv->lba);
            break;
        default:
            break;
        }
        drv->registers[ATA_ERROR] = error;
        *hwregister = error ? ATA_ERR : result;
        return;
    }
}

