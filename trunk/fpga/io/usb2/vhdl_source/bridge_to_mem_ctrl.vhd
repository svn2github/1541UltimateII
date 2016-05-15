library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity bridge_to_mem_ctrl is
port (
    ulpi_clock  : in  std_logic;
    ulpi_reset  : in  std_logic;

    nano_addr   : in  unsigned(7 downto 0);
    nano_write  : in  std_logic;
    nano_wdata  : in  std_logic_vector(15 downto 0);

    -- cmd interface
    sys_clock   : in  std_logic;
    sys_reset   : in  std_logic;

    cmd_addr    : out std_logic_vector(3 downto 0);
    cmd_valid   : out std_logic;
    cmd_write   : out std_logic;
    cmd_wdata   : out std_logic_vector(15 downto 0);
    cmd_ack     : in  std_logic );
end entity;

architecture gideon of bridge_to_mem_ctrl is
    signal fifo_data_in     : std_logic_vector(19 downto 0);
    signal fifo_write       : std_logic;
    signal cmd_data_out     : std_logic_vector(19 downto 0);
begin
    fifo_data_in <= std_logic_vector(nano_addr(3 downto 0)) & nano_wdata;
    fifo_write <= '1' when (nano_addr(7 downto 4)=X"7" and nano_write='1') else '0';
    
    i_cmd_fifo: entity work.async_fifo_ft
    generic map (
        g_data_width => 20,
        g_depth_bits => 3 )
    port map (
        -- write port signals (synchronized to write clock)
        wr_clock       => ulpi_clock,
        wr_reset       => ulpi_reset,
        wr_en          => fifo_write,
        wr_din         => fifo_data_in,
        wr_full        => open,

        -- read port signals (synchronized to read clock)
        rd_clock        => sys_clock,
        rd_reset        => sys_reset,
        rd_next         => cmd_ack,
        rd_dout         => cmd_data_out,
        rd_valid        => cmd_valid );

    cmd_addr  <= cmd_data_out(19 downto 16);
    cmd_wdata <= cmd_data_out(15 downto 0);
    cmd_write <= '1'; -- we don't support reads yet
end architecture;
