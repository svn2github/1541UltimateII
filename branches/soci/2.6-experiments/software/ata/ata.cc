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

void ATA :: diag(void)
{
    DWORD size = 0;
    BlockDevice *device = atamanager.select();
    if (device) device->ioctl(GET_SECTOR_COUNT, &size);
    calc_geometry((int)size);

    cmd = 0;
    registers[ATA_ERROR] = 1;
    registers[ATA_COUNT] = 1;
    registers[ATA_SECTOR] = 1;
    registers[ATA_CYLINDER_LOW] = 0;
    registers[ATA_CYLINDER_HIGH] = 0;
    registers[ATA_HEAD] = 0;
    write_cache = 0;
    write_error = 0;
}

void ATA :: set_string(volatile BYTE *b, char *s, int n)
{
    int i;

    for (i = 0; i < n; i += 2) {
        b[i | 1] = *s ? *s++ : 0x20;
        b[i] = *s ? *s++ : 0x20;
    }
}

int ATA :: get_lba(void)
{
    int lba;
    BYTE head = registers[ATA_HEAD];
    int cylinder = (registers[ATA_CYLINDER_HIGH] << 8) | registers[ATA_CYLINDER_LOW];
    BYTE sector = registers[ATA_SECTOR];
    if (head & 0x40) {
        head &= 15;
        lba = (head << 24) | (cylinder << 8) | sector;
    } else {
        head &= 15;
        if (sector == 0 || sector > actual.sectors || head >= actual.heads ||
                cylinder >= actual.cylinders) {
            return -1;
        }
        lba = (cylinder * actual.heads + head) * actual.sectors + sector - 1;
    }
    return lba < geometry.size ? lba : -1;
}

void ATA :: set_lba(int lba)
{
    int tmp;
    BYTE head = registers[ATA_HEAD] & 0xf0;
    if (head & 0x40) {
        registers[ATA_SECTOR] = lba;
        registers[ATA_CYLINDER_LOW] = lba >> 8;
        registers[ATA_CYLINDER_HIGH] = lba >> 16;
        registers[ATA_HEAD] = head | (lba >> 24);
        return;
    } 
    if (!actual.sectors || actual.heads) return;
    tmp = lba / actual.sectors;
    registers[ATA_SECTOR] = lba - tmp * actual.sectors + 1;
    lba = tmp / actual.heads;
    registers[ATA_HEAD] = head | (tmp - lba * actual.heads);
    registers[ATA_CYLINDER_LOW] = lba;
    registers[ATA_CYLINDER_HIGH] = lba >> 8;
}

void ATA :: calc_geometry(int size)
{
    int i, c, h, s;

    geometry.size = size;
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
    geometry.cylinders = c;
    geometry.heads = h;
    geometry.sectors = s;
    actual.cylinders = c;
    actual.heads = h;
    actual.sectors = s;
}

