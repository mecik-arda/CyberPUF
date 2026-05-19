library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.aes_pkg.ALL;

entity aes256_key_expansion is
    port (
        clk             : in  std_logic;
        rst             : in  std_logic;
        key_in          : in  std_logic_vector(255 downto 0);
        start           : in  std_logic;
        round_keys      : out round_key_array_t;
        done            : out std_logic;
        busy            : out std_logic
    );
end entity aes256_key_expansion;

architecture rtl of aes256_key_expansion is

    type state_t is (IDLE, LOAD_KEY, EXPAND, FINISHED);
    signal state : state_t;

    signal w : word_array_t(0 to 59);
    signal round_idx : unsigned(5 downto 0);

begin

    process(clk, rst)
        variable temp_word : std_logic_vector(31 downto 0);
        variable rcon_val  : std_logic_vector(31 downto 0);
        variable idx       : integer;
    begin
        if rst = '1' then
            state <= IDLE;
            done <= '0';
            busy <= '0';
            round_idx <= (others => '0');
            for i in 0 to 59 loop
                w(i) <= (others => '0');
            end loop;
            for i in 0 to 14 loop
                round_keys(i) <= (others => '0');
            end loop;
        elsif rising_edge(clk) then
            done <= '0';

            case state is
                when IDLE =>
                    busy <= '0';
                    if start = '1' then
                        state <= LOAD_KEY;
                        busy <= '1';
                    end if;

                when LOAD_KEY =>
                    w(0) <= key_in(255 downto 224);
                    w(1) <= key_in(223 downto 192);
                    w(2) <= key_in(191 downto 160);
                    w(3) <= key_in(159 downto 128);
                    w(4) <= key_in(127 downto 96);
                    w(5) <= key_in(95 downto 64);
                    w(6) <= key_in(63 downto 32);
                    w(7) <= key_in(31 downto 0);
                    round_idx <= to_unsigned(8, 6);
                    state <= EXPAND;

                when EXPAND =>
                    idx := to_integer(round_idx);

                    if idx <= 59 then
                        temp_word := w(idx - 1);

                        if (idx mod 8) = 0 then
                            temp_word := rot_word(temp_word);
                            temp_word := sub_word(temp_word);
                            rcon_val := RCON(idx / 8) & x"000000";
                            temp_word := temp_word xor rcon_val;
                        elsif (idx mod 8) = 4 then
                            temp_word := sub_word(temp_word);
                        end if;

                        w(idx) <= w(idx - 8) xor temp_word;
                        round_idx <= round_idx + 1;
                    else
                        state <= FINISHED;
                    end if;

                when FINISHED =>
                    for rk in 0 to 14 loop
                        round_keys(rk) <= w(rk * 4) & w(rk * 4 + 1) & w(rk * 4 + 2) & w(rk * 4 + 3);
                    end loop;
                    done <= '1';
                    busy <= '0';
                    state <= IDLE;

                when others =>
                    state <= IDLE;
            end case;
        end if;
    end process;

end architecture rtl;
