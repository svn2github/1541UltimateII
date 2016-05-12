/*
##########################################################################
##                                                                      ##
##   RENE GARCIA                        GNU General Public Licence V3   ##
##                                                                      ##
##########################################################################
##                                                                      ##
##      Project : MPS Emulator                                          ##
##      Class   : MpsPrinter                                            ##
##      Piece   : mps_printer.cc                                        ##
##      Language: C ANSI                                                ##
##      Author  : Rene GARCIA                                           ##
##                                                                      ##
##########################################################################
*/

        /*-
         *
         *  $Id:$
         *
         *  $URL:$
         *
        -*/

/******************************  Inclusions  ****************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "lodepng.h"
#include "mps_printer.h"

/************************************************************************
*                                                                       *
*               G L O B A L   M O D U L E   V A R I A B L E S           *
*                                                                       *
************************************************************************/

/* =======  Pitch for letters */
uint8_t MpsPrinter::spacing_x[7][13] =
{
    {  0, 2, 4, 6, 8,10,12,14,16,18,20,22,24 },    // Pica              24px/char
    {  0, 2, 3, 5, 7, 8,10,12,13,15,17,19,20 },    // Elite             20px/char
    {  0, 1, 3, 4, 5, 7, 8, 9,11,12,13,15,16 },    // Micro             16px/char
    {  0, 1, 2, 3, 5, 6, 7, 8, 9,10,12,13,14 },    // Compressed        14px/char
    {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12 },    // Pica Compressed   12px/char
    {  0, 1, 2, 2, 3, 4, 5, 6, 7, 7, 8, 9,10 },    // Elite Compressed  10px/char
    {  0, 1, 1, 2, 3, 3, 4, 5, 5, 6, 7, 7, 8 },    // Micro Compressed  8px/char
};

/* =======  Pitch for sub/super-script */
uint8_t MpsPrinter::spacing_y[6][17] =
{
    {  0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,45,48 },    // Normal Draft & NLQ High
    {  2, 5, 8,11,14,17,20,23,26,28,32,35,38,41,44,47,50 },    // Normal NLQ Low
    {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16 },    // Superscript Draft & NLQ High
    {  1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17 },    // Superscript NLQ Low
    { 19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35 },    // Subscript Draft & NLQ High
    { 20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36 },    // Subscript NLQ Low
};

/* =======  Printable control chars in quotted mode */
uint8_t MpsPrinter::cbm_special[MPS_PRINTER_MAX_SPECIAL] =
{
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x81,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
    0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f
};

/************************************************************************
*               MpsPrinter::MpsPrinter(filename)          Constructor   *
*               ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                        *
* Function : MpsPrinter object default constructor with no argument     *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    filename : (char *) filename base, an indice and extension will be *
*               added, default is NULL if not provided                  *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

MpsPrinter::MpsPrinter(char * filename)
{
#ifndef NOT_ULTIMATE
    fm = FileManager :: getFileManager();
    if (filename == NULL) filename = (char *) "/SD/mps";
#else
    if (filename == NULL) filename = (char *) "mps";
#endif
    strcpy(outfile,filename);

    /* Initialise PNG convertor attributes */
    lodepng_state_init(&lodepng_state);

    /* Initialise color palette for memory bitmap and file output */
    lodepng_palette_clear(&lodepng_state.info_png.color);
    lodepng_palette_clear(&lodepng_state.info_raw);

    /* White */
    lodepng_palette_add(&lodepng_state.info_png.color, 255, 255, 255, 255);
    lodepng_palette_add(&lodepng_state.info_raw, 255, 255, 255, 255);

    /* Light grey */
    lodepng_palette_add(&lodepng_state.info_png.color, 224, 224, 224, 255);
    lodepng_palette_add(&lodepng_state.info_raw, 224, 224, 224, 255);

    /* Dark grey */
    lodepng_palette_add(&lodepng_state.info_png.color, 160, 160, 160, 255);
    lodepng_palette_add(&lodepng_state.info_raw, 160, 160, 160, 255);

    /* Black */
    lodepng_palette_add(&lodepng_state.info_png.color, 0, 0, 0, 255);
    lodepng_palette_add(&lodepng_state.info_raw, 0, 0, 0, 255);

    /* Bitmap uses 2 bit depth and a palette */
    lodepng_state.info_png.color.colortype  = LCT_PALETTE;
    lodepng_state.info_png.color.bitdepth   = 2;
    lodepng_state.info_raw.colortype        = LCT_PALETTE;
    lodepng_state.info_raw.bitdepth         = 2;

    /* Physical page description (A4 240x216 dpi) */
    lodepng_state.info_png.phys_defined     = 1;
    lodepng_state.info_png.phys_x           = 9448;
    lodepng_state.info_png.phys_y           = 8687;
    lodepng_state.info_png.phys_unit        = 1;

    /* I rule, you don't */
    lodepng_state.encoder.auto_convert      = 0;

        /*-
         *
         *  Page num start from 1 but should be
         *  better to find the max existing number
         *  in the filesystem because files are not
         *  overwritten if they exist (this is a choice)
         *
        -*/

    page_num = 1;

    /* =======  Reset printer attributes */
    Reset();
}

