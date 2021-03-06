/*
 * filetype_crt.cc
 *
 * Written by 
 *    Gideon Zweijtzer <info@1541ultimate.net>
 *
 *  This file is part of the 1541 Ultimate-II application.
 *  Copyright (C) 2008-2011 Gideon Zweijtzer <info@1541ultimate.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "filetype_crt.h"
#include "directory.h"
#include "filemanager.h"
#include "c64.h"
#include "userinterface.h"
#include <ctype.h>

// tester instance
FactoryRegistrator<BrowsableDirEntry *, FileType *> tester_crt(FileType :: getFileTypeFactory(), FileTypeCRT :: test_type);

extern C64 *c64;

/*********************************************************************/
/* PRG File Browser Handling                                         */
/*********************************************************************/
#define CRTFILE_RUN            0x2C01
#define CRTFILE_CONFIGURE      0x2C02

struct t_cart {
    uint16_t hw_type;
    uint16_t type_select;
    char *cart_name;
};

#define CART_NOT_IMPL  0xFFFF
#define CART_NORMAL    1
#define CART_ACTION    2
#define CART_RETRO     3
#define CART_DOMARK    4
#define CART_OCEAN     5
#define CART_EASYFLASH 6
#define CART_SUPERSNAP 7
#define CART_EPYX      8
#define CART_FINAL3    9
#define CART_SYSTEM3   10
#define CART_KCS       11
#define CART_FINAL12   12

const struct t_cart c_recognized_carts[] = {
    {  0, CART_NORMAL,    "Normal cartridge" },
    {  1, CART_ACTION,    "Action Replay" },
    {  2, CART_KCS,       "KCS Power Cartridge" },
    {  3, CART_FINAL3,    "Final Cartridge III" },
    {  4, CART_NOT_IMPL,  "Simons Basic" },
    {  5, CART_OCEAN,     "Ocean type 1 (256 and 128 Kb)" },
    {  6, CART_NOT_IMPL,  "Expert Cartridge" },
    {  7, CART_NOT_IMPL,  "Fun Play" },
    {  8, CART_NOT_IMPL,  "Super Games" },
    {  9, CART_RETRO,     "Atomic Power" },
    { 10, CART_EPYX,      "Epyx Fastload" },
    { 11, CART_NOT_IMPL,  "Westermann" },
    { 12, CART_NOT_IMPL,  "Rex" },
    { 13, CART_FINAL12,   "Final Cartridge I" },
    { 14, CART_NOT_IMPL,  "Magic Formel" },
    { 15, CART_SYSTEM3,   "C64 Game System" },
    { 16, CART_NOT_IMPL,  "Warpspeed" },
    { 17, CART_NOT_IMPL,  "Dinamic" },
    { 18, CART_NOT_IMPL,  "Zaxxon" },
    { 19, CART_DOMARK,    "Magic Desk, Domark, HES Australia" },
    { 20, CART_SUPERSNAP, "Super Snapshot 5" },
    { 21, CART_NOT_IMPL,  "COMAL 80" },
    { 32, CART_EASYFLASH, "EasyFlash" },
    { 0xFFFF, 0xFFFF, "" } }; 
    

FileTypeCRT :: FileTypeCRT(BrowsableDirEntry *n)
{
	node = n;
	printf("Creating CRT type from info: %s\n", n->getName());
}

FileTypeCRT :: ~FileTypeCRT()
{
}

int FileTypeCRT :: fetch_context_items(IndexedList<Action *> &list)
{
    if(c64) {
    	list.append(new Action("Run Cart", FileTypeCRT :: execute_st, CRTFILE_RUN, (int)this));
    }
    return 1;
}

// static member
FileType *FileTypeCRT :: test_type(BrowsableDirEntry *obj)
{
	FileInfo *inf = obj->getInfo();
    if(strcmp(inf->extension, "CRT")==0)
        return new FileTypeCRT(obj);
    return NULL;
}

__inline static uint16_t get_word(uint8_t *p)
{
    return (uint16_t(p[0]) << 8) + p[1];
}

__inline static uint32_t get_dword(uint8_t *p)
{
    return uint32_t(get_word(p) << 16) + get_word(p+2);
}

// static member
int FileTypeCRT :: execute_st(SubsysCommand *cmd)
{
	return ((FileTypeCRT *)cmd->mode)->execute(cmd);
}

