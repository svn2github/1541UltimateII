library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.slot_bus_pkg.all;

entity slot_slave is
port (
    clock           : in  std_logic;
    reset           : in  std_logic;
    
    -- Cartridge pins
    RSTn            : in  std_logic;
    IO1n            : in  std_logic;
    IO2n            : in  std_logic;
    ROMLn           : in  std_logic;
    ROMHn           : in  std_logic;
    GAMEn           : in  std_logic;
    EXROMn          : in  std_logic;
    RWn             : in  std_logic;
    ADDRESS         : in  std_logic_vector(15 downto 0);
    DATA_in         : in  std_logic_vector(7 downto 0);
    DATA_out        : out std_logic_vector(7 downto 0) := (others => '0');
    DATA_tri        : out std_logic;
    
    -- interface with memory controller
    mem_req         : out std_logic; -- our memory request to serve slot
    mem_size        : out unsigned(1 downto 0);
    mem_rwn         : out std_logic;
    mem_rack        : in  std_logic;
    mem_dack        : in  std_logic;
    mem_count       : in  unsigned(1 downto 0);
    mem_rdata       : in  std_logic_vector(7 downto 0);
    mem_wdata       : out std_logic_vector(7 downto 0);
    -- mem_addr comes from cartridge logic

    reset_out       : out std_logic;
    
    -- timing inputs
    aec_recovered   : in  std_logic;
    phi2_tick       : in  std_logic;
    do_sample_addr  : in  std_logic;
    do_probe_end    : in  std_logic;
    do_sample_io    : in  std_logic;
    do_io_event     : in  std_logic;
    serve_vic       : out std_logic;

    -- interface with freezer (cartridge) logic
    allow_serve     : in  std_logic := '0'; -- from timing unit (modified version of serve_enable)
    serve_rom       : in  std_logic := '0'; -- ROML or ROMH
    serve_empty     : in  std_logic := '0'; -- Ultimax empty
    serve_io1       : in  std_logic := '0'; -- IO1n
    serve_io2       : in  std_logic := '0'; -- IO2n
    allow_write     : in  std_logic := '0';
    kernal_enable   : in  std_logic := '0';
    kernal_probe    : out std_logic := '0';
    kernal_area     : out std_logic := '0';
    force_ultimax   : out std_logic := '0';

    epyx_timeout    : out std_logic; -- '0' => epyx is on, '1' epyx is off    
    cpu_write       : out std_logic; -- for freezer

    slot_req        : out t_slot_req;
    slot_resp       : in  t_slot_resp;

    -- interface with hardware
    BUFFER_ENn      : out std_logic );

end slot_slave;    

architecture gideon of slot_slave is
    signal address_c    : std_logic_vector(15 downto 0) := (others => '0');
    signal data_c       : std_logic_vector(7 downto 0) := X"FF";
    signal io1n_c       : std_logic := '1';
    signal io2n_c       : std_logic := '1';
    signal rwn_c        : std_logic := '1';
    signal romhn_c      : std_logic := '1';
    signal romln_c      : std_logic := '1';
    signal aec_c        : std_logic := '0';
    signal dav          : std_logic := '0';
    signal addr_is_io   : std_logic;
    signal addr_is_kernal : std_logic;
    signal addr_is_empty : std_logic;
    signal mem_req_i    : std_logic;
    signal mem_size_i   : unsigned(1 downto 0);
    signal servicable_cpu : std_logic;
    signal servicable_vic : std_logic;
    signal io_read_cond : std_logic;
    signal io_write_cond: std_logic;
    signal late_write_cond  : std_logic;
    signal ultimax      : std_logic;
    signal ultimax_d    : std_logic := '0';
    signal ultimax_d2   : std_logic := '0';
    signal mem_wdata_i  : std_logic_vector(7 downto 0);
    signal kernal_probe_i   : std_logic;
    signal kernal_area_i    : std_logic;
    signal mem_data_0       : std_logic_vector(7 downto 0) := X"00";
    signal mem_data_1       : std_logic_vector(7 downto 0) := X"00";
    signal data_mux         : std_logic;
    
    attribute register_duplication : string;
    attribute register_duplication of rwn_c     : signal is "no";
    attribute register_duplication of io1n_c    : signal is "no";
    attribute register_duplication of io2n_c    : signal is "no";
    attribute register_duplication of romln_c   : signal is "no";
    attribute register_duplication of romhn_c   : signal is "no";
    attribute register_duplication of reset_out : signal is "no";

    type   t_state is (idle, mem_access, wait_end, reg_out);
                       
    attribute iob : string;
    attribute iob of data_c : signal is "true"; 

    signal state     : t_state;
    
