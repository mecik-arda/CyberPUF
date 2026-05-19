library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.aes_pkg.ALL;

entity aes256_cipher is
    port (
        clk             : in  std_logic;
        rst             : in  std_logic;
        plaintext       : in  std_logic_vector(127 downto 0);
        round_keys      : in  round_key_array_t;
        start           : in  std_logic;
        ciphertext      : out std_logic_vector(127 downto 0);
        done            : out std_logic;
        busy            : out std_logic
    );
end entity aes256_cipher;

architecture rtl of aes256_cipher is

    type state_t is (IDLE, INIT_ADD_KEY, ROUND_SUB_BYTES, ROUND_SHIFT_ROWS, ROUND_MIX_COLUMNS, ROUND_ADD_KEY, FINAL_SUB_BYTES, FINAL_SHIFT_ROWS, FINAL_ADD_KEY, FINISHED);
    signal fsm_state : state_t;

    signal aes_state : state_array_t;
    signal round_num : unsigned(3 downto 0);

begin

    process(clk, rst)
        variable temp_state : state_array_t;
        variable t0, t1, t2, t3 : std_logic_vector(7 downto 0);
        variable rk_vec : std_logic_vector(127 downto 0);
        variable rk_state : state_array_t;
    begin
        if rst = '1' then
            fsm_state <= IDLE;
            done <= '0';
            busy <= '0';
            round_num <= (others => '0');
            ciphertext <= (others => '0');
            for r in 0 to 3 loop
                for c in 0 to 3 loop
                    aes_state(r, c) <= (others => '0');
                end loop;
            end loop;
        elsif rising_edge(clk) then
            done <= '0';

            case fsm_state is
                when IDLE =>
                    busy <= '0';
                    if start = '1' then
                        aes_state <= vector_to_state(plaintext);
                        round_num <= (others => '0');
                        busy <= '1';
                        fsm_state <= INIT_ADD_KEY;
                    end if;

                when INIT_ADD_KEY =>
                    rk_vec := round_keys(0);
                    rk_state := vector_to_state(rk_vec);
                    for r in 0 to 3 loop
                        for c in 0 to 3 loop
                            aes_state(r, c) <= aes_state(r, c) xor rk_state(r, c);
                        end loop;
                    end loop;
                    round_num <= to_unsigned(1, 4);
                    fsm_state <= ROUND_SUB_BYTES;

                when ROUND_SUB_BYTES =>
                    for r in 0 to 3 loop
                        for c in 0 to 3 loop
                            aes_state(r, c) <= sub_byte(aes_state(r, c));
                        end loop;
                    end loop;
                    fsm_state <= ROUND_SHIFT_ROWS;

                when ROUND_SHIFT_ROWS =>
                    temp_state := aes_state;

                    aes_state(1, 0) <= temp_state(1, 1);
                    aes_state(1, 1) <= temp_state(1, 2);
                    aes_state(1, 2) <= temp_state(1, 3);
                    aes_state(1, 3) <= temp_state(1, 0);

                    aes_state(2, 0) <= temp_state(2, 2);
                    aes_state(2, 1) <= temp_state(2, 3);
                    aes_state(2, 2) <= temp_state(2, 0);
                    aes_state(2, 3) <= temp_state(2, 1);

                    aes_state(3, 0) <= temp_state(3, 3);
                    aes_state(3, 1) <= temp_state(3, 0);
                    aes_state(3, 2) <= temp_state(3, 1);
                    aes_state(3, 3) <= temp_state(3, 2);

                    if round_num = to_unsigned(14, 4) then
                        fsm_state <= FINAL_ADD_KEY;
                    else
                        fsm_state <= ROUND_MIX_COLUMNS;
                    end if;

                when ROUND_MIX_COLUMNS =>
                    for c in 0 to 3 loop
                        t0 := aes_state(0, c);
                        t1 := aes_state(1, c);
                        t2 := aes_state(2, c);
                        t3 := aes_state(3, c);

                        aes_state(0, c) <= xtime(t0) xor (xtime(t1) xor t1) xor t2 xor t3;
                        aes_state(1, c) <= t0 xor xtime(t1) xor (xtime(t2) xor t2) xor t3;
                        aes_state(2, c) <= t0 xor t1 xor xtime(t2) xor (xtime(t3) xor t3);
                        aes_state(3, c) <= (xtime(t0) xor t0) xor t1 xor t2 xor xtime(t3);
                    end loop;
                    fsm_state <= ROUND_ADD_KEY;

                when ROUND_ADD_KEY =>
                    rk_vec := round_keys(to_integer(round_num));
                    rk_state := vector_to_state(rk_vec);
                    for r in 0 to 3 loop
                        for c in 0 to 3 loop
                            aes_state(r, c) <= aes_state(r, c) xor rk_state(r, c);
                        end loop;
                    end loop;
                    round_num <= round_num + 1;
                    fsm_state <= ROUND_SUB_BYTES;

                when FINAL_ADD_KEY =>
                    rk_vec := round_keys(14);
                    rk_state := vector_to_state(rk_vec);
                    for r in 0 to 3 loop
                        for c in 0 to 3 loop
                            aes_state(r, c) <= aes_state(r, c) xor rk_state(r, c);
                        end loop;
                    end loop;
                    fsm_state <= FINISHED;

                when FINAL_SUB_BYTES =>
                    fsm_state <= IDLE;

                when FINAL_SHIFT_ROWS =>
                    fsm_state <= IDLE;

                when FINISHED =>
                    ciphertext <= state_to_vector(aes_state);
                    done <= '1';
                    busy <= '0';
                    fsm_state <= IDLE;

                when others =>
                    fsm_state <= IDLE;
            end case;
        end if;
    end process;

end architecture rtl;
