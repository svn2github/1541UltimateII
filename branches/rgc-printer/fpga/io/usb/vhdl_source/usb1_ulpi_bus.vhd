library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity usb1_ulpi_bus is
port (
    clock       : in    std_logic;
    reset       : in    std_logic;
    
    ULPI_DATA   : inout std_logic_vector(7 downto 0);
    ULPI_DIR    : in    std_logic;
    ULPI_NXT    : in    std_logic;
    ULPI_STP    : out   std_logic;
    
    -- status
    status      : out   std_logic_vector(7 downto 0);

    -- register interface
    reg_read    : in    std_logic;
    reg_write   : in    std_logic;
    reg_address : in    std_logic_vector(5 downto 0);
    reg_wdata   : in    std_logic_vector(7 downto 0);
    reg_ack     : out   std_logic;
    
    -- stream interface
    tx_data     : in    std_logic_vector(7 downto 0);
    tx_last     : in    std_logic;
    tx_valid    : in    std_logic;
    tx_start    : in    std_logic;
    tx_next     : out   std_logic;

    rx_data     : out   std_logic_vector(7 downto 0);
    rx_register : out   std_logic;
    rx_last     : out   std_logic;
    rx_valid    : out   std_logic;
    rx_store    : out   std_logic );

    attribute keep_hierarchy : string;
    attribute keep_hierarchy of usb1_ulpi_bus : entity is "yes";

end usb1_ulpi_bus;
    
architecture gideon of usb1_ulpi_bus is
    signal ulpi_data_out    : std_logic_vector(7 downto 0);

    signal ulpi_data_in     : std_logic_vector(7 downto 0);
    signal ulpi_dir_d1      : std_logic;
    signal ulpi_dir_d2      : std_logic;
    signal ulpi_dir_d3      : std_logic;
    signal ulpi_nxt_d1      : std_logic;
    signal ulpi_nxt_d2      : std_logic;
    signal ulpi_nxt_d3      : std_logic;
    signal reg_cmd_d2       : std_logic;
    signal reg_cmd_d3       : std_logic;
    signal reg_cmd_d4       : std_logic;
    signal reg_cmd_d5       : std_logic;
    signal rx_reg_i         : std_logic;
    signal tx_reg_i         : std_logic;
    signal rx_status_i      : std_logic;

    signal ulpi_stop        : std_logic := '1';
    signal ulpi_last        : std_logic;
        
    type t_state is ( idle, reading, writing, writing_data, transmit );
    signal state            : t_state;
    
    attribute iob : string;
    attribute iob of ulpi_data_in  : signal is "true";
    attribute iob of ulpi_dir_d1   : signal is "true";
    attribute iob of ulpi_nxt_d1   : signal is "true";
    attribute iob of ulpi_data_out : signal is "true";
    attribute iob of ULPI_STP      : signal is "true";
    
