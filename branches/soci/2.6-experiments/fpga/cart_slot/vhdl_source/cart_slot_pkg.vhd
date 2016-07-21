library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package cart_slot_pkg is

    constant c_cart_c64_mode            : unsigned(4 downto 0) := "0" & X"0";
    constant c_cart_c64_stop            : unsigned(4 downto 0) := "0" & X"1";
    constant c_cart_c64_stop_mode       : unsigned(4 downto 0) := "0" & X"2";
    constant c_cart_c64_clock_detect    : unsigned(4 downto 0) := "0" & X"3";
    constant c_cart_cartridge_rom_base  : unsigned(4 downto 0) := "0" & X"4";
    constant c_cart_cartridge_type      : unsigned(4 downto 0) := "0" & X"5";
    constant c_cart_cartridge_kill      : unsigned(4 downto 0) := "0" & X"6";
    constant c_cart_kernal_enable       : unsigned(4 downto 0) := "0" & X"7";
    constant c_cart_reu_enable          : unsigned(4 downto 0) := "0" & X"8";
    constant c_cart_reu_size            : unsigned(4 downto 0) := "0" & X"9";
    constant c_cart_swap_buttons        : unsigned(4 downto 0) := "0" & X"A";
    constant c_cart_timing              : unsigned(4 downto 0) := "0" & X"B";
    constant c_cart_phi2_recover        : unsigned(4 downto 0) := "0" & X"C";
    constant c_cart_cartridge_ram_base  : unsigned(4 downto 0) := "0" & X"D";
    constant c_cart_sampler_enable      : unsigned(4 downto 0) := "0" & X"E";
    constant c_cart_ethernet_enable     : unsigned(4 downto 0) := "0" & X"F";
    constant c_cart_ata                 : unsigned(4 downto 0) := "1" & X"0";
                                                   
    type t_cart_control is record
        c64_reset      : std_logic;
        c64_nmi        : std_logic;
        c64_ultimax    : std_logic;
        c64_stop       : std_logic;
        c64_stop_mode  : std_logic_vector(1 downto 0);
        cartridge_type : std_logic_vector(4 downto 0);
        cartridge_kill : std_logic;
        kernal_enable  : std_logic;
        reu_enable     : std_logic;
        reu_size       : std_logic_vector(2 downto 0);
        eth_enable     : std_logic;
        sampler_enable : std_logic;
	swap_buttons   : std_logic;
        timing_addr_valid   : unsigned(2 downto 0);
        phi2_edge_recover   : std_logic;
        ata_err             : std_logic;
        ata_drq             : std_logic;
        ata_drdy            : std_logic;
        ata_bsy             : std_logic;
    end record;
    
    type t_cart_status is record
        c64_stopped    : std_logic;
        clock_detect   : std_logic;
        ata_data       : std_logic;
        ata_sel        : std_logic;
        ata_cmd        : std_logic;
        ata_rst        : std_logic;
    end record;

    constant c_cart_control_init : t_cart_control := (
        c64_nmi        => '0',
        c64_reset      => '0',
        c64_ultimax    => '0',
        c64_stop       => '0',
        c64_stop_mode  => "00",
        cartridge_type => "00000",
        cartridge_kill => '0',
        kernal_enable  => '0',
        reu_enable     => '0',
        reu_size       => "111",
        eth_enable     => '0',
        sampler_enable => '0',
        timing_addr_valid => "100",
        phi2_edge_recover => '1',
        swap_buttons   => '1',
        ata_err        => '0',
        ata_drq        => '0',
        ata_drdy       => '1',
        ata_bsy        => '1');
    

end cart_slot_pkg;