/************************************************************************
*                       MpsPrinter::~MpsPrinter()         Destructor    *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~                       *
* Function : Free all resources used by an MpsPrinter instance          *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    none                                                               *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

MpsPrinter::~MpsPrinter()
{
    lodepng_state_cleanup(&lodepng_state);
}

/************************************************************************
*                       MpsPrinter::getMpsPrinter()       Singleton     *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~                     *
* Function : Get pointer to the MpsPrinter singleton instance           *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    none                                                               *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (MpsPrinter *) Pointer to singleton instance of MpsPrinter         *
*                                                                       *
************************************************************************/

MpsPrinter *MpsPrinter::getMpsPrinter()
{
    // The singleton is here, static
    static MpsPrinter m_mpsInstance;
    return &m_mpsInstance;
}

/************************************************************************
*               MpsPrinter::setFilename(filename)               Public  *
*               ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                       *
* Function : Change base filename used for output PNG files. Page       *
*            counter is reset to 1                                      *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    filename : (char *) filename base, an indice and extension will be *
*               added                                                   *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::setFilename(char * filename)
{
    /* Store new filename */
    if (filename)
        strcpy(outfile,filename);

    /* Reset page counter */
    page_num = 1;
}

/************************************************************************
*                       MpsPrinter::setCBMCharset(cs)           Public  *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                   *
* Function : Change Commodore 64 charset                                *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    cs : (uint8_t) New Commodore 64 Charset                            *
*               0 - Uppercases/Graphics                                 *
*               1 - Lowercases/Uppercases                               *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::setCBMCharset(uint8_t cs)
{
    if (cs !=0 && cs != 1)
        return;

    cbm_charset = cs;
}

/************************************************************************
*                       MpsPrinter::setDotSize(ds)              Public  *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~                      *
* Function : Change ink dot size                                        *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    ds : (uint8_t) New dot size                                        *
*               0 - 1 pixe diameter like                                *
*               1 - 2 pixels diameter like                              *
*               2 - 3 pixels diameter like                              *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::setDotSize(uint8_t ds)
{
    if (ds >2) ds = 2;
    dot_size = ds;
}

/************************************************************************
*                       MpsPrinter::Clear()               Private       *
*                       ~~~~~~~~~~~~~~~~~~~                             *
* Function : Clear page and set printer head on top left position       *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    none                                                               *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::Clear(void)
{
    bzero (bitmap,MPS_PRINTER_BITMAP_SIZE);
    head_x         = 0;
    head_y         = 0;
    next_interline = interline;
    clean          = true;
    quoted         = false;
}

/************************************************************************
*                       MpsPrinter::Reset()                 Public      *
*                       ~~~~~~~~~~~~~~~~~~~                             *
* Function : Set printer to initial state                               *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    none                                                               *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::Reset(void)
{
    /* =======  Default H tabulations */
    for (int i=0; i<32; i++)
    {
        htab[i] = 168+i*24*8;
    }

    /* =======  Default printer attributes */
    dot_size      = 1;
    step          = 0;
    script        = MPS_PRINTER_SCRIPT_NORMAL;
    interline     = 36;
    cbm_charset   = 0;
    underline     = false;
    double_width  = false;
    bold          = false;
    nlq           = false;
    double_strike = false;

    /* -------  Clear the bitmap (all white) */
    Clear();
}

