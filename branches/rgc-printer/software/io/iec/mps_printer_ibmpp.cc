/*
##########################################################################
##                                                                      ##
##   RENE GARCIA                        GNU General Public Licence V3   ##
##                                                                      ##
##########################################################################
##                                                                      ##
##      Project : MPS Emulator                                          ##
##      Class   : MpsPrinter                                            ##
##      Piece   : mps_printer_ibmpp.cc                                  ##
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
#ifdef NOT_ULTIMATE
#include <strings.h>
#endif
#include "mps_printer.h"

/************************************************************************
*                                                                       *
*               G L O B A L   M O D U L E   V A R I A B L E S           *
*                                                                       *
************************************************************************/


/************************************************************************
*                  MpsPrinter::IBMpp_Interpreter(data)         Private  *
*                  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                  *
* Function : Epson FX-80 single data interpreter automata               *
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
MpsPrinter::IBMpp_Interpreter(uint8_t input)
{
    switch(state)
    {
        case MPS_PRINTER_STATE_INITIAL:

            /* =======  Select action if command char received */
            cbm_command = input;
            param_count = 0;
            switch(input)
            {
                case 0x07:   // Bell
                    // ignore
                    break;

                case 0x08:   // Backspace
                    {
                        uint16_t cwidth = Char(Charset2Chargen(' '));
                        if (cwidth <= head_x)
                            head_x -= cwidth;
                        else
                            head_x = 0;
                    }
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

                case 0x0A:   // LF: line feed (LF+CR)
                    head_y += next_interline;
                    head_x  = margin_left;
                    if (head_y > margin_bottom)
                        FormFeed();
                    break;

                case 0x0B:   // VT: line feed or next vertical tabulation
                    {
                        int i = 0;
                        while (i < MPS_PRINTER_MAX_VTABULATIONS && vtab[i] < MPS_PRINTER_PAGE_PRINTABLE_HEIGHT)
                        {
                            if (vtab[i] > head_y)
                            {
                                head_y = vtab[i];
                                break;
                            }

                            i++;
                        }
                    }
                    break;

                case 0x0C:   // FF: form feed
                    FormFeed();
                    break;

                case 0x0D:   // CR: carriage return (CR only, no LF)
                    head_x  = margin_left;
                    break;

                case 0x0E:   // SO: Double width printing ON
                    double_width = true;
                    break;

                case 0x0F:   // SI: 17.1 chars/inch on
                    step = MPS_PRINTER_STEP_CONDENSED;
                    break;

                case 0x11:   // DC1: Printer select
                    // ignore
                    break;

                case 0x12:   // DC2: 17.1 chars/inch off
                    step = MPS_PRINTER_STEP_PICA;
                    break;

                case 0x13:   // DC3: No operation
                    // ignore
                    break;

                case 0x14:   // DC4: Double width printing off
                    double_width = false;
                    break;

                case 0x18:   // CAN: Clear print buffer
                    // ignored
                    break;

                case 0x1B:   // ESC: ASCII code for escape
                    state = MPS_PRINTER_STATE_ESC;
                    break;

                default:    // maybe a printable character
                    if (IsPrintable(input))
                    {
                        next_interline = interline;

                        head_x += Char(Charset2Chargen(input));
                        if (head_x > margin_right)
                        {
                            head_y += interline;
                            head_x  = 0;

                            if (head_y > margin_bottom)
                                FormFeed();
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
                case 0x2D:  // ESC - : Underline on/off
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x30:  // ESC 0 : Spacing = 1/8"
                    interline = 27;
                    next_interline = interline;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x31:  // ESC 1 : Spacing = 7/72"
                    interline = 21;
                    next_interline = interline;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x32:  // ESC 2 : Spacing = 1/6"
                    interline = 36;
                    next_interline = interline;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x33:  // ESC 3 : Spacing = n/216"
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x34:  // ESC 4 : Set Top Of Form (TOF)
                    // to be done
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x35:  // ESC 5 : Automatic LF ON/OFF
                    // to be done
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x36:  // ESC 6 : IBM Table 2 selection
                    // to be done
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x37:  // ESC 7 : IBM Table 1 selection
                    // to be done
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x3A:  // ESC : : Print pitch = 1/12"
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x3D:  // ESC = : Down Load Loading of user characters (DLL)
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x41:  // ESC A : Spacing = n/72"
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x42:  // ESC B : Vertical TAB stops program
                    state = MPS_PRINTER_STATE_INITIAL;
                    // te be done
                    break;

                case 0x43:  // ESC C : Set form length
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x44:  // ESC D : Horizontal TAB stops program
                    state = MPS_PRINTER_STATE_INITIAL;
                    // te be done
                    break;

                case 0x45:  // ESC E : Emphasized printing ON
                    bold = true;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x46:  // ESC F : Emphasized printing OFF
                    bold = false;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x47:  // ESC G : Double Stike Printing ON
                    double_strike = true;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x48:  // ESC H : Double Stike Printing OFF
                    double_strike = false;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x49:  // ESC I : Select print definition
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    // to be done
                    break;

                case 0x4A:  // ESC J : Skip n/216" of paper
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    // to be done
                    break;

                case 0x4B:  // ESC K : Set normal density graphics
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x4C:  // ESC L : Set double density graphics
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x4E:  // ESC N : Defines bottom of from (BOF) in lines
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x4F:  // ESC O : Clear bottom of from (BOF)
                    // Ignored in this version, usefull only for continuous paper feed
                    break;

                case 0x51:  // ESC Q : De-select printer
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    // to be done
                    break;

                case 0x52:  // ESC R : Clear tab stops
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    // to be done
                    break;

                case 0x53:  // ESC S : Superscript/subscript printing
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    break;

                case 0x54:  // ESC T : Clear superscript/subscript printing
                    state = MPS_PRINTER_STATE_INITIAL;
                    script = MPS_PRINTER_SCRIPT_NORMAL;
                    break;

                case 0x55:  // ESC U : Mono/Bidirectional printing
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    // to be done
                    break;

                case 0x57:  // ESC W : Double width characters ON/OFF
                    state = MPS_PRINTER_STATE_ESC_PARAM;
                    // to be done
                    break;

                case 0x59:  // ESC Y : Double dentity BIM selection, normal speed
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x5A:  // ESC Z : Four times density BIM selection
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x5C:  // ESC \ : Print n characters from extended table
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x5E:  // ESC ^ : Print one character from extended table
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x5F:  // ESC _ : Overline ON/OFF
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                default:
                    DBGMSGV("undefined IBM Pro Printer escape sequence %d", input);
            }

            break;

        // =======  Escape sequence parameters
        case MPS_PRINTER_STATE_ESC_PARAM:
            param_count++;
            switch(esc_command)
            {
                case 0x33:  // ESC 3 : Spacing = n/216"
                    interline = input;
                    next_interline = interline;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x3D:  // ESC = : Down Lile Loading of user characters (parse but ignore)
                    if (param_count == 1) param_build = input;
                    if (param_count == 2) param_build |= input<<8;
                    if ((param_count > 2) && (param_count == param_build+2))
                        state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x41:  // ESC A : Spacing = n/72"
                    interline = input * 3;
                    next_interline = interline;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x43:  // ESC C : Set form length
                    // Ignored in this version
                    if (input != 0)
                        state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x49:  // ESC I : Extend printable characters set
                    // to be done
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x4A:  // ESC J : Skip n/216" of paper
                    state = MPS_PRINTER_STATE_INITIAL;
                    // to be done
                    break;

                case 0x4E:  // ESC N : Defines bottom of from (BOF)
                    // Ignored in this version, usefull only for continuous paper feed
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x53:  // ESC S : Superscript/subscript printing
                    script = input & 0x01 ? MPS_PRINTER_SCRIPT_SUB : MPS_PRINTER_SCRIPT_SUPER;
                    state = MPS_PRINTER_STATE_INITIAL;
                    break;

                case 0x2D:  // ESC - : Underline on/off
                    underline = input & 0x01 ? true : false;
                    state = MPS_PRINTER_STATE_INITIAL;
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
                            head_x += CBMBim(input & 0x7F);
                        }
                        else
                        {
                            // Was not graphic data, reinject to interpreter
                            state = MPS_PRINTER_STATE_INITIAL;
                            IBMpp_Interpreter(input);

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
                                    head_x += CBMBim(bim_sub[j]);

                            state = MPS_PRINTER_STATE_INITIAL;
                            IBMpp_Interpreter(input);
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

                        state = MPS_PRINTER_STATE_INITIAL;
                    }
                    break;
            }
            break;

        default:
            DBGMSGV("undefined printer state %d", state);
    }
}

/****************************  END OF FILE  ****************************/
