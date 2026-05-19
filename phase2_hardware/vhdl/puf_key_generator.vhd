library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity puf_key_generator is
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
        puf_key         : out std_logic_vector(KEY_WIDTH - 1 downto 0);
        key_valid       : out std_logic;
        busy            : out std_logic;
        bit_index_out   : out std_logic_vector(8 downto 0);
        debug_count_a   : out std_logic_vector(COUNTER_WIDTH - 1 downto 0);
        debug_count_b   : out std_logic_vector(COUNTER_WIDTH - 1 downto 0)
    );
end entity puf_key_generator;

architecture rtl of puf_key_generator is

    type state_t is (
        IDLE,
        START_PUF,
        WAIT_PUF,
        STORE_BIT,
        NEXT_BIT,
        KEY_READY
    );
    signal state : state_t;

    signal puf_start       : std_logic;
    signal puf_challenge   : std_logic_vector(3 downto 0);
    signal puf_response    : std_logic;
    signal puf_valid       : std_logic;
    signal puf_busy        : std_logic;
    signal puf_count_a     : std_logic_vector(COUNTER_WIDTH - 1 downto 0);
    signal puf_count_b     : std_logic_vector(COUNTER_WIDTH - 1 downto 0);

    signal key_reg         : std_logic_vector(KEY_WIDTH - 1 downto 0);
    signal bit_counter     : unsigned(8 downto 0);
    signal challenge_idx   : unsigned(3 downto 0);
    signal rep_counter     : unsigned(4 downto 0);

    signal accumulated_bit : unsigned(4 downto 0);

    component ro_puf_core is
        generic (
            NUM_RO_PAIRS     : integer := 16;
            NUM_INVERTERS    : integer := 3;
            COUNTER_WIDTH    : integer := 20;
            COUNT_CYCLES     : integer := 1000
        );
        port (
            clk             : in  std_logic;
            rst             : in  std_logic;
            start           : in  std_logic;
            challenge       : in  std_logic_vector(3 downto 0);
            response_bit    : out std_logic;
            response_valid  : out std_logic;
            busy            : out std_logic;
            ro_count_a      : out std_logic_vector(COUNTER_WIDTH - 1 downto 0);
            ro_count_b      : out std_logic_vector(COUNTER_WIDTH - 1 downto 0)
        );
    end component;

begin

    puf_inst: ro_puf_core
        generic map (
            NUM_RO_PAIRS  => NUM_RO_PAIRS,
            NUM_INVERTERS => NUM_INVERTERS,
            COUNTER_WIDTH => COUNTER_WIDTH,
            COUNT_CYCLES  => COUNT_CYCLES
        )
        port map (
            clk           => clk,
            rst           => rst,
            start         => puf_start,
            challenge     => puf_challenge,
            response_bit  => puf_response,
            response_valid => puf_valid,
            busy          => puf_busy,
            ro_count_a    => puf_count_a,
            ro_count_b    => puf_count_b
        );

    debug_count_a <= puf_count_a;
    debug_count_b <= puf_count_b;
    bit_index_out <= std_logic_vector(bit_counter);

    process(clk, rst)
        variable majority_result : std_logic;
    begin
        if rst = '1' then
            state <= IDLE;
            key_reg <= (others => '0');
            puf_key <= (others => '0');
            key_valid <= '0';
            busy <= '0';
            puf_start <= '0';
            puf_challenge <= (others => '0');
            bit_counter <= (others => '0');
            challenge_idx <= (others => '0');
            rep_counter <= (others => '0');
            accumulated_bit <= (others => '0');
        elsif rising_edge(clk) then
            puf_start <= '0';
            key_valid <= '0';

            case state is
                when IDLE =>
                    busy <= '0';
                    if generate_key = '1' then
                        key_reg <= (others => '0');
                        bit_counter <= (others => '0');
                        challenge_idx <= (others => '0');
                        accumulated_bit <= (others => '0');
                        rep_counter <= (others => '0');
                        busy <= '1';
                        state <= START_PUF;
                    end if;

                when START_PUF =>
                    puf_challenge <= std_logic_vector(challenge_idx);
                    puf_start <= '1';
                    state <= WAIT_PUF;

                when WAIT_PUF =>
                    if puf_valid = '1' then
                        if puf_response = '1' then
                            accumulated_bit <= accumulated_bit + 1;
                        end if;
                        rep_counter <= rep_counter + 1;

                        if rep_counter = to_unsigned(REPETITIONS - 1, 5) then
                            state <= STORE_BIT;
                        else
                            state <= START_PUF;
                        end if;
                    end if;

                when STORE_BIT =>
                    if accumulated_bit > to_unsigned(REPETITIONS / 2, 5) then
                        majority_result := '1';
                    else
                        majority_result := '0';
                    end if;

                    key_reg(KEY_WIDTH - 1 - to_integer(bit_counter)) <= majority_result;

                    accumulated_bit <= (others => '0');
                    rep_counter <= (others => '0');

                    state <= NEXT_BIT;

                when NEXT_BIT =>
                    if bit_counter = to_unsigned(KEY_WIDTH - 1, 9) then
                        state <= KEY_READY;
                    else
                        bit_counter <= bit_counter + 1;

                        if challenge_idx = to_unsigned(NUM_RO_PAIRS - 1, 4) then
                            challenge_idx <= (others => '0');
                        else
                            challenge_idx <= challenge_idx + 1;
                        end if;

                        state <= START_PUF;
                    end if;

                when KEY_READY =>
                    puf_key <= key_reg;
                    key_valid <= '1';
                    busy <= '0';
                    state <= IDLE;

                when others =>
                    state <= IDLE;
            end case;
        end if;
    end process;

end architecture rtl;
