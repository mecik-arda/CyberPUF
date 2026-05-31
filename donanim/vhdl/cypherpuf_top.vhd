library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.aes_pkg.ALL;

entity cypherpuf_top is
    generic (
        NUM_RO_PAIRS     : integer := 16;
        NUM_INVERTERS    : integer := 3;
        COUNTER_WIDTH    : integer := 20;
        COUNT_CYCLES     : integer := 1000;
        PUF_REPETITIONS  : integer := 16
    );
    port (
        clk                 : in  std_logic;
        rst                 : in  std_logic;

        cmd_generate_key    : in  std_logic;
        cmd_start_decrypt   : in  std_logic;

        data_in             : in  std_logic_vector(127 downto 0);
        data_out            : out std_logic_vector(127 downto 0);

        status_puf_busy     : out std_logic;
        status_puf_done     : out std_logic;
        status_key_exp_busy : out std_logic;
        status_key_exp_done : out std_logic;
        status_aes_busy     : out std_logic;
        status_aes_done     : out std_logic;

        debug_puf_key       : out std_logic_vector(255 downto 0);
        debug_bit_index     : out std_logic_vector(8 downto 0);
        debug_count_a       : out std_logic_vector(COUNTER_WIDTH - 1 downto 0);
        debug_count_b       : out std_logic_vector(COUNTER_WIDTH - 1 downto 0)
    );
end entity cypherpuf_top;

architecture rtl of cypherpuf_top is

    type system_state_t is (
        SYS_IDLE,
        SYS_PUF_GENERATING,
        SYS_PUF_DONE,
        SYS_KEY_EXPANDING,
        SYS_KEY_DONE,
        SYS_READY,
        SYS_DECRYPTING,
        SYS_DECRYPT_DONE
    );
    signal sys_state : system_state_t;

    signal puf_generate     : std_logic;
    signal puf_key          : std_logic_vector(255 downto 0);
    signal puf_key_valid    : std_logic;
    signal puf_busy         : std_logic;
    signal puf_bit_index    : std_logic_vector(8 downto 0);
    signal puf_dbg_count_a  : std_logic_vector(COUNTER_WIDTH - 1 downto 0);
    signal puf_dbg_count_b  : std_logic_vector(COUNTER_WIDTH - 1 downto 0);

    signal key_exp_start    : std_logic;
    signal key_exp_key_in   : std_logic_vector(255 downto 0);
    signal key_exp_round_keys : round_key_array_t;
    signal key_exp_done     : std_logic;
    signal key_exp_busy     : std_logic;

    signal aes_start        : std_logic;
    signal aes_ciphertext   : std_logic_vector(127 downto 0);
    signal aes_plaintext    : std_logic_vector(127 downto 0);
    signal aes_done         : std_logic;
    signal aes_busy         : std_logic;

    signal round_keys_reg   : round_key_array_t;
    signal puf_key_reg      : std_logic_vector(255 downto 0);
    signal keys_ready       : std_logic;

    component puf_key_generator is
        generic (
            KEY_WIDTH        : integer := 256;
            NUM_RO_PAIRS     : integer := 16;
            NUM_INVERTERS    : integer := 3;
            COUNTER_WIDTH    : integer := 20;
            COUNT_CYCLES     : integer := 1000;
            REPETITIONS      : integer := 16
        );
        port (
            clk             : in  std_logic;
            rst             : in  std_logic;
            generate_key    : in  std_logic;
            puf_key         : out std_logic_vector(255 downto 0);
            key_valid       : out std_logic;
            busy            : out std_logic;
            bit_index_out   : out std_logic_vector(8 downto 0);
            debug_count_a   : out std_logic_vector(COUNTER_WIDTH - 1 downto 0);
            debug_count_b   : out std_logic_vector(COUNTER_WIDTH - 1 downto 0)
        );
    end component;

    component aes256_key_expansion is
        port (
            clk             : in  std_logic;
            rst             : in  std_logic;
            key_in          : in  std_logic_vector(255 downto 0);
            start           : in  std_logic;
            round_keys      : out round_key_array_t;
            done            : out std_logic;
            busy            : out std_logic
        );
    end component;

    component aes256_decryption is
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
    end component;

