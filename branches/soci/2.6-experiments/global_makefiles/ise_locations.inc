################################################################################
##
## (C) COPYRIGHT 2010 Gideon's Logic Architectures'
##
################################################################################
## XILINX ISE locations include file
##
## rename this file to ise_locations.inc to add it to the make file. Change 
## the directory locations to that what correspond to your system.
################################################################################

PLATFORM=lin64
ISE_VERSION	?= 13	# when not defined use highest version available

export ISE_LOCATIONS_FILE_INCLUDED=yes

export MYXILINX=/cygdrive/c/Xilinx/13.2/ISE_DS/ISE
export XILINX=C:\\Xilinx\\13.2\\ISE_DS\\ISE
PLATFORM=nt

export LMC_HOME				+= $(MYXILINX)/smartmodel/$(PLATFORM)/installed_lin
export LD_LIBRARY_PATH		+= $(MYXILINX)/bin/$(PLATFORM):/usr/X11R6/lib
export NPX_PLUGIN_PATH		+= $(MYXILINX)/java/${PLATFORM}/jre/plugin/i386/ns4

export XST 	= $(MYXILINX)/bin/$(PLATFORM)/xst
export NGDBUILD = $(MYXILINX)/bin/$(PLATFORM)/ngdbuild
export MAP	= $(MYXILINX)/bin/$(PLATFORM)/map
export PAR	= $(MYXILINX)/bin/$(PLATFORM)/par
export TRCE	= $(MYXILINX)/bin/$(PLATFORM)/trce
export BITGEN	= $(MYXILINX)/bin/$(PLATFORM)/bitgen
export CPLDFIT	= $(MYXILINX)/bin/$(PLATFORM)/cpldfit
export HPREP6	= $(MYXILINX)/bin/$(PLATFORM)/hprep6
export PROMGEN	= $(MYXILINX)/bin/$(PLATFORM)/promgen

