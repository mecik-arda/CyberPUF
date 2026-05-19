library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.aes_pkg.ALL;

entity aes256_decryption is
    port (
        clk             : in  std_logic;
        rst             : in  std_logic;
        ciphertext      : in  std_logic_vector(127 downto 0);
        round_keys      : in  round_key_array_t;
        start           : in  std_logic;
        plaintext       : out std_logic_vector(127 downto 0);
        done            : out std_logic;
        busy            : out std_logic
    );
end entity aes256_decryption;

architecture rtl of aes256_decryption is

    type state_t is (
        IDLE,
        INIT_ADD_KEY,
        INV_SHIFT_ROWS,
        INV_SUB_BYTES,
        INV_ADD_KEY,
        INV_MIX_COLUMNS,
        FINAL_INV_SHIFT_ROWS,
        FINAL_INV_SUB_BYTES,
        FINAL_ADD_KEY,
        FINISHED
    );
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
            plaintext <= (others => '0');
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
                        aes_state <= vector_to_state(ciphertext);
                        round_num <= to_unsigned(14, 4);
                        busy <= '1';
                        fsm_state <= INIT_ADD_KEY;
                    end if;

                when INIT_ADD_KEY =>
                    rk_vec := round_keys(14);
                    rk_state := vector_to_state(rk_vec);
                    for r in 0 to 3 loop
                        for c in 0 to 3 loop
                            aes_state(r, c) <= aes_state(r, c) xor rk_state(r, c);
                        end loop;
                    end loop;
                    round_num <= to_unsigned(13, 4);
                    fsm_state <= INV_SHIFT_ROWS;

                when INV_SHIFT_ROWS =>
                    temp_state := aes_state;

                    aes_state(1, 0) <= temp_state(1, 3);
                    aes_state(1, 1) <= temp_state(1, 0);
                    aes_state(1, 2) <= temp_state(1, 1);
                    aes_state(1, 3) <= temp_state(1, 2);

                    aes_state(2, 0) <= temp_state(2, 2);
                    aes_state(2, 1) <= temp_state(2, 3);
                    aes_state(2, 2) <= temp_state(2, 0);
                    aes_state(2, 3) <= temp_state(2, 1);

                    aes_state(3, 0) <= temp_state(3, 1);
                    aes_state(3, 1) <= temp_state(3, 2);
                    aes_state(3, 2) <= temp_state(3, 3);
                    aes_state(3, 3) <= temp_state(3, 0);

                    fsm_state <= INV_SUB_BYTES;

                when INV_SUB_BYTES =>
                    for r in 0 to 3 loop
                        for c in 0 to 3 loop
                            aes_state(r, c) <= inv_sub_byte(aes_state(r, c));
                        end loop;
                    end loop;
                    fsm_state <= INV_ADD_KEY;

                when INV_ADD_KEY =>
                    rk_vec := round_keys(to_integer(round_num));
                    rk_state := vector_to_state(rk_vec);
                    for r in 0 to 3 loop
                        for c in 0 to 3 loop
                            aes_state(r, c) <= aes_state(r, c) xor rk_state(r, c);
                        end loop;
                    end loop;

                    if round_num = to_unsigned(0, 4) then
                        fsm_state <= FINISHED;
                    else
                        fsm_state <= INV_MIX_COLUMNS;
                    end if;

                when INV_MIX_COLUMNS =>
                    for c in 0 to 3 loop
                        t0 := aes_state(0, c);
                        t1 := aes_state(1, c);
                        t2 := aes_state(2, c);
                        t3 := aes_state(3, c);

                        aes_state(0, c) <= gf_mult(t0, 14) xor gf_mult(t1, 11) xor gf_mult(t2, 13) xor gf_mult(t3, 9);
                        aes_state(1, c) <= gf_mult(t0, 9)  xor gf_mult(t1, 14) xor gf_mult(t2, 11) xor gf_mult(t3, 13);
                        aes_state(2, c) <= gf_mult(t0, 13) xor gf_mult(t1, 9)  xor gf_mult(t2, 14) xor gf_mult(t3, 11);
                        aes_state(3, c) <= gf_mult(t0, 11) xor gf_mult(t1, 13) xor gf_mult(t2, 9)  xor gf_mult(t3, 14);
                    end loop;

                    round_num <= round_num - 1;
                    fsm_state <= INV_SHIFT_ROWS;

                when FINAL_INV_SHIFT_ROWS =>
                    fsm_state <= IDLE;

                when FINAL_INV_SUB_BYTES =>
                    fsm_state <= IDLE;

                when FINAL_ADD_KEY =>
                    fsm_state <= IDLE;

                when FINISHED =>
                    plaintext <= state_to_vector(aes_state);
                    done <= '1';
                    busy <= '0';
                    fsm_state <= IDLE;

                when others =>
                    fsm_state <= IDLE;
            end case;
        end if;
    end process;

end architecture rtl;
