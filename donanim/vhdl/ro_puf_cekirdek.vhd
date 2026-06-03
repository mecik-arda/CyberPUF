library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity ro_puf_cekirdek is
    generic (
        RO_CIFT_SAYISI     : integer := 16;
        INVERTER_SAYISI    : integer := 3;
        SAYICI_GENISLIGI    : integer := 20;
        SAYMA_DONGULERI     : integer := 1000
    );
    port (
        clk             : in  std_logic;
        rst             : in  std_logic;
        start           : in  std_logic;
        challenge       : in  std_logic_vector(7 downto 0);
        response_bit    : out std_logic;
        response_valid  : out std_logic;
        busy            : out std_logic;
        ro_count_a      : out std_logic_vector(SAYICI_GENISLIGI - 1 downto 0);
        ro_count_b      : out std_logic_vector(SAYICI_GENISLIGI - 1 downto 0)
    );
end entity ro_puf_cekirdek;

architecture rtl of ro_puf_cekirdek is

    constant TOTAL_RO : integer := RO_CIFT_SAYISI * 2;

    signal ro_aktif : std_logic_vector(TOTAL_RO - 1 downto 0);
    signal ro_output : std_logic_vector(TOTAL_RO - 1 downto 0);

    signal sayac_a : unsigned(SAYICI_GENISLIGI - 1 downto 0);
    signal sayac_b : unsigned(SAYICI_GENISLIGI - 1 downto 0);
    signal ref_counter : unsigned(SAYICI_GENISLIGI - 1 downto 0);

    signal selected_ro_a : integer range 0 to TOTAL_RO - 1;
    signal selected_ro_b : integer range 0 to TOTAL_RO - 1;

    signal ro_clk_a : std_logic;
    signal ro_clk_b : std_logic;
    signal count_enable : std_logic;
    signal clear_counters : std_logic;

    type state_t is (IDLE, SELECT_RO, COUNTING, COMPARE, OUTPUT_RESULT);
    signal state : state_t;

    component halka_osilator is
        generic (
            INVERTER_SAYISI : integer := 3;
            CHAIN_ID      : integer := 0
        );
        port (
            enable      : in  std_logic;
            osc_out     : out std_logic
        );
    end component;

begin

    gen_ro: for i in 0 to TOTAL_RO - 1 generate
        ro_inst: halka_osilator
            generic map (
                INVERTER_SAYISI => INVERTER_SAYISI,
                CHAIN_ID      => i
            )
            port map (
                enable  => ro_aktif(i),
                osc_out => ro_output(i)
            );
    end generate gen_ro;

    ro_clk_a <= ro_output(selected_ro_a) when selected_ro_a < TOTAL_RO else '0';
    ro_clk_b <= ro_output(selected_ro_b) when selected_ro_b < TOTAL_RO else '0';

    process(ro_clk_a, clear_counters)
    begin
        if clear_counters = '1' then
            sayac_a <= (others => '0');
        elsif rising_edge(ro_clk_a) then
            if count_enable = '1' then
                sayac_a <= sayac_a + 1;
            end if;
        end if;
    end process;

    process(ro_clk_b, clear_counters)
    begin
        if clear_counters = '1' then
            sayac_b <= (others => '0');
        elsif rising_edge(ro_clk_b) then
            if count_enable = '1' then
                sayac_b <= sayac_b + 1;
            end if;
        end if;
    end process;

    process(clk, rst)
        variable pair_index : integer;
    begin
        if rst = '1' then
            state <= IDLE;
            response_bit <= '0';
            response_valid <= '0';
            busy <= '0';
            ref_counter <= (others => '0');
            selected_ro_a <= 0;
            selected_ro_b <= 0;
            ro_aktif <= (others => '0');
            ro_count_a <= (others => '0');
            ro_count_b <= (others => '0');
            count_enable <= '0';
            clear_counters <= '1';
        elsif rising_edge(clk) then
            response_valid <= '0';

            case state is
                when IDLE =>
                    busy <= '0';
                    ro_aktif <= (others => '0');
                    count_enable <= '0';
                    clear_counters <= '1';
                    if start = '1' then
                        state <= SELECT_RO;
                        busy <= '1';
                    end if;

                when SELECT_RO =>
                    pair_index := to_integer(unsigned(challenge));

                    if pair_index >= RO_CIFT_SAYISI then
                        pair_index := RO_CIFT_SAYISI - 1;
                    end if;

                    selected_ro_a <= pair_index * 2;
                    selected_ro_b <= pair_index * 2 + 1;

                    ro_aktif <= (others => '0');
                    ro_aktif(pair_index * 2) <= '1';
                    ro_aktif(pair_index * 2 + 1) <= '1';

                    clear_counters <= '0';
                    count_enable <= '0';
                    ref_counter <= (others => '0');
                    state <= COUNTING;

                when COUNTING =>
                    count_enable <= '1';
                    ref_counter <= ref_counter + 1;

                    if ref_counter = to_unsigned(SAYMA_DONGULERI - 1, SAYICI_GENISLIGI) then
                        count_enable <= '0';
                        ro_aktif <= (others => '0');
                        state <= COMPARE;
                    end if;

                when COMPARE =>
                    ro_count_a <= std_logic_vector(sayac_a);
                    ro_count_b <= std_logic_vector(sayac_b);

                    if sayac_a > sayac_b then
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