begin
    -- Marking incoming data based on next/dir pattern
    rx_data      <= ulpi_data_in;
    rx_store     <= ulpi_dir_d1 and ulpi_dir_d2 and ulpi_nxt_d1;
    rx_valid     <= ulpi_dir_d1 and ulpi_dir_d2;
    rx_last      <= not ulpi_dir_d1 and ulpi_dir_d2;
    rx_status_i  <= ulpi_dir_d1 and ulpi_dir_d2 and not ulpi_nxt_d1 and not rx_reg_i;

    rx_reg_i     <= (ulpi_dir_d1 and ulpi_dir_d2 and not ulpi_dir_d3) and
                    (not ulpi_nxt_d1 and not ulpi_nxt_d2 and ulpi_nxt_d3) and
                    reg_cmd_d5;

    rx_register  <= rx_reg_i;
    reg_ack      <= rx_reg_i or tx_reg_i;
    
    p_sample: process(clock, reset)
    begin
        if rising_edge(clock) then
            ulpi_data_in <= ULPI_DATA;

            reg_cmd_d2   <= ulpi_data_in(7) and ulpi_data_in(6);
            reg_cmd_d3   <= reg_cmd_d2;
            reg_cmd_d4   <= reg_cmd_d3;
            reg_cmd_d5   <= reg_cmd_d4;

            ulpi_dir_d1  <= ULPI_DIR;
            ulpi_dir_d2  <= ulpi_dir_d1;
            ulpi_dir_d3  <= ulpi_dir_d2;

            ulpi_nxt_d1  <= ULPI_NXT;
            ulpi_nxt_d2  <= ulpi_nxt_d1;
            ulpi_nxt_d3  <= ulpi_nxt_d2;

            if rx_status_i='1' then
                status <= ulpi_data_in;
            end if;

            if reset='1' then
                status <= (others => '0');
            end if;
        end if;
    end process;

    p_tx_state: process(clock, reset)
    begin
        if rising_edge(clock) then
            ulpi_stop    <= '0';
            tx_reg_i     <= '0';

            case state is

            when idle =>
                ulpi_data_out <= X"00";
                if reg_read='1' and rx_reg_i='0' then
                    ulpi_data_out <= "11" & reg_address;
                    state <= reading;
                elsif reg_write='1' and tx_reg_i='0' then
                    ulpi_data_out <= "10" & reg_address;
                    state <= writing;
                elsif tx_valid = '1' and tx_start = '1' and ULPI_DIR='0' then
                    ulpi_data_out <= tx_data;
                    ulpi_last     <= tx_last;
                    state         <= transmit;
                end if;

            when reading =>
                if rx_reg_i='1' then
                    ulpi_data_out <= X"00";
                    state <= idle;
                end if;
                if ulpi_dir_d1='1' then
                    state <= idle; -- terminate current tx
                    ulpi_data_out <= X"00";
                end if;

            when writing =>
                if ULPI_NXT='1' then
                    ulpi_data_out <= reg_wdata;
                    state <= writing_data;
                end if;
                if ulpi_dir_d1='1' then
                    state <= idle; -- terminate current tx
                    ulpi_data_out <= X"00";
                end if;
            
            when writing_data =>
                if ULPI_NXT='1' and ULPI_DIR='0' then
                    tx_reg_i <= '1';
                    ulpi_stop <= '1';
                    state <= idle;
                end if;
                if ulpi_dir_d1='1' then
                    state <= idle; -- terminate current tx
                    ulpi_data_out <= X"00";
                end if;
                
            when transmit =>
                if ULPI_NXT = '1' then
                    if ulpi_last='1' or tx_valid = '0' then
                        ulpi_data_out <= X"00";
                        ulpi_stop     <= '1';
                        state         <= idle;
                    else
                        ulpi_data_out <= tx_data;
                        ulpi_last     <= tx_last;
                    end if;
                end if;
            
            when others =>
                null;

            end case;

            if reset='1' then
                state     <= idle;
                ulpi_stop <= '0';
                ulpi_last <= '0';
            end if;
        end if;
    end process;

    p_next: process(state, tx_valid, tx_start, rx_reg_i, tx_reg_i, ULPI_DIR, ULPI_NXT, ulpi_last, reg_read, reg_write)
    begin
        case state is
        when idle =>
            tx_next <= not ULPI_DIR and tx_valid and tx_start;
            if reg_read='1' and rx_reg_i='0' then
                tx_next <= '0';
            end if;
            if reg_write='1' and tx_reg_i='0' then
                tx_next <= '0';
            end if;

        when transmit =>
            tx_next <= ULPI_NXT and tx_valid and not ulpi_last;
        
        when others =>
            tx_next <= '0';
        
        end case;            
    end process;

    ULPI_STP   <= ulpi_stop;
    ULPI_DATA  <= ulpi_data_out when ULPI_DIR='0' and ulpi_dir_d1='0' else (others => 'Z');
    
end gideon;    
    