// non-static member
int FileTypeCRT :: execute(SubsysCommand *cmd)
{
	File *file = 0;
	FileInfo *inf;
	
    FileManager *fm = FileManager :: getFileManager();

    uint32_t mem_addr = ((uint32_t)C64_CARTRIDGE_RAM_BASE) << 16;
    memset((void *)mem_addr, 0, 1024*1024); // clear all cart memory

    printf("Cartridge Load.. %s\n", cmd->filename.c_str());
    FRESULT fres = fm->fopen(cmd->path.c_str(), cmd->filename.c_str(), FA_READ, &file);
    uint32_t dw;
    total_read = 0;
    max_bank = 0;
    load_at_a000 = false;
    static char str[64];

    if(file) {
        name = NULL;
        if(check_header(file)) { // pointer is now at 0x20
            dw = get_dword(crt_header + 16);
            printf("Header OK. Now reading chip packets starting from %6x.\n", dw);
            fres = file->seek(dw);
            if(fres == FR_OK)
				while(read_chip_packet(file))
					;

        } else {
            if(name) {
                strcpy(str, name);
                strcat(str, ". Not Implemented.");
                cmd->user_interface->popup(str, BUTTON_OK);
            } else {
                cmd->user_interface->popup("Header of CRT file not correct.", BUTTON_OK);
            }
            return -1;
        }

        c64->unfreeze(0, 0);
        configure_cart();

        fm->fclose(file);
    } else {
        printf("Error opening file.\n");
        cmd->user_interface->popup(FileSystem :: get_error_string(fres), BUTTON_OK);
        return -2;
    }
    return 0;
}

bool FileTypeCRT :: check_header(File *f)
{
    uint32_t bytes_read;
    FRESULT res = f->read(crt_header, 0x20, &bytes_read);
    if(res != FR_OK)
        return false;
    if(strncmp((char*)crt_header, "C64 CARTRIDGE   ", 16))
        return false;
    if(crt_header[0x16])
        return false;

    printf("CRT Hardware type: %b ", crt_header[0x17]);
    int idx = 0;
    while(c_recognized_carts[idx].hw_type != 0xFFFF) {
        if(uint8_t(c_recognized_carts[idx].hw_type) == crt_header[0x17]) {
            name = c_recognized_carts[idx].cart_name;
            printf("%s\n", name);
            type_select = c_recognized_carts[idx].type_select;
            if(type_select == CART_NOT_IMPL) {
                printf("Not implemented.\n");
                return false;
            }
            if(type_select == CART_KCS) {
            	/* Bug in many KCS CRT images, wrong header size */
            	crt_header[0x10]=crt_header[0x11]=crt_header[0x12]=0;
            	crt_header[0x13]=0x40;
            }
            return true;
        }
        idx++;
    }
    printf("Unknown\n");
    return false;
}    

bool FileTypeCRT :: read_chip_packet(File *f)
{
    uint32_t bytes_read;
    FRESULT res = f->read(chip_header, 0x10, &bytes_read);
    if((res != FR_OK)||(!bytes_read))
        return false;
    if(strncmp((char*)chip_header, "CHIP", 4))
        return false;
    uint32_t total_length = get_dword(chip_header + 4);
    uint16_t bank = get_word(chip_header + 10);
    uint16_t load = get_word(chip_header + 12);
    uint16_t size = get_word(chip_header + 14);

    if(bank > 0x7f)
        return false;

    if((type_select == CART_OCEAN)&&(load == 0xA000))
        bank &= 0xEF; // max 128k per region

    if(bank > max_bank)
        max_bank = bank;

    bool split = false;
    if(size > 0x2000)
        if((type_select == CART_OCEAN)||(type_select == CART_EASYFLASH)||(type_select == CART_DOMARK)||(type_select == CART_SYSTEM3))
            split = true;

    uint32_t mem_addr = ((uint32_t)C64_CARTRIDGE_RAM_BASE) << 16;

    if(type_select == CART_KCS) {
    	mem_addr += load - 0x8000;
    } else if(type_select == CART_NORMAL) {
    	mem_addr += (load & 0x2000); // use bit 13 of the load address
    } else if(split) {
        mem_addr += 0x2000 * uint32_t(bank);
    } else {
        mem_addr += uint32_t(size) * uint32_t(bank);
    }

    if(load == 0xA000) {
        mem_addr += 512 * 1024; // interleaved mode (TODO: make it the same in hardware as well, currently only for EasyFlash)
        load_at_a000 = true;
    }
    printf("Reading chip data bank %d to $%4x with size $%4x to 0x%8x. %s\n", bank, load, size, mem_addr, (split)?"Split":"Contiguous");
    
    if(size) {
        if(split) {
            res = f->read((void *)mem_addr, 0x2000, &bytes_read);
            total_read += uint32_t(bytes_read);
            if(bytes_read != 0x2000) {
                printf("Just read %4x bytes, expecting 8K\n", bytes_read);
                return false;
            }
            mem_addr += 512 * 1024; // interleaved
            res = f->read((void *)mem_addr, size-0x2000, &bytes_read);
            total_read += uint32_t(bytes_read);
        } else {            
            res = f->read((void *)mem_addr, size, &bytes_read);
            total_read += uint32_t(bytes_read);
            if(bytes_read != size) {
                printf("Just read %4x bytes\n", bytes_read);
                return false;
            }
        }
        return (res == FR_OK);
    }
    return true;
}

