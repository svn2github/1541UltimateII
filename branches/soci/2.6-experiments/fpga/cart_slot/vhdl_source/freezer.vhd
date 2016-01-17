library ieee;
use ieee.std_logic_1164.all;

entity freezer is
port (
    clock           : in  std_logic;
    reset           : in  std_logic;
    
    RST_in          : in  std_logic;
    button_freeze   : in  std_logic;
    
    cpu_cycle_done  : in  std_logic;
    cpu_write       : in  std_logic;
    
    freezer_state   : out std_logic_vector(1 downto 0); -- debug

    unfreeze        : in  std_logic; -- could be software driven, or automatic, depending on cartridge
    freeze_trig     : out std_logic;
    freeze_act      : out std_logic );

end freezer;

architecture gideon of freezer is
    signal wr_cnt      : integer range 0 to 3;

    type t_state is (idle, triggered, enter_freeze, button);
    signal state : t_state;

begin
    freeze_trig <= '1' when (state = triggered) else '0';
    freeze_act <= '1' when (state = enter_freeze) else '0';

    process(clock)
    begin
        if rising_edge(clock) then
            if reset='1' or RST_in='1' then -- reset
                state <= button;
            end if; 

            if cpu_cycle_done = '1' then
                if (cpu_write = '1') and (state = triggered) then
                    wr_cnt <= wr_cnt + 1;
                else
                    wr_cnt <= 0;
                end if;
            end if;

            case state is
            when idle =>
                if button_freeze = '1' then -- wait for push
                    state <= triggered;
                end if;

            when triggered =>
                if unfreeze = '1' then -- abort, or ignore
                    state <= button;
                elsif wr_cnt = 3 then -- wait for 3 writes
                    state <= enter_freeze;
                end if;
            
            when enter_freeze =>
                if unfreeze = '1' then -- wait for unfreeze
                    state <= button;
                end if;

            when button =>
                if button_freeze = '0' then -- wait until button is not pressed anymore
                    state <= idle;
                end if;
                
            when others =>
                state <= button;
            end case;
            
        end if;
    end process;
   
    with state select freezer_state <=
        "00" when idle,
        "01" when triggered,
        "10" when enter_freeze,
        "11" when button,
        "00" when others;

end gideon;