ATA :: ATA(volatile BYTE *regs, volatile BYTE *buf, volatile BYTE *hwreg)
{
    hwregister = hwreg;
    *hwregister = ATA_BSY;
    registers = regs;
    buffer = buf;
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

    if (hw & ATA_RST) {
        if (hw & ATA_DRDY) return;
        *hwregister = ATA_BSY | ATA_DRDY;
        return;
    }
    if (hw & ATA_DRDY) {
        diag();
        *hwregister = 0;
        return;
    }
    if (hw & ATA_CMD) {
        *hwregister = ATA_BSY;
        result = 0; error = 0; cmd = 0;
        buf = buffer;
        switch (registers[ATA_COMMAND]) {
        case 0x20: // READ SECTORS
        case 0x21:
            lba = get_lba();
            if (lba < 0) {
                error = ATA_IDNF;
                break;
            }
            device = atamanager.current;
            if (device && device->read((BYTE *)buf, lba, 1) == RES_OK) {
                count = registers[ATA_COUNT] - 1;
                cmd = 1;
                fill = pos = 1;
                lba++;
                result = ATA_DRQ;
                break;
            }
            error = ATA_ABRT | ATA_UNC;
            break;
        case 0x30: // WRITE SECTORS
        case 0x31:
            lba = get_lba();
            if (lba < 0) {
                error = ATA_IDNF;
                break;
            }
            count = registers[ATA_COUNT];
            fill = 0;
            cmd = 3;
            result = ATA_DRQ;
            break;
        case 0x40: // VERIFY SECTORS
        case 0x41:
            lba = get_lba();
            if (lba < 0) {
                error = ATA_IDNF;
                break;
            }
            count = registers[ATA_COUNT];
            device = atamanager.current;
            while (--count) {
                if (device && device->read((BYTE *)buf, lba, 1) != RES_OK) {
                    set_lba(lba);
                    error = ATA_ABRT | ATA_UNC;
                    break;
                }
                lba++;
            }
            break;
        case 0x70: // SEEK
            lba = get_lba();
            if (lba < 0) {
                error = ATA_IDNF;
                break;
            }
            break;
        case 0x90: // EXECUTE DEVICE DIAGNOSTIC
            diag();
            *hwregister = 0;
            return;
        case 0x91: // INITIALIZE DEVICE PARAMETERS
            actual.heads = (registers[ATA_HEAD] & 15) + 1;
            actual.sectors = registers[ATA_COUNT];
            if (actual.sectors < 1 || actual.sectors > 63) {
                actual.cylinders = 0;
            } else {
                int size = geometry.size;
                if (size > 16514064) size = 16514064;
                size /= actual.heads * actual.sectors;
                actual.cylinders = (size > 65535) ? 65535 : size;
            }
            if (actual.cylinders == 0) {
                actual.heads = 0;
                actual.sectors = 0;
                error = ATA_ABRT;
                break;
            }
            break;
        case 0xe7: // FLUSH CACHE
            if (write_error) {
                error = write_error;
                set_lba(error_lba);
                write_error = 0;
            }
            break;
        case 0xec: // IDENTIFY DEVICE
            memset((void *)buf, 0, 512);
            putw(0, 0x0040);
            putw(1, geometry.cylinders);
            putw(3, geometry.heads);
            putw(6, geometry.sectors);
            putw(7, geometry.size >> 16);
            putw(8, geometry.size);
            set_string(buf + 20, ATA_SERIAL_NUMBER, 20);
            putw(21, ATA_BUF + 1);
            set_string(buf + 46, ATA_REVISION, 8);
            set_string(buf + 54, "ATA-SD " ATA_COPYRIGHT, 40);
            putw(49, 1 << 9); /* LBA support */
            if (actual.sectors) {
                putw(53, 1 << 0);
                putw(54, actual.cylinders);
                putw(55, actual.heads);
                putw(56, actual.sectors);
                i = actual.cylinders * actual.heads * actual.sectors;
                if (i > geometry.size) i = geometry.size;
                putw(57, i);
                putw(58, i >> 16);
            }
            putw(60, geometry.size);
            putw(61, geometry.size >> 16);
            i  = 1 << 5;  /* write cache */
            i |= 1 << 12; /* write buffer */
            i |= 1 << 13; /* read buffer */
            putw(82, i);
            i  = 1 << 12; /* flush cache */
            i |= 1 << 14;
            putw(83, i);
            putw(84, 1 << 14);
            i  = write_cache << 5; /* write cache */
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
            switch (registers[ATA_FEATURE]) {
            case 0x02: // SET ENABLE WRITE CACHE
                write_cache = 1;
                break;
            case 0x03: // SET TRANSFER MODE
                i = registers[ATA_COUNT];
                if (i > 1 && i != 8) {
                    error = ATA_ABRT;
                }
                break;
            case 0x82: // SET DISABLE WRITE CACHE
                write_cache = 0;
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
        registers[ATA_ERROR] = error;
        *hwregister = error ? ATA_ERR : result;
        return;
    }
    if (cmd == 1 && count && fill < ATA_BUF && !((hw & ATA_DATA) && fill > 1)) {// READ SECTORS
        device = atamanager.current;
        if (device && device->read((BYTE *)buffer + (pos << 9), lba, 1) == RES_OK) {
            count--;
            fill++;
            lba++;
            pos = (pos + 1) & ATA_BUF;
        } else cmd = 2;
    }
    if (hw & ATA_DATA) {
        *hwregister = ATA_BSY;
        result = 0; error = 0;
        switch (cmd) {
        case 1: // READ SECTORS
        case 2:
            if (--fill) {
                result = ATA_DRQ;
                break;
            }
            if (cmd == 2) {
                set_lba(lba);
                error = ATA_ABRT | ATA_UNC;
            }
            cmd = 0;
            break;
        case 3: // WRITE SECTORS
            fill++;
            if (--count) {
                result = ATA_DRQ;
                if (fill <= ATA_BUF) break;
            } else {
                cmd = 0;
                if (write_cache) {
                    registers[ATA_ERROR] = 0;
                    *hwregister = 0; // Early response, assume all will go fine...
                }
            }
            device = atamanager.current;
            if (device && device->write((BYTE *)buffer, lba, fill) == RES_OK) {
                lba += fill; fill = 0;
                if (!count && write_cache) return; // And all went fine
                break;
            }
            cmd = 0;
            error = ATA_ABRT | ATA_UNC;
            if (write_cache) { // Too late for error...
                write_error = error;
                error_lba = lba;
                return; 
            }
            set_lba(lba);
            break;
        default:
            break;
        }
        registers[ATA_ERROR] = error;
        *hwregister = error ? ATA_ERR : result;
        return;
    }
}