void FileTypeCRT :: configure_cart(void)
{
    C64_MODE = C64_MODE_RESET;
    C64_REU_ENABLE = 0;
    C64_ETHERNET_ENABLE = 0;

    C64_CARTRIDGE_TYPE = CART_TYPE_NONE;

    uint32_t len = total_read;
    if(!len)
        type_select = CART_NOT_IMPL;

    int bits = 21;
    while(len < 0x100001) {
        len <<= 1;
        bits--;
    }

    uint16_t b = max_bank;
    uint16_t mask = 0;
    while(b) {
        mask <<= 1;
        mask |= 1;
        b >>= 1;
    }

    printf("Total ROM size read: %6x bytes; %d bits (%6x). Max_Bank = %d. Mask = %d\n", total_read, bits, (1<<bits), max_bank, mask);

    switch(type_select) {
        case CART_NORMAL:
            if ((crt_header[0x18] == 1) && (crt_header[0x19] == 0)) {  // only special case that we check
                C64_CARTRIDGE_TYPE = CART_TYPE_16K_UMAX;
            } /*else if ((crt_header[0x18] == 0) && (crt_header[0x19] == 0)) {
                C64_CARTRIDGE_TYPE = CART_TYPE_16K;
            } else if ((crt_header[0x18] == 0) && (crt_header[0x19] == 1)) {
                C64_CARTRIDGE_TYPE = CART_TYPE_8K;
            } */else {
                if (total_read > 8192)
                    C64_CARTRIDGE_TYPE = CART_TYPE_16K; // 16K
                else
                    C64_CARTRIDGE_TYPE = CART_TYPE_8K; // 8K
            }
            break;
        case CART_ACTION:
            C64_CARTRIDGE_TYPE = CART_TYPE_ACTION;
            break;
        case CART_RETRO:
            C64_CARTRIDGE_TYPE = CART_TYPE_RETRO;
            break;
        case CART_DOMARK:
            C64_CARTRIDGE_TYPE = CART_TYPE_DOMARK;
            break;
        case CART_OCEAN:
//            if ((total_read > 0x20000)&&(total_read <= 0x40000)) { // 16K banks
//                C64_CARTRIDGE_TYPE = 0x0B; // Ocean 256K
//                if(!load_at_a000) { // error! There is no data for upper range
//                    printf("Fixing Ocean 256, by copying second 128K to A000 range\n");
//                    uint32_t mem_base = ((uint32_t)C64_CARTRIDGE_RAM_BASE) << 16;
//                    memcpy((void *)(mem_base + 512*1024), (void *)(mem_base + 128*1024), 128*1024);
//                }
//            } else {
//                C64_CARTRIDGE_TYPE = 0x09; // Ocean 128K/512K
//            }                
//
            if (load_at_a000) {
                C64_CARTRIDGE_TYPE = CART_TYPE_OCEAN256; // Ocean 256K
            } else {
//                uint32_t mem_base = ((uint32_t)C64_CARTRIDGE_RAM_BASE) << 16;
//                memcpy((void *)(mem_base + 256*1024), (void *)(mem_base + 0*1024), 256*1024);
                C64_CARTRIDGE_TYPE = CART_TYPE_OCEAN128; // Ocean 128K/512K
            }                
            break;
        case CART_EASYFLASH:
            C64_CARTRIDGE_TYPE = CART_TYPE_EASY_FLASH; // EasyFlash
            break;
        case CART_SUPERSNAP:
            C64_CARTRIDGE_TYPE = CART_TYPE_SS5; // Snappy
            break;
        case CART_EPYX:
            C64_CARTRIDGE_TYPE = CART_TYPE_EPYX; // Epyx
            break;
        case CART_FINAL3:
            C64_CARTRIDGE_TYPE = CART_TYPE_FC3; // Final3
            break;
        case CART_SYSTEM3:
            C64_CARTRIDGE_TYPE = CART_TYPE_SYSTEM3; // System3
            break;
        case CART_KCS:
            C64_CARTRIDGE_TYPE = CART_TYPE_KCS; // System3
            break;
        case CART_FINAL12:
            C64_CARTRIDGE_TYPE = CART_TYPE_FINAL12; // System3
            break;


        default:
            break;
    }
    printf("*%b*\n", C64_CARTRIDGE_TYPE);

    C64_MODE = C64_MODE_UNRESET;
}