/************************************************************************
*                       MpsPrinter::FormFeed()            Public        *
*                       ~~~~~~~~~~~~~~~~~~~~~~                          *
* Function : Save current page to PNG file and clear page               *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    none                                                               *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::FormFeed(void)
{
    char filename[40];

    if (!clean)
    {
        sprintf(filename,"%s-%03d.png", outfile, page_num);
        page_num++;
#ifdef NOT_ULTIMATE
        printf("printing to file %s\n", filename);
#endif
        Print(filename);
    }

    Clear();
}

/************************************************************************
*                       MpsPrinter::Print(filename)       Private       *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~                     *
* Function : Save current page to PNG finename provided                 *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    filename : (char *) filename to save PNG file to                   *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::Print(const char * filename)
{
    uint8_t *buffer;
    size_t outsize;

    buffer=NULL;
    unsigned error = lodepng_encode(&buffer, &outsize, bitmap, MPS_PRINTER_PAGE_WIDTH, MPS_PRINTER_PAGE_HEIGHT, &lodepng_state);
#ifndef NOT_ULTIMATE
    File *f;
    FRESULT fres = fm->fopen((const char *) filename, FA_WRITE|FA_CREATE_NEW, &f);
    if (f) {
        uint32_t bytes;
        f->write(buffer, outsize, &bytes);
        fm->fclose(f);
    }
#else
    int fhd = open(filename, O_WRONLY|O_CREAT, 0666);
    if (fhd) {
        write(fhd, buffer, outsize);
        close(fhd);
    }
    else
    {
        printf("Saving file failed\n");
    }
#endif

    free(buffer);
}

/************************************************************************
*                               MpsPrinter::Dot(x,y)          Private   *
*                               ~~~~~~~~~~~~~~~~~~~~                    *
* Function : Prints a single dot on page, dot size depends on dentity   *
*            setting. If position is out of printable area, no dot is   *
*            printed                                                    *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    x : (uint16_t) pixel position from left of printable area of page  *
*    y : (uint16_t) pixel position from top of printable area of page   *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::Dot(uint16_t x, uint16_t y)
{
    /* =======  Check if position is out of range */

    if ( x > MPS_PRINTER_PAGE_PRINTABLE_WIDTH  ||
         y > MPS_PRINTER_PAGE_PRINTABLE_HEIGHT ) return;

    /* =======  Draw dot */

    switch (dot_size)
    {
        case 0:     // Density 0 : 1 single black point (diameter 1 pixel) mostly for debug
            Ink(x,y);
            break;

        case 1:     // Density 1 : 1 black point with gray around (looks like diameter 2)
            Ink(x,y);
            Ink(x-1,y-1,1);
            Ink(x+1,y+1,1);
            Ink(x-1,y+1,1);
            Ink(x+1,y-1,1);
            Ink(x,y-1,2);
            Ink(x,y+1,2);
            Ink(x-1,y,2);
            Ink(x+1,y,2);
            break;

        case 2:     // Density 2 : 4 black points with gray around (looks like diameter 3)
        default:
            Ink(x,y);
            Ink(x,y+1);
            Ink(x+1,y);
            Ink(x+1,y+1);

            Ink(x-1,y-1,1);
            Ink(x+2,y-1,1);
            Ink(x-1,y+2,1);
            Ink(x+2,y+2,1);

            Ink(x,y-1,2);
            Ink(x+1,y-1,2);
            Ink(x,y+1,2);
            Ink(x-1,y,2);
            Ink(x-1,y+1,2);
            Ink(x+2,y,2);
            Ink(x+2,y+1,2);
            Ink(x,y+2,2);
            Ink(x+1,y+2,2);
            break;
    }

    /* -------  If double strike is ON, draw a second dot just to the right of the first one */
    if (double_strike)
    {
        /* prevent infinite recursion loop */
        double_strike = false;
        Dot(x,y+1);
        double_strike = true;
    }

    /* -------  Now we know that the page is not blank */
    clean  = false;
}

/************************************************************************
*                             MpsPrinter::Ink(x,y,c)          Private   *
*                             ~~~~~~~~~~~~~~~~~~~~~~                    *
* Function : Add ink on a single pixel position. If ink as already been *
*            added on this position it will add more ink to be darker   *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    x : (uint16_t) pixel position from left of printable area of page  *
*    y : (uint16_t) pixel position from top of printable area of page   *
*    c : (uint8_t) grey level                                           *
*        0=black (default), 1=dark grey, 2=light grey                   *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::Ink(uint16_t x, uint16_t y, uint8_t c)
{
    /* =======  Calculate true x and y position on page */
    uint16_t tx=x+MPS_PRINTER_PAGE_MARGIN_LEFT;
    uint16_t ty=y+MPS_PRINTER_PAGE_MARGIN_TOP;
    uint8_t current;

    /* -------  Which byte address is it on raster buffer */
    uint32_t byte = ((ty*MPS_PRINTER_PAGE_WIDTH+tx)*MPS_PRINTER_PAGE_DEPTH)>>3;

    /* -------  Whitch bits on byte are coding the pixe (4 pixels per byte) */
    uint8_t sub = tx & 0x3;
    c &= 0x3;

    /* =======  Add ink to the pixel */
    switch (sub)
    {
        case 0:
            current = (bitmap[byte] >> 6) & 0x03;
            c = Combine(c, current);
            bitmap[byte] &= 0x3F;
            bitmap[byte] |= c << 6;
            break;

        case 1:
            current = (bitmap[byte] >> 4) & 0x03;
            c = Combine(c, current);
            bitmap[byte] &= 0xCF;
            bitmap[byte] |= c << 4;
            break;

        case 2:
            current = (bitmap[byte] >> 2) & 0x03;
            c = Combine(c, current);
            bitmap[byte] &= 0xF3;
            bitmap[byte] |= c << 2;
            break;

        case 3:
            current = bitmap[byte] & 0x03;
            c = Combine(c, current);
            bitmap[byte] &= 0xFC;
            bitmap[byte] |= c;
            break;
    }
}

/************************************************************************
*                       MpsPrinter::Combine(c1,c2)            Private   *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~                      *
* Function : Combine two grey levels and give the resulting grey        *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c1 : (int) first grey level (0 is white, 3 is black)               *
*    c2 : (int) second grey level (0 is white, 3 is black)              *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (uint8_t) resulting grey level (in range range 0-3)                *
*                                                                       *
************************************************************************/

uint8_t
MpsPrinter::Combine(uint8_t c1, uint8_t c2)
{
        /*-
         *
         *   Very easy to understand :
         *
         *   white      + white         = white
         *   white      + light grey    = light grey
         *   white      + dark grey     = dark grey
         *   light grey + light grey    = dark grey
         *   light grey + dark grey     = black
         *   dark grey  + dark grey     = black
         *   black      + *whatever*    = black
         *
        -*/

    uint8_t result = c1 + c2;
    if (result > 3) result = 3;
    return result;
}

