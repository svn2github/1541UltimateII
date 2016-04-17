library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.slot_bus_pkg.all;

entity all_carts_v4 is
generic (
    g_kernal_base   : std_logic_vector(27 downto 0) := X"0ECC000"; -- multiple of 16K
    g_rom_base      : std_logic_vector(27 downto 0) := X"0F00000"; -- multiple of 1M
    g_ram_base      : std_logic_vector(27 downto 0) := X"0EF0000" ); -- multiple of 64K
    --g_rtc_base      : std_logic_vector(27 downto 0) := X"4060400";
port (
    clock           : in  std_logic;
    reset           : in  std_logic;
    
    RST_in          : in  std_logic;
    c64_reset       : in  std_logic;
    phi2_recovered  : in  std_logic;
    aec_recovered   : in  std_logic;
    mem_rack        : in  std_logic;
    
    kernal_enable   : in  std_logic;
    kernal_area     : in  std_logic;
    freeze_trig     : in  std_logic; -- goes '1' when the button has been pressed and we're waiting to enter the freezer
    freeze_act      : in  std_logic; -- goes '1' when we need to switch in the cartridge for freeze mode
    unfreeze        : out std_logic; -- indicates the freeze logic to switch back to non-freeze mode.
    
    cart_kill       : in  std_logic;
    cart_logic      : in  std_logic_vector(3 downto 0);   -- 1 out of 16 logic emulations
    
    slot_req        : in  t_slot_req;
    slot_resp       : out t_slot_resp;

    epyx_timeout    : in  std_logic;
    ata_err         : in  std_logic;
    ata_drq         : in  std_logic;
    ata_drdy        : in  std_logic;
    ata_bsy         : in  std_logic;
    ata_data        : out std_logic;
    ata_cmd         : out std_logic;
    ata_rst         : out std_logic;
    serve_enable    : out std_logic; -- enables fetching bus address PHI2=1
    serve_rom       : out std_logic; -- ROML or ROMH
    serve_empty     : out std_logic; -- Ultimax empty
    serve_io1       : out std_logic; -- IO1n
    serve_io2       : out std_logic; -- IO2n
    allow_write     : out std_logic;
    
    mem_addr        : out unsigned(25 downto 0);   
    mem_rwn         : in  std_logic;
    
    irq_n           : out std_logic;
    nmi_n           : out std_logic;
    exrom_n         : out std_logic;
    game_n          : out std_logic;
    game_n_or       : out std_logic;

    CART_LEDn       : out std_logic );

end all_carts_v4;    

