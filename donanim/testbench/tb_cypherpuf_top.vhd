library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.aes_pkg.ALL;

entity tb_cypherpuf_top is
end entity tb_cypherpuf_top;

architecture sim of tb_cypherpuf_top is

    constant CLK_PERIOD : time := 10 ns;

    signal clk                 : std_logic := '0';
    signal rst                 : std_logic := '1';
    signal cmd_generate_key    : std_logic := '0';
    signal cmd_start_decrypt   : std_logic := '0';
    signal data_in             : std_logic_vector(127 downto 0) := (others => '0');
    signal data_out            : std_logic_vector(127 downto 0);
    signal status_puf_busy     : std_logic;
    signal status_puf_done     : std_logic;
    signal status_key_exp_busy : std_logic;
    signal status_key_exp_done : std_logic;
    signal status_aes_busy     : std_logic;
    signal status_aes_done     : std_logic;
    signal debug_puf_key       : std_logic_vector(255 downto 0);
    signal debug_bit_index     : std_logic_vector(8 downto 0);
    signal debug_count_a       : std_logic_vector(19 downto 0);
    signal debug_count_b       : std_logic_vector(19 downto 0);

    component cypherpuf_top is
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
            debug_count_a       : out std_logic_vector(19 downto 0);
            debug_count_b       : out std_logic_vector(19 downto 0)
        );
    end component;

begin

    clk <= not clk after CLK_PERIOD / 2;

    uut: cypherpuf_top
        generic map (
            NUM_RO_PAIRS    => 16,
            NUM_INVERTERS   => 3,
            COUNTER_WIDTH   => 20,
            COUNT_CYCLES    => 50,
            PUF_REPETITIONS => 3
        )
        port map (
            clk                 => clk,
            rst                 => rst,
            cmd_generate_key    => cmd_generate_key,
            cmd_start_decrypt   => cmd_start_decrypt,
            data_in             => data_in,
            data_out            => data_out,
            status_puf_busy     => status_puf_busy,
            status_puf_done     => status_puf_done,
            status_key_exp_busy => status_key_exp_busy,
            status_key_exp_done => status_key_exp_done,
            status_aes_busy     => status_aes_busy,
            status_aes_done     => status_aes_done,
            debug_puf_key       => debug_puf_key,
            debug_bit_index     => debug_bit_index,
            debug_count_a       => debug_count_a,
            debug_count_b       => debug_count_b
        );

    process
    begin
        report "========================================";
        report "CypherPUF Top-Level Entegrasyon Testi";
        report "========================================";

        rst <= '1';
        wait for CLK_PERIOD * 10;
        rst <= '0';
        wait for CLK_PERIOD * 5;

        report "ADIM 1: PUF anahtar uretimi baslatiliyor...";
        cmd_generate_key <= '1';
        wait for CLK_PERIOD;
        cmd_generate_key <= '0';

        wait until status_key_exp_done = '1';
        wait for CLK_PERIOD * 2;

        report "PUF Anahtar uretimi ve key expansion TAMAMLANDI.";
        report "PUF Key: " & to_hstring(debug_puf_key);

        report "ADIM 2: Sifre cozme (decrypt) testi baslatiliyor...";
        data_in <= x"00112233445566778899AABBCCDDEEFF";

        cmd_start_decrypt <= '1';
        wait for CLK_PERIOD;
        cmd_start_decrypt <= '0';

        wait until status_aes_done = '1';
        wait for CLK_PERIOD * 2;

        report "Sifreli Giris : 00112233445566778899AABBCCDDEEFF";
        report "Cozulen Cikis : " & to_hstring(data_out);
        report "Decrypt TAMAMLANDI.";

        report "ADIM 3: Ikinci blok decrypt testi...";
        data_in <= x"DEADBEEFCAFEBABE1234567890ABCDEF";

        cmd_start_decrypt <= '1';
        wait for CLK_PERIOD;
        cmd_start_decrypt <= '0';

        wait until status_aes_done = '1';
        wait for CLK_PERIOD * 2;

        report "Sifreli Giris : DEADBEEFCAFEBABE1234567890ABCDEF";
        report "Cozulen Cikis : " & to_hstring(data_out);
        report "Ikinci blok decrypt TAMAMLANDI.";

        report "ADIM 4: Anahtar yeniden uretimi testi...";
        cmd_generate_key <= '1';
        wait for CLK_PERIOD;
        cmd_generate_key <= '0';

        wait until status_key_exp_done = '1';
        wait for CLK_PERIOD * 2;

        report "Yeni PUF Key: " & to_hstring(debug_puf_key);
        report "Anahtar yenileme TAMAMLANDI.";

        report "ADIM 5: Yeni anahtarla decrypt testi...";
        data_in <= x"AABBCCDD11223344FFEEDDCC99887766";

        cmd_start_decrypt <= '1';
        wait for CLK_PERIOD;
        cmd_start_decrypt <= '0';

        wait until status_aes_done = '1';
        wait for CLK_PERIOD * 2;

        report "Sifreli Giris : AABBCCDD11223344FFEEDDCC99887766";
        report "Cozulen Cikis : " & to_hstring(data_out);
        report "Yeni anahtarla decrypt TAMAMLANDI.";

        report "========================================";
        report "TUM ENTEGRASYON TESTLERI TAMAMLANDI.";
        report "========================================";

        wait for CLK_PERIOD * 20;
        assert false report "Simulasyon tamamlandi." severity failure;
        wait;
    end process;

end architecture sim;