/************************************************************************
*                       MpsPrinter::CharItalic(c,x,y)         Private   *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                   *
* Function : Print a single italic draft quality character              *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c : (uint16_t) char from italic chargen table                      *
*    x : (uint16_t) first pixel position from left of printable area    *
*    y : (uint16_t) first pixel position from top of printable area     *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (uint16_t) printed char width (in pixels)                          *
*                                                                       *
************************************************************************/

uint16_t
MpsPrinter::CharItalic(uint16_t c, uint16_t x, uint16_t y)
{
    /* =======  Check is chargen is in italic chargen range */
    if (c > 128) return 0;

    uint8_t lst_head = 0;         // Last printed pattern to calculate reverse
    uint8_t shift = chargen_italic[c][11] & 1;    // 8 down pins from 9 ?

    /* =======  For each value of the pattern */
    for (int i=0; i<12; i++)
    {
        /* -------  Last is always 0 */
        uint8_t cur_head = (i==11)?0:chargen_italic[c][i];

        /* -------  Reverse is negative printing */
        if (reverse)
        {
            cur_head = cur_head | lst_head;
            cur_head ^= 0xFF;
            lst_head = chargen_italic[c][i];
        }

        /* -------  On double width, each pattern is printed twice */
        for (int d=0; d<(double_width?2:1); d++)
        {
            /* This is what we'll print */
            uint8_t head = cur_head;

            /* Calculate x position according to width and stepping */
            int dx = x + ((spacing_x[step][i]+d*spacing_x[step][1])<<(double_width?1:0));

            /* -------  Each dot to print (LSB is up) */
            for (int j=0; j<8; j++)
            {
                /* pin 9 is used for underline, can't be used on shifted chars */
                if (underline && shift && j==7) continue;

                /* Need to print a dot ?*/
                if (head & 0x01)
                {
                    /* vertical position according to stepping for normal, subscript or superscript */
                    int dy = y+spacing_y[script][j+shift];

                    /* The dot itself */
                    Dot(dx, dy);

                    /* Double dot to the right if printing bold */
                    if (bold) Dot(dx+spacing_x[step][1], dy);
                }

                head >>= 1;
            }

            /* -------  Need to underline ? */

            /* Underline is one dot every 2 pixels */
            if (!(i&1) && underline)
            {
                int dy = y+spacing_y[script][8];

                Dot(dx, dy);
                if (bold) Dot(dx+(spacing_x[step][1]<<(double_width?1:0)), dy);
            }
        }
    }

    /* =======  This is the width of the printed char */
    return spacing_x[step][12]<<(double_width?1:0);
}

/************************************************************************
*                       MpsPrinter::CharDraft(c,x,y)          Private   *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~                    *
* Function : Print a single regular draft quality character             *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c : (uint16_t) char from draft chargen table                       *
*    x : (uint16_t) first pixel position from left of printable area    *
*    y : (uint16_t) first pixel position from top of printable area     *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (uint16_t) printed char width (in pixels)                          *
*                                                                       *
************************************************************************/

uint16_t
MpsPrinter::CharDraft(uint16_t c, uint16_t x, uint16_t y)
{
    /* =======  Check is chargen is in draft chargen range */
    if (c > 403) return 0;

    uint8_t lst_head = 0; // Last printed pattern to calculate reverse
    uint8_t shift = chargen_draft[c][11] & 1;     // 8 down pins from 9 ?

    /* =======  For each value of the pattern */
    for (int i=0; i<12; i++)
    {
        /* -------  Last is always 0 */
        uint8_t cur_head = (i==11)?0:chargen_draft[c][i];

        /* -------  Reverse is negative printing */
        if (reverse)
        {
            cur_head = cur_head | lst_head;
            cur_head ^= 0xFF;
            lst_head = chargen_draft[c][i];
        }

        /* -------  On double width, each pattern is printed twice */
        for (int d=0; d<(double_width?2:1); d++)
        {
            /* This is what we'll print */
            uint8_t head = cur_head;

            /* Calculate x position according to width and stepping */
            int dx = x + ((spacing_x[step][i]+d*spacing_x[step][1])<<(double_width?1:0));

            /* -------  Each dot to print (LSB is up) */
            for (int j=0; j<8; j++)
            {
                /* pin 9 is used for underline, can't be used on shifted chars */
                if (underline && shift && j==7) continue;

                /* Need to print a dot ?*/
                if (head & 0x01)
                {
                    /* vertical position according to stepping for normal, subscript or superscript */
                    int dy = y+spacing_y[script][j+shift];

                    /* The dot itself */
                    Dot(dx, dy);

                    /* Double dot to the right if printing bold */
                    if (bold) Dot(dx+(spacing_x[step][1]<<(double_width?1:0)), dy);
                }

                head >>= 1;
            }

            /* -------  Need to underline ? */

            /* Underline is one dot every 2 pixels */
            if (!(i&1) && underline)
            {
                int dy = y+spacing_y[script][8];

                Dot(dx, dy);
                if (bold) Dot(dx+(spacing_x[step][1]<<(double_width?1:0)), dy);
            }
        }
    }

    /* =======  If the char is completed by a second chargen below, go print it */
    if (chargen_draft[c][11] & 0x80)
    {
        CharDraft((chargen_draft[c][11] & 0x70) >> 4, x, y+spacing_y[script][shift+8]);
    }

    /* =======  This is the width of the printed char */
    return spacing_x[step][12]<<(double_width?1:0);
}