architecture gideon of all_carts_v4 is
    signal bank_bits    : std_logic_vector(18 downto 13);
    signal mode_bits    : std_logic_vector(2 downto 0);
    
    signal freeze_act_d : std_logic;

    signal cart_en      : std_logic;

    signal reu_mapping  : std_logic;
    signal allow_bank   : std_logic;
    signal clock_port   : std_logic;
    signal write_once   : std_logic;
    signal no_freeze    : std_logic;

    signal ata_bsy_i    : std_logic;
    signal ata_cmd_i    : std_logic;
    signal ata_data_i   : std_logic;
    signal ata_select   : std_logic;
    signal ata_head     : std_logic_vector(3 downto 0);
    signal ata_rst_i    : std_logic;
    signal ata_rst2_i   : std_logic;
    signal clock_addr   : unsigned(4 downto 0);
    signal clock_ram    : std_logic;
    signal clock_write  : std_logic;
    signal io_range     : boolean;
    signal clock_range  : boolean;
    signal rr_range     : boolean;
    signal cart_logic_d : std_logic_vector(cart_logic'range) := (others => '0');
    signal mem_addr_i   : std_logic_vector(19 downto 0);
    signal mem_ram      : std_logic;
        
    signal ata_buffer_pointer : unsigned(11 downto 0);
    signal ata_buffer_pointer2 : unsigned(11 downto 0);
    signal ata_increment    : std_logic;
    signal ata_rewind       : std_logic;
    signal clock_increment  : std_logic;

    constant c_none         : std_logic_vector(3 downto 0) := "0000";
    constant c_8k           : std_logic_vector(3 downto 0) := "0001";
    constant c_16k          : std_logic_vector(3 downto 0) := "0010";
    constant c_16k_umax     : std_logic_vector(3 downto 0) := "0011";
    constant c_fc3          : std_logic_vector(3 downto 0) := "0100";
    constant c_ss5          : std_logic_vector(3 downto 0) := "0101";
    constant c_retro        : std_logic_vector(3 downto 0) := "0110";
    constant c_action       : std_logic_vector(3 downto 0) := "0111";
    constant c_system3      : std_logic_vector(3 downto 0) := "1000";
    constant c_domark       : std_logic_vector(3 downto 0) := "1001";
    constant c_ocean128     : std_logic_vector(3 downto 0) := "1010";
    constant c_ocean256     : std_logic_vector(3 downto 0) := "1011";
    constant c_easy_flash   : std_logic_vector(3 downto 0) := "1100";
    constant c_epyx         : std_logic_vector(3 downto 0) := "1110";
    constant c_idedos       : std_logic_vector(3 downto 0) := "1111";

    constant c_ata_z        : std_logic_vector(7 downto 0) := X"7F";

    -- alias
    signal slot_addr        : std_logic_vector(15 downto 0);
    signal io_read          : std_logic;
    signal io_write         : std_logic;
    signal io_addr          : std_logic_vector(8 downto 0);
    signal io_wdata         : std_logic_vector(7 downto 0);
    signal io_rdata         : std_logic_vector(7 downto 0);
    signal io_reg_out       : std_logic;
begin
    slot_addr <= std_logic_vector(slot_req.bus_address);
    io_write  <= slot_req.io_write;
    io_read   <= slot_req.io_read;
    io_addr   <= std_logic_vector(slot_req.io_address(8 downto 0));
    io_wdata  <= slot_req.data;
    
    process(clock)
    begin
        if rising_edge(clock) then
            unfreeze     <= '0';
            freeze_act_d <= freeze_act;
            
            -- control register
            if reset = '1' or RST_in = '1' or c64_reset = '1' then
                cart_logic_d <= cart_logic; -- activate change of mode!
                mode_bits    <= (others => '0');
                bank_bits    <= (others => '0');
                allow_bank   <= '0';
                clock_port   <= '0';
                write_once   <= '0';
                no_freeze    <= '0';
                reu_mapping  <= '0';
                cart_en      <= '1';
                ata_select   <= '0';
                ata_head     <= (others => '0');
                ata_rst_i    <= '0';
                ata_bsy_i    <= '1';
                clock_write  <= '0';
            elsif cart_en = '0' then
                cart_logic_d <= cart_logic; -- activate change of mode!
            end if;

            ata_rewind <= '0';
            if ata_rewind = '1' then
                ata_buffer_pointer2 <= (others => '0');
                ata_buffer_pointer <= (others => '0');
            elsif ata_increment = '1' and phi2_recovered='1' then
                if ata_buffer_pointer(7 downto 0) = X"FF" then
                    ata_data_i <= '1';
                    ata_bsy_i <= '1';
                end if;
                ata_buffer_pointer2 <= ata_buffer_pointer;
                ata_buffer_pointer <= ata_buffer_pointer + 1;
                ata_increment <= '0';
            end if;
            -- clock address counter
            if io_addr(8 downto 0) = "0" & X"FB" and io_write = '1' then
                clock_addr <= unsigned(io_wdata(5 downto 1));
                clock_increment <= '0';
            elsif mem_rack='1' and clock_increment = '1' then
                clock_addr <= clock_addr + 1;
                clock_increment <= '0';
            end if;
            if ata_bsy = '1' then
                ata_bsy_i  <= '0';
                ata_cmd_i  <= '0';
                ata_data_i <= '0';
            end if;
            if ata_drdy = '1' then
                ata_rst_i <= ata_rst2_i;
            end if;
            
            case cart_logic_d is
            when c_fc3 =>
                if io_write = '1' and io_addr(8 downto 0) = "1" & X"FF" then -- DFFF
                    if cart_en = '1' or freeze_act = '1' then -- accessible when enabled or during freeze
                        bank_bits(15 downto 14) <= io_wdata(1 downto 0); -- 4x16K banks
                        mode_bits <= io_wdata(6) & io_wdata(4) & io_wdata(5); -- NMI, EXROM, GAME
                        cart_en   <= not io_wdata(7); -- disable, but only this register!
                    end if;
                end if;
                -- in reality freezing is done when the button is released
                -- the software holds NMI down to prevent further triggers
                -- so that's a good indicator when to finish pressing
                unfreeze <= not mode_bits(2);

            when c_idedos =>
                if cart_en = '1' and io_addr(8) = '0' then
                    case io_addr(7 downto 4) is
                        when X"2" =>
                            case io_addr(3 downto 0) is
                                when X"0" =>
                                    if (io_write = '1' or io_read = '1') and ata_select = '0' and ata_drq = '1' then
                                        ata_increment <= '1';
                                    end if;
                                when X"6" => 
                                    if io_write = '1' and ata_rst_i = '0' then
                                        ata_select <= io_wdata(4);
                                        ata_head <= io_wdata(3 downto 0);
                                    end if;
                                when X"7" => 
                                    if io_write ='1' and ata_select = '0' then
                                        ata_rewind <= '1';
                                        ata_cmd_i <= '1';
                                        ata_bsy_i <= '1';
                                    end if;
                                when X"E" => 
                                    if io_write = '1' then
                                        ata_rst2_i <= io_wdata(2);
                                        if io_wdata(2) = '1' then
                                            ata_rst_i <= '1';
                                            ata_select <= '0';
                                            ata_head <= (others => '0');
                                        end if;
                                    end if;
                                when others =>
                                    null;
                            end case;
                        when X"5" =>
                            if io_addr(3 downto 0) = X"F" and (io_write = '1' or io_read = '1') then
                                clock_increment <= '1';
                            end if;
                        when X"6" =>
                            if io_addr(3) = '0' and io_write = '1' then
                                bank_bits(16 downto 14) <= io_addr(2 downto 0);
                            end if;
                        when X"F" =>
                            if io_addr(3 downto 2) = "11" and io_write = '1' then
                                mode_bits(1 downto 0) <= io_addr(1) & not io_addr(0);
                                unfreeze <= '1';
                            end if;
                            if io_addr(3 downto 0) = X"B" and io_write = '1' then
                                if io_wdata(0) = '1' then
                                    cart_en <= '0';
                                    mode_bits(1 downto 0) <= "10";
                                end if;
                                clock_ram <= io_wdata(6);
                                clock_write <= io_wdata(7);
                            end if;
                        when others =>
                            null;
                    end case;
                end if;

                if freeze_act = '1' then
                    if mode_bits(1 downto 0) = "10" then
                        cart_en <= '1';
                    else
                        unfreeze <= '1';
                    end if;
                end if;
                                    
            when c_retro =>
                if io_write='1' and io_addr(8 downto 1) = X"00" and cart_en='1' then -- DE00/DE01
                    bank_bits(15) <= io_wdata(7); -- 8x8K banks
                    bank_bits(14 downto 13) <= io_wdata(4 downto 3);
                    if io_addr(0) = '0' then -- DE00
                        mode_bits <= io_wdata(5) & io_wdata(1 downto 0); -- RAM/ROM, EXROM, GAME
                        unfreeze  <= io_wdata(6);
                        cart_en   <= not io_wdata(2);
                    else                     -- DE01
                        if write_once = '0' then
                            reu_mapping <= io_wdata(6);
                            no_freeze <= io_wdata(2);
                            allow_bank <= io_wdata(1);
                            write_once <= '1';
                        end if;
                        clock_port <= io_wdata(0);
                    end if;
                end if;
                if freeze_act='1' and freeze_act_d='0' and no_freeze = '0' then
                    bank_bits  <= (others => '0');
                    mode_bits  <= (others => '0');
                    cart_en    <= '1';
                end if;

            when c_action =>
                -- the real cartridge also triggers on read, but that's an annoying bug
                if io_write='1' and io_addr(8) = '0' and cart_en='1' then -- DExx
                    bank_bits(14 downto 13) <= io_wdata(4 downto 3); -- 4x8K banks
                    mode_bits <= io_wdata(5) & io_wdata(1 downto 0); -- RAM/ROM, EXROM, GAME
                    unfreeze  <= io_wdata(6);
                    cart_en   <= not io_wdata(2);
                end if;
                if freeze_act='1' and freeze_act_d='0' then
                    bank_bits  <= (others => '0');
                    mode_bits  <= (others => '0');
                    cart_en    <= '1';
                end if;

            when c_easy_flash =>
                if io_write='1' and io_addr(8)='0' and cart_en='1' then -- DExx
                    if io_addr(1)='0' then -- DE00
                        bank_bits <= io_wdata(5 downto 0);
                    else -- DE02
                        mode_bits <= io_wdata(2 downto 0); -- LED(7), M X G
                    end if;
                end if;
                
            when c_ss5 =>
                if io_write='1' and io_addr(8) = '0' and cart_en='1' then -- DE00-DEFF
                    bank_bits(15 downto 14) <= io_wdata(4) & io_wdata(2);
                    mode_bits <= io_wdata(3) & io_wdata(1) & io_wdata(0);
                    unfreeze  <= not io_wdata(0);
                    cart_en   <= not io_wdata(3);
                end if;
                if freeze_act='1' and freeze_act_d='0' then
                    bank_bits  <= (others => '0');
                    mode_bits  <= (others => '0');
                    cart_en    <= '1';
                end if;

            when c_8k | c_16k_umax =>
                if io_write='1' and io_addr(8 downto 0) = "111111111" then -- DFFF
                    cart_en <= '0'; -- permanent off
                end if;

            when c_ocean128 =>
                if io_write='1' and io_addr(8)='0' then -- DE00 range
                    bank_bits <= io_wdata(5 downto 0);
                end if;
            
            when c_domark =>
                if io_write='1' and io_addr(8)='0' then -- DE00 range
                    bank_bits(17 downto 13) <= io_wdata(4 downto 0);
                    mode_bits(0) <= io_wdata(7);
--                    if io_wdata(7 downto 5) /= "000" then -- permanent off
--                        cart_en <= '0';
--                    end if;
                    cart_en <= not (io_wdata(7) or io_wdata(6) or io_wdata(5));
                end if;

            when c_ocean256 =>
                if io_write='1' and io_addr(8)='0' then -- DE00 range
                    bank_bits(16 downto 13) <= io_wdata(3 downto 0);
                end if;

            when c_system3 => -- 16K, only 8K used?
                if (io_write='1' or io_read='1') and io_addr(8)='0' then -- DE00 range
                    bank_bits <= io_addr(5 downto 0);
                end if;
            
            when others =>
                null;

            end case;
            
            if cart_kill='1' then
                cart_en  <= '0';
            end if;
        end if;
    end process;
    
    CART_LEDn <= not cart_en;

    io_range    <= slot_addr(15 downto 9) = X"D" & "111"; -- DE00-DFFF
    clock_range <= io_range and (slot_addr(8 downto 4)='0' & X"0"); -- DE00-DE0F
    rr_range    <= clock_range and (slot_addr(3 downto 1)="000"); -- DE00-DE01

    -- determine address
--  process(cart_logic_d, cart_base_d, slot_addr, mode_bits, bank_bits, reu_mapping, allow_bank)
    process(cart_logic_d, slot_addr, mode_bits, bank_bits, reu_mapping, allow_bank, ata_select, ata_buffer_pointer2, ata_buffer_pointer, ata_drq, ata_bsy_i, ata_bsy, clock_ram, clock_addr, clock_write, mem_rwn, io_wdata, freeze_act, io_range, freeze_trig, no_freeze, cart_en, clock_port, ata_err, epyx_timeout)
    begin
        serve_enable <= cart_en or kernal_enable;
        -- defaults
        -- 64K, 8K banks, no writes
        mem_ram <= '0';
        mem_addr_i <= '0' & bank_bits & slot_addr(12 downto 0);
        allow_write <= '0';
        io_rdata <= X"FF";
        io_reg_out <= '0';

        serve_empty <= '0';
        irq_n       <= '1';
        nmi_n       <= '1';
        serve_io1 <= '0';
        serve_io2 <= '0';
        game_n    <= '1';
        game_n_or <= '0';
        exrom_n   <= '1';
        serve_rom <= '1';
        case cart_logic_d is
        when c_fc3 =>
            if cart_en='1' or freeze_act='1' then -- cart_en only controls the register access, but otherwise there would be problems with the boot cartridge RUN
                game_n  <= mode_bits(0) and not freeze_act; -- only pulls GAME
                exrom_n <= mode_bits(1); -- Boots in 16K
                nmi_n     <= mode_bits(2) and not (freeze_trig or freeze_act);
            end if;
            serve_io1 <= '1';
            serve_io2 <= '1'; -- ROM visible
            serve_enable <= '1'; -- because of ROM is always in I/O!

            mem_addr_i(13) <= slot_addr(13); -- 16K banks

        when c_retro =>
            if freeze_act = '1' and no_freeze = '0' then
                game_n    <= '0';
                game_n_or <= not phi2_recovered;
                serve_rom <= slot_addr(13); -- only at Exxx
            elsif cart_en='1' then
                game_n    <= not mode_bits(0); -- Boots in 8K
                game_n_or <= not phi2_recovered;
                exrom_n   <= mode_bits(1);
                serve_io1 <= reu_mapping and (not clock_port or slot_addr(7) or slot_addr(6) or slot_addr(5) or slot_addr(4));
                serve_io2 <= not reu_mapping;
            end if;
            irq_n     <= not(freeze_trig or freeze_act) or no_freeze;
            nmi_n     <= not(freeze_trig or freeze_act) or no_freeze;

            if mode_bits(2)='1' and slot_addr(13)='0' and freeze_act='0' then -- RAM in I/O and 8xxx
                mem_ram <= '1';
                mem_addr_i(15) <= '0'; -- 4x8K RAM
                if slot_addr(15 downto 14)="10" then
                    allow_write <= mode_bits(1) and mode_bits(0) and cart_en; -- ultimax only
                elsif io_range then -- I/O
                    if allow_bank='0' then
                        mem_addr_i(14 downto 13) <= "00";
                    end if;
                    if slot_addr(8) = reu_mapping then
                        allow_write <= '0'; -- not where the reu is
                    else
                        allow_write <= cart_en;
                    end if;
                end if;
            end if;

            if rr_range then
                io_reg_out  <= cart_en;
                allow_write <= '0'; -- skip registers
            elsif clock_port='1' and clock_range then
                allow_write <= '0'; -- skip clock port
            end if;

            io_rdata(7) <= bank_bits(15);
            io_rdata(6) <= reu_mapping;  -- reu compatible map
            io_rdata(5) <= '0';          -- flash mode only A16
            io_rdata(4) <= bank_bits(14);
            io_rdata(3) <= bank_bits(13);
            io_rdata(2) <= '0';          -- freeze button pressed
            io_rdata(1) <= allow_bank;
            io_rdata(0) <= '0';          -- flash mode

        when c_action =>
            if freeze_act = '1' then
                game_n    <= '0';
                serve_rom <= slot_addr(13); -- only at Exxx
            elsif cart_en='1' then
                game_n    <= not mode_bits(0); -- Boots in 8K
                exrom_n   <= mode_bits(1);
                serve_io2 <= '1';
            end if;
            irq_n     <= not(freeze_trig or freeze_act);
            nmi_n     <= not(freeze_trig or freeze_act);

            if mode_bits(2)='1' and slot_addr(13)='0' and freeze_act='0' then -- RAM in I/O or 8xxx
                mem_ram <= '1';
                mem_addr_i(15 downto 13) <= "000"; -- 8K RAM
                if (slot_addr(15)='1' and slot_addr(14)='0') or (io_range and slot_addr(8)='1') then
                    allow_write <= cart_en; -- yes, write through if enabled at 8xxx
                end if;
            end if;

        when c_idedos =>
            if freeze_act = '1' and mode_bits(1 downto 0) = "10" then
                game_n    <= '0';
                game_n_or <= not aec_recovered;
                irq_n     <= '0';
                nmi_n     <= '0';
            else
                game_n    <= not cart_en or not mode_bits(0); -- Boots in 8K
                game_n_or <= not aec_recovered and mode_bits(1);
                exrom_n   <= not cart_en or mode_bits(1);
                irq_n     <= not freeze_trig;
                nmi_n     <= not freeze_trig;
            end if;
            serve_empty <= slot_addr(13) or not slot_addr(15);
            serve_io1 <= cart_en and (slot_addr(7) or slot_addr(6) or slot_addr(5));

            mem_addr_i(13) <= slot_addr(13); -- 16K banks

            if slot_addr(15) = '1' then
                if io_range and slot_addr(8) = '0' then -- DExx
                    if slot_addr(7 downto 3) = X"2" & '0' then
                        mem_ram <= '1';
                        mem_addr_i(15 downto 5) <= (others => '0');
                        if slot_addr(2 downto 0) = "110" and mem_rwn = '0' then
                            mem_addr_i(4) <= io_wdata(4);
                        else 
                            mem_addr_i(4) <= ata_select;
                        end if;
                        mem_addr_i(3) <= not slot_addr(2) and not slot_addr(1) and slot_addr(0) and not mem_rwn;
                        if ata_bsy = '0' then
                            allow_write <= '1';
                        end if;
                    end if;
                    case slot_addr(7 downto 0) is
                        when X"20" | X"31" =>
                            mem_ram <= '1';
                            mem_addr_i(15 downto 13) <= "10" & ata_select;
                            if mem_rwn = '1' and slot_addr(0) = '1' then
                                mem_addr_i(12 downto 1) <= std_logic_vector(ata_buffer_pointer2);
                            else
                                mem_addr_i(12 downto 1) <= std_logic_vector(ata_buffer_pointer);
                            end if;
                            if ata_drq = '1' and ata_bsy_i = '0' and ata_bsy = '0' then
                                allow_write <= '1';
                            end if;
                        when X"28" | X"29" | X"2A" | X"2B" | X"2C" | X"2D"  =>
                            io_rdata <= c_ata_z;
                            io_reg_out <= cart_en;
                        when X"27" | X"2E" =>
                            io_rdata <= (ata_bsy_i or ata_bsy) & not ata_drdy & "00" & ata_drq & "00" & ata_err;
                            io_reg_out <= cart_en;
                        when X"2F" =>
                            io_rdata <= c_ata_z(7) & "1" & not ata_head & not ata_select & ata_select;
                            io_reg_out <= cart_en;
                        when X"32" =>
                            io_rdata <= "001" & bank_bits(16 downto 14) & not mode_bits(0) & mode_bits(1);
                            io_reg_out <= cart_en;
                        when X"5F" =>
                            if clock_ram = '1' then
                                mem_ram <= '1';
                                mem_addr_i(15 downto 0) <= X"0" & "1111111" & std_logic_vector(clock_addr(4 downto 0));
                            else
                                null;--mem_addr_i <= g_rtc_base(27 downto 4) & std_logic_vector(clock_addr(3 downto 0));
                            end if;
                            allow_write <= clock_write and clock_ram;
                        when others =>
                            null;
                    end case;
                    if slot_addr(7 downto 4) = X"2" and ata_select = '1' then
                        io_rdata <= c_ata_z;
                        io_reg_out <= cart_en;
                    end if;
                elsif freeze_act = '1' and mode_bits(1 downto 0) = "10" and slot_addr(14 downto 3) = X"FFF" and slot_addr(1) = '1' then
                    mem_addr_i(2 downto 1) <= "10"; -- FFFC
                end if;
            else
                mem_ram <= '1';
                mem_addr_i(15 downto 14) <= '0' & slot_addr(14); -- 32K RAM
                allow_write <= (mode_bits(0) and mode_bits(1)) or freeze_act; -- only in ultimax
            end if;
        
        when c_easy_flash =>
            game_n    <= not (mode_bits(0) or not mode_bits(2));
            exrom_n   <= not mode_bits(1);
            serve_io2 <= '1'; -- RAM

            mem_addr_i(19) <= slot_addr(13);

            if io_range and slot_addr(8) = '1' then -- DFxx
                mem_ram <= '1';
                mem_addr_i(15 downto 8) <= (others => '0'); -- 256B RAM
                allow_write <= '1';
            end if;

        when c_ss5 =>
            if cart_en='1' then
                game_n    <= mode_bits(0);
                game_n_or <= not phi2_recovered;
                exrom_n   <= not mode_bits(1);
                serve_io1 <= '1';
            end if;
            irq_n     <= not(freeze_trig or freeze_act);
            nmi_n     <= not(freeze_trig or freeze_act);

            if (mode_bits(1 downto 0)="00") and (slot_addr(15 downto 13)="100") then
                mem_ram <= '1';
                mem_addr_i(15 downto 13) <= '0' & bank_bits(15 downto 14); -- 32K RAM
                allow_write <= '1';
            else
                mem_addr_i(13) <= slot_addr(13); -- 16K banks
            end if;

        when c_domark =>
            exrom_n   <= mode_bits(0);

            mem_addr_i(19) <= slot_addr(13);

        when c_ocean128 | c_system3 =>
            exrom_n   <= not cart_en;

            mem_addr_i(19) <= slot_addr(13);

        when c_16k_umax =>
            game_n    <= not cart_en;

            mem_addr_i(13) <= slot_addr(13); -- 16K banks

        when c_16k =>
            game_n    <= not cart_en;
            exrom_n   <= not cart_en;

            mem_addr_i(13) <= slot_addr(13); -- 16K banks

        when c_ocean256 =>
            game_n    <= not cart_en;
            exrom_n   <= not cart_en;

            mem_addr_i(19) <= slot_addr(13);

        when c_8k =>
            exrom_n   <= not cart_en;

        when c_epyx =>
            exrom_n   <= epyx_timeout;
            serve_io2 <= '1'; -- rom visible df00-dfff
            serve_enable <= '1'; -- because of ROM is always in I/O!

        when others =>
            null;

        end case;
    end process;

    mem_addr <= unsigned(g_kernal_base(mem_addr'high downto 14) & slot_addr(12 downto 0) & '0') when kernal_area='1' else
                unsigned(g_ram_base(mem_addr'high downto 16) & mem_addr_i(15 downto 0)) when mem_ram='1' else
                unsigned(g_rom_base(mem_addr'high downto 20) & mem_addr_i);

    ata_data <= ata_data_i;
    ata_cmd <= ata_cmd_i;
    ata_rst <= ata_rst_i;

    slot_resp.data <= io_rdata;
    slot_resp.reg_output <= io_reg_out;
    slot_resp.irq <= '0';
end gideon;