begin

    puf_gen_inst: puf_key_generator
        generic map (
            KEY_WIDTH       => 256,
            NUM_RO_PAIRS    => NUM_RO_PAIRS,
            NUM_INVERTERS   => NUM_INVERTERS,
            COUNTER_WIDTH   => COUNTER_WIDTH,
            COUNT_CYCLES    => COUNT_CYCLES,
            REPETITIONS     => PUF_REPETITIONS
        )
        port map (
            clk             => clk,
            rst             => rst,
            generate_key    => puf_generate,
            puf_key         => puf_key,
            key_valid       => puf_key_valid,
            busy            => puf_busy,
            bit_index_out   => puf_bit_index,
            debug_count_a   => puf_dbg_count_a,
            debug_count_b   => puf_dbg_count_b
        );

    key_exp_inst: aes256_key_expansion
        port map (
            clk             => clk,
            rst             => rst,
            key_in          => key_exp_key_in,
            start           => key_exp_start,
            round_keys      => key_exp_round_keys,
            done            => key_exp_done,
            busy            => key_exp_busy
        );

    aes_dec_inst: aes256_decryption
        port map (
            clk             => clk,
            rst             => rst,
            ciphertext      => aes_ciphertext,
            round_keys      => round_keys_reg,
            start           => aes_start,
            plaintext       => aes_plaintext,
            done            => aes_done,
            busy            => aes_busy
        );

    debug_puf_key <= puf_key_reg;
    debug_bit_index <= puf_bit_index;
    debug_count_a <= puf_dbg_count_a;
    debug_count_b <= puf_dbg_count_b;

    process(clk, rst)
    begin
        if rst = '1' then
            sys_state <= SYS_IDLE;
            puf_generate <= '0';
            key_exp_start <= '0';
            key_exp_key_in <= (others => '0');
            aes_start <= '0';
            aes_ciphertext <= (others => '0');
            data_out <= (others => '0');
            status_puf_busy <= '0';
            status_puf_done <= '0';
            status_key_exp_busy <= '0';
            status_key_exp_done <= '0';
            status_aes_busy <= '0';
            status_aes_done <= '0';
            puf_key_reg <= (others => '0');
            keys_ready <= '0';
            for i in 0 to 14 loop
                round_keys_reg(i) <= (others => '0');
            end loop;
        elsif rising_edge(clk) then
            puf_generate <= '0';
            key_exp_start <= '0';
            aes_start <= '0';
            status_puf_done <= '0';
            status_key_exp_done <= '0';
            status_aes_done <= '0';

            status_puf_busy <= puf_busy;
            status_key_exp_busy <= key_exp_busy;
            status_aes_busy <= aes_busy;

            case sys_state is
                when SYS_IDLE =>
                    if cmd_generate_key = '1' then
                        puf_generate <= '1';
                        keys_ready <= '0';
                        sys_state <= SYS_PUF_GENERATING;
                    elsif cmd_start_decrypt = '1' and keys_ready = '1' then
                        aes_ciphertext <= data_in;
                        aes_start <= '1';
                        sys_state <= SYS_DECRYPTING;
                    end if;

                when SYS_PUF_GENERATING =>
                    if puf_key_valid = '1' then
                        puf_key_reg <= puf_key;
                        status_puf_done <= '1';
                        sys_state <= SYS_PUF_DONE;
                    end if;

                when SYS_PUF_DONE =>
                    key_exp_key_in <= puf_key_reg;
                    key_exp_start <= '1';
                    sys_state <= SYS_KEY_EXPANDING;

                when SYS_KEY_EXPANDING =>
                    if key_exp_done = '1' then
                        for i in 0 to 14 loop
                            round_keys_reg(i) <= key_exp_round_keys(i);
                        end loop;
                        status_key_exp_done <= '1';
                        sys_state <= SYS_KEY_DONE;
                    end if;

                when SYS_KEY_DONE =>
                    keys_ready <= '1';
                    sys_state <= SYS_READY;

                when SYS_READY =>
                    if cmd_start_decrypt = '1' then
                        aes_ciphertext <= data_in;
                        aes_start <= '1';
                        sys_state <= SYS_DECRYPTING;
                    elsif cmd_generate_key = '1' then
                        puf_generate <= '1';
                        keys_ready <= '0';
                        sys_state <= SYS_PUF_GENERATING;
                    end if;

                when SYS_DECRYPTING =>
                    if aes_done = '1' then
                        data_out <= aes_plaintext;
                        status_aes_done <= '1';
                        sys_state <= SYS_DECRYPT_DONE;
                    end if;

                when SYS_DECRYPT_DONE =>
                    sys_state <= SYS_READY;

                when others =>
                    sys_state <= SYS_IDLE;
            end case;
        end if;
    end process;

end architecture rtl;