/************************************************************************
*                       MpsPrinter::CharNLQ(c,x,y)            Private   *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~                      *
* Function : Print a single regular NLQ character                       *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c : (uint16_t) char from draft nlq (high and low) table            *
*    x : (uint16_t) first pixel position from left of printable area    *
*    y : (uint16_t) first pixel position from top of printable area     *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (uint16_t) printed char width (in pixels)                          *
*                                                                       *
************************************************************************/

uint16_t
MpsPrinter::CharNLQ(uint16_t c, uint16_t x, uint16_t y)
{
    /* =======  Check is chargen is in NLQ chargen range */
    if (c > 403) return 0;

    uint8_t lst_head_low = 0;     // Last low printed pattern to calculate reverse
    uint8_t lst_head_high = 0;    // Last high printed pattern to calculate reverse
    uint8_t shift = chargen_nlq_high[c][11] & 1;  // 8 down pins from 9 ?

    /* =======  For each value of the pattern */
    for (int i=0; i<12; i++)
    {
        uint8_t cur_head_high;
        uint8_t cur_head_low;

        /* -------  Calculate last column */
        if (i==11)
        {
            if (chargen_nlq_high[c][11] & 0x04)
            {
                /* Repeat last colums */
                cur_head_high = chargen_nlq_high[c][10];
            }
            else
            {
                /* Blank column */
                cur_head_high = 0;
            }

            if (chargen_nlq_low[c][11] & 0x04)
            {
                /* Repeat last colums */
                cur_head_low = chargen_nlq_low[c][10];
            }
            else
            {
                /* Blank column */
                cur_head_low = 0;
            }
        }
        else
        {
            /* Not on last column, get data from chargen table */
            cur_head_high = chargen_nlq_high[c][i];
            cur_head_low = chargen_nlq_low[c][i];
        }

        /* -------  Reverse is negative printing */
        if (reverse)
        {
            cur_head_high = cur_head_high | lst_head_high;
            cur_head_high ^= 0xFF;
            lst_head_high = chargen_nlq_high[c][i];

            cur_head_low = cur_head_low | lst_head_low;
            cur_head_low ^= 0xFF;
            lst_head_low = chargen_nlq_low[c][i];
        }

        /* -------  First we start with the high pattern */
        uint8_t head = cur_head_high;

        /* Calculate x position according to width and stepping */
        int dx = x + (spacing_x[step][i]<<(double_width?1:0));

        /* -------  Each dot to print (LSB is up) */
        for (int j=0; j<8; j++)
        {
            /* Need to print a dot ?*/
            if (head & 0x01)
            {
                /* vertical position according to stepping for normal, subscript or superscript */
                int dy = y+spacing_y[script][j+shift];

                /* The dot itself */
                Dot(dx, dy);

                /* On double width, another dot to the right */
                if (double_width) Dot(dx+spacing_x[step][1]*2, dy);

                /* On bold, another dot to rhe right */
                if (bold)
                {
                    Dot(dx+spacing_x[step][1], dy);

                    /* On bold and double width, another dot (so 4 dots total) */
                    if (double_width) Dot(dx+spacing_x[step][1]*3, dy);
                }
            }

            head >>= 1;
        }

        /* -------  Now we print the low pattern */
        head = cur_head_low;

        /* -------  Each dot to print (LSB is up) */
        for (int j=0; j<8; j++)
        {
            /* pin 9 on low pattern is used for underline, can't be used on shifted chars */
            if (underline && shift && j==7) continue;

            /* Need to print a dot ?*/
            if (head & 0x01)
            {
                /* vertical position according to stepping for normal, subscript or superscript */
                int dy = y+spacing_y[script+1][j+shift];

                /* The dot itself */
                Dot(dx, dy);

                /* On double width, another dot to the right */
                if (double_width) Dot(dx+spacing_x[step][1]*2, dy);

                /* On bold, another dot to rhe right */
                if (bold)
                {
                    Dot(dx+spacing_x[step][1], dy);

                    /* On bold and double width, another dot (so 4 dots total) */
                    if (double_width) Dot(dx+spacing_x[step][1]*3, dy);
                }
            }

            head >>= 1;
        }

        /* -------  Need to underline ? */
        /* Underline is one dot every pixel on NLQ quality */
        if (underline)
        {
            int dy = y+spacing_y[script+1][8];

            Dot(dx, dy);
            if (bold) Dot(dx+spacing_x[step][1], dy);
        }
    }

    /* =======  If the char is completed by a second chargen below, go print it */
    if (chargen_nlq_high[c][11] & 0x80)
    {
        CharNLQ((chargen_nlq_high[c][11] & 0x70) >> 4, x, y+spacing_y[script][shift+8]);
    }

    /* =======  This is the width of the printed char */
    return spacing_x[step][12]<<(double_width?1:0);
}

