library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity ro_puf_core is
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
end entity ro_puf_core;

architecture rtl of ro_puf_core is

    constant TOTAL_RO : integer := NUM_RO_PAIRS * 2;

    signal ro_enable : std_logic_vector(TOTAL_RO - 1 downto 0);
    signal ro_output : std_logic_vector(TOTAL_RO - 1 downto 0);

    signal counter_a : unsigned(COUNTER_WIDTH - 1 downto 0);
    signal counter_b : unsigned(COUNTER_WIDTH - 1 downto 0);
    signal ref_counter : unsigned(COUNTER_WIDTH - 1 downto 0);

    signal selected_ro_a : integer range 0 to TOTAL_RO - 1;
    signal selected_ro_b : integer range 0 to TOTAL_RO - 1;

    signal ro_a_sync : std_logic_vector(2 downto 0);
    signal ro_b_sync : std_logic_vector(2 downto 0);
    signal ro_a_edge : std_logic;
    signal ro_b_edge : std_logic;

    type state_t is (IDLE, SELECT_RO, COUNTING, COMPARE, OUTPUT_RESULT);
    signal state : state_t;

    component ring_oscillator is
        generic (
            NUM_INVERTERS : integer := 3;
            CHAIN_ID      : integer := 0
        );
        port (
            enable      : in  std_logic;
            osc_out     : out std_logic
        );
    end component;

begin

    gen_ro: for i in 0 to TOTAL_RO - 1 generate
        ro_inst: ring_oscillator
            generic map (
                NUM_INVERTERS => NUM_INVERTERS,
                CHAIN_ID      => i
            )
            port map (
                enable  => ro_enable(i),
                osc_out => ro_output(i)
            );
    end generate gen_ro;

    process(clk, rst)
    begin
        if rst = '1' then
            ro_a_sync <= (others => '0');
            ro_b_sync <= (others => '0');
        elsif rising_edge(clk) then
            if selected_ro_a < TOTAL_RO then
                ro_a_sync <= ro_a_sync(1 downto 0) & ro_output(selected_ro_a);
            end if;
            if selected_ro_b < TOTAL_RO then
                ro_b_sync <= ro_b_sync(1 downto 0) & ro_output(selected_ro_b);
            end if;
        end if;
    end process;

    ro_a_edge <= ro_a_sync(2) xor ro_a_sync(1);
    ro_b_edge <= ro_b_sync(2) xor ro_b_sync(1);

    process(clk, rst)
        variable pair_index : integer;
    begin
        if rst = '1' then
            state <= IDLE;
            response_bit <= '0';
            response_valid <= '0';
            busy <= '0';
            counter_a <= (others => '0');
            counter_b <= (others => '0');
            ref_counter <= (others => '0');
            selected_ro_a <= 0;
            selected_ro_b <= 0;
            ro_enable <= (others => '0');
            ro_count_a <= (others => '0');
            ro_count_b <= (others => '0');
        elsif rising_edge(clk) then
            response_valid <= '0';

            case state is
                when IDLE =>
                    busy <= '0';
                    ro_enable <= (others => '0');
                    if start = '1' then
                        state <= SELECT_RO;
                        busy <= '1';
                    end if;

                when SELECT_RO =>
                    pair_index := to_integer(unsigned(challenge));

                    if pair_index >= NUM_RO_PAIRS then
                        pair_index := NUM_RO_PAIRS - 1;
                    end if;

                    selected_ro_a <= pair_index * 2;
                    selected_ro_b <= pair_index * 2 + 1;

                    ro_enable <= (others => '0');
                    ro_enable(pair_index * 2) <= '1';
                    ro_enable(pair_index * 2 + 1) <= '1';

                    counter_a <= (others => '0');
                    counter_b <= (others => '0');
                    ref_counter <= (others => '0');
                    state <= COUNTING;

                when COUNTING =>
                    ref_counter <= ref_counter + 1;

                    if ro_a_edge = '1' then
                        counter_a <= counter_a + 1;
                    end if;

                    if ro_b_edge = '1' then
                        counter_b <= counter_b + 1;
                    end if;

                    if ref_counter = to_unsigned(COUNT_CYCLES - 1, COUNTER_WIDTH) then
                        ro_enable <= (others => '0');
                        state <= COMPARE;
                    end if;

                when COMPARE =>
                    ro_count_a <= std_logic_vector(counter_a);
                    ro_count_b <= std_logic_vector(counter_b);

                    if counter_a > counter_b then
                        response_bit <= '1';
                    else
                        response_bit <= '0';
                    end if;
                    state <= OUTPUT_RESULT;

                when OUTPUT_RESULT =>
                    response_valid <= '1';
                    busy <= '0';
                    state <= IDLE;

                when others =>
                    state <= IDLE;
            end case;
        end if;
    end process;

end architecture rtl;
