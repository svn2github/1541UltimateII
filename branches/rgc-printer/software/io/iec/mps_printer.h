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

/*********************************  Types  ******************************/

typedef enum mps_printer_states {
    MPS_PRINTER_STATE_NORMAL,
    MPS_PRINTER_STATE_PARM,
    MPS_PRINTER_STATE_ESC,
    MPS_PRINTER_STATE_ESC_PARAM,
    MPS_PRINTER_STATE_BIM
} mps_printer_state_t;

/*======================================================================*/
/* Class MpsPrinter                                                     */
/*======================================================================*/

class MpsPrinter
{
    private:
        /* =======  Embeded data */
        /* Char Generators (bitmap definition of each character) */
        static unsigned char chargen_italic[129][12];
        static unsigned char chargen_nlq_low[404][12];
        static unsigned char chargen_nlq_high[404][12];
        static unsigned char chargen_draft[404][12];

        /* Charsets (CBM and other ASCII) */
        static unsigned short charset_cbm_us[2][256];
        static unsigned short charset_italic_cbm_us[2][256];
        //static unsigned short charset_epson_basic[256];

        /* Dot spacing on X axis depending on character width (pical, elite, compressed,...) */
        static unsigned char spacing_x[7][13];

        /* Dot spacing on Y axis depending on character style (normal, superscript, subscript) */
        static unsigned char spacing_y[6][17];

        /* PNG file basename */
        char outfile[32];
        LodePNGState lodepng_state;

        /* Horizontal tabulation stops */
        unsigned short htab[MPS_PRINTER_MAX_HTABULATIONS];

        /* Page bitmap */
    public:
        unsigned char bitmap[MPS_PRINTER_BITMAP_SIZE];


        // How many pages printed since start
        int page_num;

        /* =======  Print head configuration */
        /* Ink density */
        unsigned char dot_size;

        /* Printer head position */
        unsigned short head_x;
        unsigned short head_y;

        /* Current interline value */
        unsigned short interline;

        /* =======  Current print attributes */
        bool reverse;           /* Negative characters */
        bool double_width;      /* Double width characters */
        bool nlq;               /* Near Letter Quality */
        bool clean;             /* Page is blank */
        bool bim_only;          /* Line only contains Bitmap Image */
        bool quoted;            /* Commodore quoted listing */
        bool double_strike;     /* Print twice at the same place */
        bool underline;         /* Underline is on */
        bool bold;              /* Bold is on */
        bool italic;            /* Italic is on */

        /* =======  Current CBM charset (Uppercases/graphics or Lowercases/Uppercases) */
        unsigned char cbm_charset;

        /* =======  Interpreter state */
        mps_printer_state_t state;

        /* Current CMB command waiting for a parameter */
        unsigned char cbm_command;
        unsigned char cbm_param_count;

        /* Current ESC command waiting for a parameter */
        unsigned char esc_command;
        unsigned char esc_param_count;

        /* =======  1541 Ultimate FileManager */
#ifndef NOT_ULTIMATE
        FileManager *fm;
#endif
        /* =======  Current spacing configuration */
        unsigned char step;     /* X spacing */
        unsigned char script;   /* Y spacing */

        /* =======  Singleton */
        static MpsPrinter m_mpsInstance;


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
        void setCBMCharset(char cs);
        void setDotSize(char ds);

        /* =======  Feed interpreter */
        void Interpreter(const unsigned char * input, unsigned long size);
        void Interpreter(unsigned char input);

    private:
        int Combine(int c1, int c2);
        void Clear(void);
        void Print(const char* filename);
        void Ink(int x, int y, int c=3);
        void Dot(int x, int y);
        unsigned short Charset2Chargen(unsigned char input);
        int Char(unsigned short c);
        int CharItalic(unsigned short c, int x, int y);
        int CharDraft(unsigned short c, int x, int y);
        int CharNLQ(unsigned short c, int x, int y);
        void PrintString(const char *s, int x, int y);
        void PrintStringNlq(const char *s, int x, int y);
        bool IsPrintable(unsigned char input);
        int Bim(unsigned char head);
};

#endif /* _MPS_PRINTER_H_ */

/****************************  END OF FILE  ****************************/
