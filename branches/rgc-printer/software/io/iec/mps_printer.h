/*
##########################################################################
##                                                                      ##
##   RENE GARCIA                        GNU General Public Licence V3   ##
##                                                                      ##
##########################################################################
##                                                                      ##
##      Project : MPS Emulator                                          ##
##      Class   : MpsPrinter                                            ##
##      Piece   : mps_printer.h                                         ##
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

#ifndef _MPS_PRINTER_H_
#define _MPS_PRINTER_H_

#ifndef NOT_ULTIMATE
#include "filemanager.h"
#endif

#include <stdint.h>
#include "lodepng.h"

/*******************************  Constants  ****************************/

#define MPS_PRINTER_PAGE_WIDTH              1984
#define MPS_PRINTER_PAGE_HEIGHT             2580
#define MPS_PRINTER_PAGE_DEPTH              2
#define MPS_PRINTER_PAGE_MARGIN_LEFT        32
#define MPS_PRINTER_PAGE_MARGIN_TOP         32
#define MPS_PRINTER_PAGE_PRINTABLE_WIDTH    1920
#define MPS_PRINTER_PAGE_PRINTABLE_HEIGHT   2516
#define MPS_PRINTER_MAX_HTABULATIONS        32

#define MPS_PRINTER_BITMAP_SIZE             ((MPS_PRINTER_PAGE_WIDTH*MPS_PRINTER_PAGE_HEIGHT*MPS_PRINTER_PAGE_DEPTH+7)>>3)

#define MPS_PRINTER_MAX_BIM_SUB             256
#define MPS_PRINTER_MAX_SPECIAL             46

#define MPS_PRINTER_SCRIPT_NORMAL           0
#define MPS_PRINTER_SCRIPT_SUPER            2
#define MPS_PRINTER_SCRIPT_SUB              4

/*********************************  Types  ******************************/

typedef enum mps_printer_states {
    MPS_PRINTER_STATE_NORMAL,
    MPS_PRINTER_STATE_PARAM,
    MPS_PRINTER_STATE_ESC,
    MPS_PRINTER_STATE_ESC_PARAM
} mps_printer_state_t;

/*======================================================================*/
/* Class MpsPrinter                                                     */
/*======================================================================*/

class MpsPrinter
{
    private:
        /* =======  Embeded data */
        /* Char Generators (bitmap definition of each character) */
        static uint8_t chargen_italic[129][12];
        static uint8_t chargen_nlq_low[404][12];
        static uint8_t chargen_nlq_high[404][12];
        static uint8_t chargen_draft[404][12];

        /* Charsets (CBM and other ASCII) */
        static uint16_t charset_cbm_us[2][256];
        static uint16_t charset_italic_cbm_us[2][256];
        //static uint16_t charset_epson_basic[256];

        /* Dot spacing on X axis depending on character width (pical, elite, compressed,...) */
        static uint8_t spacing_x[7][13];

        /* Dot spacing on Y axis depending on character style (normal, superscript, subscript) */
        static uint8_t spacing_y[6][17];

        /* CBM character specia for quote mode */
        static uint8_t cbm_special[MPS_PRINTER_MAX_SPECIAL];

        /* PNG file basename */
        char outfile[32];
        LodePNGState lodepng_state;

        /* Horizontal tabulation stops */
        uint16_t htab[MPS_PRINTER_MAX_HTABULATIONS];

        /* Page bitmap */
        uint8_t bitmap[MPS_PRINTER_BITMAP_SIZE];

        // How many pages printed since start
        int page_num;

        /* =======  Print head configuration */
        /* Ink density */
        uint8_t dot_size;

        /* Printer head position */
        uint16_t head_x;
        uint16_t head_y;

        /* Current interline value */
        uint16_t interline;
        uint16_t next_interline;

        /* =======  Current print attributes */
        bool reverse;           /* Negative characters */
        bool double_width;      /* Double width characters */
        bool nlq;               /* Near Letter Quality */
        bool clean;             /* Page is blank */
        bool quoted;            /* Commodore quoted listing */
        bool double_strike;     /* Print twice at the same place */
        bool underline;         /* Underline is on */
        bool bold;              /* Bold is on */
        bool italic;            /* Italic is on */

        /* =======  Current CBM charset (Uppercases/graphics or Lowercases/Uppercases) */
        uint8_t cbm_charset;

        /* =======  Interpreter state */
        mps_printer_state_t state;
        uint8_t param_count;
        uint32_t param_build;
        uint8_t bim_sub[MPS_PRINTER_MAX_BIM_SUB];
        uint16_t bim_count;

        /* Current CMB command waiting for a parameter */
        uint8_t cbm_command;

        /* Current ESC command waiting for a parameter */
        uint8_t esc_command;

        /* =======  1541 Ultimate FileManager */
#ifndef NOT_ULTIMATE
        FileManager *fm;
#endif
        /* =======  Current spacing configuration */
        uint8_t step;     /* X spacing */
        uint8_t script;   /* Y spacing */


        /*==============================================*/
        /*                 M E T H O D S                */
        /*==============================================*/
    private:
        /* =======  Constructors */
        MpsPrinter(char * filename = NULL);

        /* =======  Destructors */
        ~MpsPrinter(void);

    public:
        /* =======  End of current page */
        void FormFeed(void);

        /* =======  Singleton get */
        static MpsPrinter* getMpsPrinter();

        /* =======  Object customization */
        void setFilename(char * filename);
        void setCBMCharset(uint8_t cs);
        void setDotSize(uint8_t ds);

        /* =======  Feed interpreter */
        void Interpreter(const uint8_t * input, uint32_t size);
        void Interpreter(uint8_t input);
        void Reset(void);

    private:
        uint8_t Combine(uint8_t c1, uint8_t c2);
        void Clear(void);
        void Print(const char* filename);
        void Ink(uint16_t x, uint16_t y, uint8_t c=3);
        void Dot(uint16_t x, uint16_t y);
        uint16_t Charset2Chargen(uint8_t input);
        uint16_t Char(uint16_t c);
        uint16_t CharItalic(uint16_t c, uint16_t x, uint16_t y);
        uint16_t CharDraft(uint16_t c, uint16_t x, uint16_t y);
        uint16_t CharNLQ(uint16_t c, uint16_t x, uint16_t y);
        void PrintString(const char *s, uint16_t x, uint16_t y);
        void PrintStringNlq(const char *s, uint16_t x, uint16_t y);
        bool IsPrintable(uint8_t input);
        bool IsSpecial(uint8_t input);
        uint16_t Bim(uint8_t head);
};

#endif /* _MPS_PRINTER_H_ */

/****************************  END OF FILE  ****************************/