--    attribute fsm_encoding : string;
--    attribute fsm_encoding of state : signal is "sequential";

    signal epyx_timer       : unsigned(6 downto 0) := (others => '0');
    signal epyx_reset       : std_logic := '0';
begin
    slot_req.io_write      <= do_io_event and io_write_cond;
    slot_req.io_read       <= do_io_event and io_read_cond;
    slot_req.late_write    <= do_io_event and late_write_cond;
    slot_req.io_read_early <= addr_is_io and aec_c and rwn_c and do_sample_addr;

    process(clock)
    begin
        if rising_edge(clock) then
            -- synchronization
            if mem_req_i='0' then -- don't change while an access is taking place
                rwn_c     <= RWn;
                address_c <= ADDRESS;
            end if;
            reset_out <= reset or not RSTn;
            aec_c     <= aec_recovered;
            io1n_c    <= IO1n;
            io2n_c    <= IO2n;
            romln_c   <= ROMLn;
            romhn_c   <= ROMHn;
            data_c    <= DATA_in;
            ultimax   <= not GAMEn and EXROMn;
            ultimax_d <= ultimax;
            ultimax_d2 <= ultimax_d;
            
            if epyx_reset='1' then
                epyx_timer <= (others => '1');
                epyx_timeout <= '0';
            elsif phi2_tick='1' then
                if epyx_timer = "0000000" then
                    epyx_timeout <= '1';
                else
                    epyx_timer <= epyx_timer - 1;
                end if;
            end if;

            slot_req.bus_write <= '0';
            if do_sample_io='1' then
                cpu_write  <= not RWn;

                slot_req.bus_write  <= not RWn;
                slot_req.io_address <= unsigned(address_c);
                mem_wdata_i         <= data_c;

                late_write_cond <= not rwn_c;
                io_write_cond <= not rwn_c and (not io2n_c or not io1n_c);
                io_read_cond  <= rwn_c and (not io2n_c or not io1n_c);
                epyx_reset    <= not io2n_c or not io1n_c or not romln_c or not RSTn;
            end if;

            if do_probe_end='1' then
                data_mux <= kernal_probe_i and not romhn_c;
                force_ultimax <= kernal_probe_i;
                kernal_probe_i <= '0';
            elsif do_io_event='1' then
                force_ultimax <= '0';
            end if;

            case state is
            when idle =>
                mem_size_i    <= "00";
                dav           <= '0';
                kernal_area_i <= '0';
                if rwn_c='1' then -- read
                    if do_sample_addr='1' then
                        if slot_resp.reg_output='1' and addr_is_io='1' and aec_c='1' then -- read register
                            state <= reg_out; -- register output
                        elsif allow_serve='1' then
                            if addr_is_kernal='1' and aec_c='1' then -- kernal test
                                kernal_probe_i <= '1';
                                kernal_area_i  <= '1';
                                mem_size_i <= "01";
                                state <= mem_access;
                            elsif (servicable_cpu='1' and aec_c='1') or (servicable_vic='1' and aec_c='0') then
                                state <= mem_access; -- memory read
                            end if;
                        end if;
                    end if;
                else -- write
                    if do_sample_io='1' then
                        if addr_is_kernal='1' then
                        --  do mirror to kernal write address
                            state <= mem_access;
                            kernal_area_i <= '1';
                        elsif allow_write='1' then -- yes, writes-through always if enabled (see AR), except for I/O
                            if addr_is_io='0' or (serve_io1='1' and io1n_c='0') or (serve_io2='1' and io2n_c='0') then
                            -- memory write
                                state <= mem_access;
                            end if;
                        end if;
                    end if;
                end if;
                            
            when mem_access =>
                if mem_rack='1' then
                    if rwn_c='0' then  -- if write, we're done.
                        state <= idle;
                    else -- if read, then we need to wait for the data
                        state <= wait_end;
                    end if;
                end if;

            when wait_end =>
				if mem_dack='1' then -- the data is available, put it on the bus!
                    if mem_count="00" then
                        mem_data_0 <= mem_rdata;
                    else
                        mem_data_1 <= mem_rdata;
                    end if;
                    dav <= '1';
				end if;
                if phi2_tick='1' or do_io_event='1' then -- around the clock edges
                    state <= idle;
                end if;

            when reg_out =>
                dav <= '1';
                mem_data_0 <= slot_resp.data;

                if phi2_tick='1' or do_io_event='1' then -- around the clock edges
                    state <= idle;
                end if;
                
            when others =>
                state <= idle;

            end case;

            if kernal_area_i='1' then
                DATA_tri <= rwn_c and not romhn_c and ultimax_d2;
            else -- whenever it's requested by ROMLn/ROMHn/IO1n/IO2n or when it's empty for the CPU (VIC will use ROMHn)
                DATA_tri <= rwn_c and (mem_dack or dav) and (not romln_c or not romhn_c or not io1n_c or not io2n_c or (addr_is_empty and aec_c));
            end if;

            if reset='1' then
                data_mux        <= '0';
                dav             <= '0';
                state           <= idle;
                mem_size_i      <= "00";
                io_read_cond    <= '0';
                io_write_cond   <= '0';
                late_write_cond <= '0';
                slot_req.io_address <= (others => '0');
                cpu_write       <= '0';
                epyx_reset      <= '1';
                kernal_probe_i  <= '0';
                kernal_area_i   <= '0';
                force_ultimax   <= '0';
            end if;
        end if;
    end process;
    
    -- predicting the future, and more
    process(address_c, serve_rom, serve_empty, serve_io1, serve_io2, kernal_enable, GAMEn, EXROMn)
    begin
        addr_is_kernal <= '0';
        addr_is_io     <= '0';
        addr_is_empty  <= '0';
        servicable_vic <= not GAMEn and EXROMn; -- ultimax, only (11 downto 0) valid therefore always...
        case address_c(15 downto 12) is
            when X"1" | X"2" | X"3" | X"4" | X"5" | X"6" | X"7" | X"C" => -- ultimax empty areas
                addr_is_empty <= not GAMEn and EXROMn;
                servicable_cpu <= not GAMEn and EXROMn and serve_empty;
            when X"8" | X"9" => -- ROML
                servicable_cpu <= not (GAMEn and EXROMn) and serve_rom;
            when X"A" | X"B" => -- ROMH
                addr_is_empty <= not GAMEn and EXROMn;
                servicable_cpu <= (not GAMEn and not EXROMn and serve_rom) or
                                  (not GAMEn and EXROMn and serve_empty);
            when X"D" =>        -- IO
                if address_c(11 downto 9)="111" then -- DE/DF
                    if address_c(8) = '0' then 
                        servicable_cpu <= serve_io1;
                    else
                        servicable_cpu <= serve_io2;
                    end if;
                    addr_is_io <= '1';
                else
                    servicable_cpu <= '0';
                end if;
            when X"E" | X"F" => -- ROMH
                addr_is_kernal <= (GAMEn or not EXROMn) and kernal_enable;
                servicable_cpu <= not GAMEn and EXROMn and serve_rom;
            when others =>      -- RAM
                servicable_cpu <= '0';
        end case;
    end process;

    serve_vic  <= servicable_vic;

    mem_req_i  <= '1' when state = mem_access else '0';
    mem_req    <= mem_req_i;
    mem_rwn    <= rwn_c;
    mem_wdata  <= mem_wdata_i;
    mem_size   <= mem_size_i;
        
    BUFFER_ENn <= '0';

    DATA_out <= mem_data_0 when data_mux='0' else mem_data_1;
                                
    slot_req.data        <= mem_wdata_i;
    slot_req.bus_address <= unsigned(address_c(15 downto 0));

    kernal_probe <= kernal_probe_i;
    kernal_area  <= kernal_area_i;
end gideon;
