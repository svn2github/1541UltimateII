library ieee;
use ieee.std_logic_1164.all;

library mblite;
use mblite.config_Pkg.all;
use mblite.core_Pkg.all;
use mblite.std_Pkg.all;

library work;

entity cached_mblite is
port (
    clock       : in  std_logic;
    reset       : in  std_logic;

    invalidate  : in  std_logic := '0';
    inv_addr    : in  std_logic_vector(31 downto 0);
    
    dmem_o      : out dmem_out_type;
    dmem_i      : in  dmem_in_type;

    imem_o      : out dmem_out_type;
    imem_i      : in  dmem_in_type;

    irq_i       : in  std_logic;
    irq_o       : out std_logic );

end entity;

architecture structural of cached_mblite is

    -- signals from processor to cache
    signal cimem_o : imem_out_type;
    signal cimem_i : imem_in_type;
    signal cdmem_o : dmem_out_type;
    signal cdmem_i : dmem_in_type;

BEGIN
    core0 : core
    port map (
        imem_o => cimem_o,
        imem_i => cimem_i,
        dmem_o => cdmem_o,
        dmem_i => cdmem_i,
        int_i  => irq_i,
        int_o  => irq_o,
        rst_i  => reset,
        clk_i  => clock );

    i_cache: entity work.dm_simple
    generic map (
        g_data_register => true,
        g_mem_direct    => true )
    port map (
        clock  => clock,
        reset  => reset,

        dmem_i.adr_o => cimem_o.adr_o,
        dmem_i.ena_o => cimem_o.ena_o,
        dmem_i.sel_o => "0000",
        dmem_i.we_o  => '0',
        dmem_i.dat_o => (others => '0'),
        
        dmem_o.ena_i => cimem_i.ena_i,
        dmem_o.dat_i => cimem_i.dat_i,

        mem_o  => imem_o,
        mem_i  => imem_i );
    
    d_cache: entity work.dm_with_invalidate
--    generic map (
--        g_address_swap  => X"00010000"
    port map (
        clock      => clock,
        reset      => reset,

        invalidate => invalidate,
        inv_addr   => inv_addr,

        dmem_i     => cdmem_o,
        dmem_o     => cdmem_i,
                   
        mem_o      => dmem_o,
        mem_i      => dmem_i );

--    arb: entity work.dmem_arbiter
--    port map (
--        clock  => clock,
--        reset  => reset,
--        imem_i => imem_o,
--        imem_o => imem_i,
--        dmem_i => dmem_o,
--        dmem_o => dmem_i,
--        mmem_o => mmem_o,
--        mmem_i => mmem_i  );
--    
--    process(clock)
--    begin
--        if rising_edge(clock) then
--            if cdmem_i.ena_i='1' and cimem_i.ena_i='1' then
--                stuck <= '0';
--                stuck_cnt <= 0;
--            elsif stuck_cnt = 31 then
--                stuck <= '1';
--            else
--                stuck_cnt <= stuck_cnt + 1;
--            end if;
--        end if;
--    end process;

end architecture;