/************************************************************************
*                       MpsPrinter::Bim(c,x,y)                  Private *
*                       ~~~~~~~~~~~~~~~~~~~~~~                          *
* Function : Print a single bitmap image record                         *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c : (uint16_t) record to print                                     *
*    x : (uint16_t) first pixel position from left of printable area    *
*    y : (uint16_t) first pixel position from top of printable area     *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (uint16_t) printed width (in pixels)                               *
*                                                                       *
************************************************************************/

uint16_t
MpsPrinter::Bim(uint8_t head)
{
    /* -------  Each dot to print (LSB is up) */
    for (int j=0; j<8; j++)
    {
        /* Need to print a dot ?*/
        if (head & 0x01)
        {
            Dot(head_x, head_y+spacing_y[MPS_PRINTER_SCRIPT_NORMAL][j]);
        }

        head >>= 1;
    }

    /* Return spacing */
    return spacing_x[step][2];
}

/************************************************************************
*               MpsPrinter::Interprepter(buffer, size)          Public  *
*               ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                  *
* Function : Interpret a set of data as sent by the computer            *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    buffer : (uint8_t *) Buffer to interpret                           *
*    size   : (uint32_t) bytes in buffer to interpret                   *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::Interpreter(const uint8_t * input, uint32_t size)
{
    /* =======  Call the other Interpreter method on each byte */
    while(size-- > 0)
    {
        Interpreter(*input);
        input++;
    }
}

/************************************************************************
*                       MpsPrinter::IsPrintable(c)              Private *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~                      *
* Function : Tell if an char code is printable or not                   *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c : (uint8_t) Byte code to test                                    *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (bool) true if printable                                           *
*                                                                       *
************************************************************************/

bool
MpsPrinter::IsPrintable(uint8_t input)
{
    /* In charset Table non printables are coded 500 */
    return (charset_cbm_us[cbm_charset][input] == 500) ? false : true;
}

/************************************************************************
*                       MpsPrinter::IsSpecial(c)                Private *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~                        *
* Function : Tell if an char code is special or not                     *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c : (uint8_t) Byte code to test                                    *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (bool) true if special                                             *
*                                                                       *
************************************************************************/

bool
MpsPrinter::IsSpecial(uint8_t input)
{
    for (int i=0; i< MPS_PRINTER_MAX_SPECIAL; i++)
        if (input == cbm_special[i])
            return true;

    return false;
}

/************************************************************************
*                       MpsPrinter::Charset2Chargen(c)          Private *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                  *
* Function : Give the chargen code of a charset character               *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c : (uint8_t) Byte code to convert                                 *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (uint16_t) chargen code.                                           *
*  will be 500 if non printable                                         *
*  will be >=1000 if italic (sub 1000 to get italic chargen code)       *
*                                                                       *
************************************************************************/

uint16_t
MpsPrinter::Charset2Chargen(uint8_t input)
{
    /* =======  Italic is enabled, get italic code if not 500 */
    if (italic)
    {
        uint16_t charnum = charset_italic_cbm_us[cbm_charset][input];
        if (charnum != 500) return 1000+charnum;
    }

    /* Otherwise of if italic code is 500 get normal chargen code (may be 500 too) */
    return charset_cbm_us[cbm_charset][input];
}

/************************************************************************
*                       MpsPrinter::Char(c)                     Private *
*                       ~~~~~~~~~~~~~~~~~~~                             *
* Function : Print a char on a page it will call Italic, Draft or NLQ   *
*            char print method depending on current configuration       *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    c : (uint16_t) Chargen code to print (+1000 if italic)             *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    (uint16_t) printed char width                                      *
*                                                                       *
************************************************************************/

uint16_t
MpsPrinter::Char(uint16_t c)
{
    /* 1000 is italic offset */
    if (c >= 1000)
    {
        /* call Italic print method */
        return CharItalic(c-1000, head_x, head_y);
    }
    else
    {
        if (nlq)
        {
            /* call NLQ print method */
            return CharNLQ(c, head_x, head_y);
        }
        else
        {
            /* call NLQ print method */
            return CharDraft(c, head_x, head_y);
        }
    }
}

/************************************************************************
*                       MpsPrinter::Interprepter(data)          Public  *
*                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                  *
* Function : Interpret a single data as sent by the computer            *
*-----------------------------------------------------------------------*
* Inputs:                                                               *
*                                                                       *
*    data : (uint8_t *) Single data to interpret                        *
*                                                                       *
*-----------------------------------------------------------------------*
* Outputs:                                                              *
*                                                                       *
*    none                                                               *
*                                                                       *
************************************************************************/

void
MpsPrinter::Interpreter(uint8_t input)
{
    switch(state)
    {
        case MPS_PRINTER_STATE_NORMAL:
            /* =======  Special Commodore special quotted chars */
            if (quoted)
            {
                if (!IsPrintable(input))
                {
                    if (IsSpecial(input))
                    {
                        bool saved_reverse = reverse;
                        reverse = true;

                        head_x += Char(Charset2Chargen(input | 0x40));
                        if (head_x > MPS_PRINTER_PAGE_PRINTABLE_WIDTH)
                        {
                            head_y += interline;
                            head_x  = 0;
                        }

                        reverse = saved_reverse;
                        return;
                    }
                    else
                    {
                        quoted = false;
                    }
                }
            }

            /* =======  Select action if command char received */
            cbm_command = input;
            param_count = 0;
            switch(input)
            {
                case 0x08:   // BIT IMG: bitmap image
                    next_interline = spacing_y[MPS_PRINTER_SCRIPT_NORMAL][7];
                    state = MPS_PRINTER_STATE_PARAM;
                    break;

                case 0x09:   // TAB: horizontal tabulation
                    {
                        int i = 0;
                        while (i < MPS_PRINTER_MAX_HTABULATIONS && htab[i] < MPS_PRINTER_PAGE_PRINTABLE_WIDTH)
                        {
                            if (htab[i] > head_x)
                            {
                                head_x = htab[i];
                                break;
                            }

                            i++;
                        }
                    }
                    break;

                case 0x0B:   // VT: vertical tabulation
                    // to be done
                    break;

                case 0x0C:   // FF: form feed
                    FormFeed();
                    break;

                case 0x0A:   // LF: line feed (LF+CR)
                case 0x0D:   // CR: carriage return (CR+LF)
                    head_y += next_interline;
                    head_x  = 0;
                    quoted = false;
                    if (head_y > MPS_PRINTER_PAGE_PRINTABLE_HEIGHT)
                        FormFeed();
                    break;

                case 0x0E:   // EN ON: Double width printing on
                    double_width = true;
                    break;

                case 0x0F:   // EN OFF: Double width printing off
                    double_width = false;
                    break;

                case 0x10:   // POS: Jump to horizontal position in number of chars
                    state = MPS_PRINTER_STATE_PARAM;
                    break;

                case 0x11:   // CRSR DWN: Upper/lower case printing
                    cbm_charset = 1;
                    break;

                case 0x12:   // RVS ON: Reverse printing on
                    reverse = true;
                    break;

                case 0x13:   // DC3: Paging off
                    //reverse = true;
                    break;

                case 0x1B:   // ESC: ASCII code for escape
                    state = MPS_PRINTER_STATE_ESC;
                    break;

                case 0x1F:   // NLQ ON: Near letter quality on
                    nlq = true;
                    break;

                case 0x8D:   // CS: Cariage return with no line feed
                    head_x  = 0;
                    break;

                case 0x91:   // CRSR UP: Upper case printing
                    cbm_charset = 0;
                    break;

                case 0x92:   // RVS OFF: Reverse printing off
                    reverse = false;
                    break;

                case 0x9F:   // NLQ OFF: Near letter quality off
                    nlq = false;
                    break;

                default:    // maybe a printable character
                    if (IsPrintable(input))
                    {
                        next_interline = interline;

                        if (input == 0x22)
                        quoted = quoted ? false : true;

                        head_x += Char(Charset2Chargen(input));
                        if (head_x > MPS_PRINTER_PAGE_PRINTABLE_WIDTH)
                        {
                            head_y += interline;
                            head_x  = 0;
                        }
                    }
                    break;
            }
            break;

        // =======  Escape sequences
        case MPS_PRINTER_STATE_ESC:
            esc_command = input;
            param_count = 0;
            switch (input)
            {
                case 0x10:  //ESC POS : Jump to horizontal position in number of dots
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x2D:  // ESC - : Underline on/off
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x34:  // ESC 4 : Italic ON
                    italic = true;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x35:  // ESC 5 : Italic OFF
                    italic = false;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x38:  // ESC 8 : Out of paper detection disabled
                    state = MPS_PRINTER_STATE_NORMAL;
                    // ignored
                    break;

                case 0x39:  // ESC 9 : Out of paper detection enabled
                    state = MPS_PRINTER_STATE_NORMAL;
                    // ignored
                    break;

                case 0x3D:  // ESC = : Down Lile Loading of user characters
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x43:  // ESC c : Set form length
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x45:  // ESC e : Emphasized printing ON
                    bold = true;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x46:  // ESC f : Emphasized printing OFF
                    bold = false;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x47:  // ESC g : Double strike printing ON
                    double_strike = true;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x48:  // ESC h : Double strike printing OFF
                    double_strike = false;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x49:  // ESC i : Select print definition
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x4E:  // ESC n : Defines bottom of from (BOF)
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x4F:  // ESC o : Clear bottom of from (BOF)
                    // Ignored in this version, usefull only for continuous paper feed
                    break;

                case 0x53:  // ESC s : Superscript/subscript printing
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x54:  // ESC t : Clear superscript/subscript printing
                    state = MPS_PRINTER_STATE_NORMAL;
                    script = MPS_PRINTER_SCRIPT_NORMAL;
                    break;

                case 0x78:  // ESC X : DRAFT/NLQ print mode selection
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x5B:  // ESC [ : Print style selection
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                default:
                    printf("Oh, undefined printer escape sequence %d\n", input);
            }

            break;

        // =======  Escape sequence parameters
        case MPS_PRINTER_STATE_ESC_PARAM:
            param_count++;
            switch(esc_command)
            {
                case 0x10:  // ESC POS : Jump to horizontal position in number of dots
                    if (param_count == 1) param_build = input << 8;
                    if (param_count == 2)
                    {
                        param_build |= input;
                        param_build <<= 2;

                        if ((param_build < MPS_PRINTER_PAGE_PRINTABLE_WIDTH) && param_build > head_x)
                        {
                            head_x = param_build;
                        }

                        state = MPS_PRINTER_STATE_NORMAL;
                    }
                    break;

                case 0x3D:  // ESC = : Down Lile Loading of user characters (parse but ignore)
                    if (param_count == 1) param_build = input;
                    if (param_count == 2) param_build |= input<<8;
                    if ((param_count > 2) && (param_count == param_build+2))
                        state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x43:  // ESC c : Set form length
                    // Ignored in this version
                    if (input != 0)
                        state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x49:  // ESC i : Select print definition
                    switch (input)
                    {
                        case 0x00:  // Draft
                        case 0x30:
                            nlq = false;
                            break;

                        case 0x02:  // NLQ
                        case 0x32:
                            nlq = true;

                            break;
                        case 0x04:  // Draft + DLL enabled (not implemented)
                        case 0x34:
                            nlq = false;
                            break;

                        case 0x06:  // NLQ + DLL enabled (not implemented)
                        case 0x36:
                            nlq = true;
                            break;
                    }
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x4E:  // ESC n : Defines bottom of from (BOF)
                    // Ignored in this version, usefull only for continuous paper feed
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x53:  // ESC S : Superscript/subscript printing
                    script = input & 0x01 ? MPS_PRINTER_SCRIPT_SUB : MPS_PRINTER_SCRIPT_SUPER;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x2D:  // ESC - : Underline on/off
                    underline = input & 0x01 ? true : false;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x58:  // ESC x : DRAFT/NLQ print mode selection
                    nlq = input & 0x01 ? true : false;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;

                case 0x5B:  // ESC [ : Print style selection (pica, elite, ...)
                    uint8_t new_step = input & 0x0F;
                    if (new_step < 7)
                        step = new_step;
                    state = MPS_PRINTER_STATE_NORMAL;
                    break;
            }
            break;

        // =======  Escape sequence parameters
        case MPS_PRINTER_STATE_PARAM:
            param_count++;
            switch(cbm_command)
            {
                case 0x08:   // BIT IMG: bitmap image
                    if (param_count == 1 && input == 26)
                    {
                        // use TAB code to handle BIT IMG SUB
                        cbm_command = 0x09;
                    }
                    else
                    {
                        if (input & 0x80)
                        {
                            head_x += Bim(input & 0x7F);
                        }
                        else
                        {
                            // Was not graphic data, reinject to interpreter
                            state = MPS_PRINTER_STATE_NORMAL;
                            Interpreter(input);

                        }
                    }
                    break;

                case 0x09:   // BIT IMG SUB: bitmap image repeated
                    if (param_count == 2)
                    {
                        // Get number of repeats
                        param_build = (input==0) ? 256 : input;
                        bim_count = 0;
                    }
                    else
                    {
                        if (input & 0x80 && bim_count < MPS_PRINTER_MAX_BIM_SUB)
                        {
                            bim_sub[bim_count++] = input & 0x7F;
                        }
                        else
                        {
                            // Was not graphic data, print bim and reinject to interpreter
                            for (int i=0; i<param_build; i++)
                                for (int j=0; j<bim_count; j++)
                                    head_x += Bim(bim_sub[j]);

                            state = MPS_PRINTER_STATE_NORMAL;
                            Interpreter(input);
                        }
                    }
                    break;

                case 0x10:  // POS : Jump to horizontal position in number of chars
                    if (param_count == 1) param_build = input & 0x0F;
                    if (param_count == 2)
                    {
                        param_build = (param_build * 10) + (input & 0x0F);
                        if (param_build < 80 && (param_build * 24 ) > head_x)
                        {
                            head_x = 24 * param_build;
                        }

                        state = MPS_PRINTER_STATE_NORMAL;
                    }
                    break;
            }
            break;

        default:
            printf("Oh, undefined printer state %d\n", state);
    }
}

/*
void
MpsPrinter::PrintString(const char *s, uint16_t x, uint16_t y)
{
    uint8_t *c = (uint8_t*) s;

    while (*c)
    {
        MpsPrinter::CharDraft(charset_epson_basic[*c], x, y);
        x+=spacing_x[step][12];
        c++;
    }
}

void
MpsPrinter::PrintStringNlq(const char *s, uint16_t x, uint16_t y)
{
    uint8_t *c = (uint8_t*) s;

    while (*c)
    {
        MpsPrinter::CharNLQ(charset_epson_basic[*c], x, y);
        x+=spacing_x[step][12];
        c++;
    }
}
*/

/****************************  END OF FILE  ****************************/
