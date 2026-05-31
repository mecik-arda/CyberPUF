library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.aes_pkg.ALL;

entity tb_aes256 is
end entity tb_aes256;

architecture sim of tb_aes256 is

    constant CLK_PERIOD : time := 10 ns;

    signal clk          : std_logic := '0';
    signal rst          : std_logic := '1';

    signal enc_plaintext  : std_logic_vector(127 downto 0);
    signal enc_round_keys : round_key_array_t;
    signal enc_start      : std_logic := '0';
    signal enc_ciphertext : std_logic_vector(127 downto 0);
    signal enc_done       : std_logic;
    signal enc_busy       : std_logic;

    signal dec_ciphertext : std_logic_vector(127 downto 0);
    signal dec_round_keys : round_key_array_t;
    signal dec_start      : std_logic := '0';
    signal dec_plaintext  : std_logic_vector(127 downto 0);
    signal dec_done       : std_logic;
    signal dec_busy       : std_logic;

    signal key_in         : std_logic_vector(255 downto 0);
    signal key_start      : std_logic := '0';
    signal key_round_keys : round_key_array_t;
    signal key_done       : std_logic;
    signal key_busy       : std_logic;

    signal test_passed    : boolean := true;

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

    component aes256_cipher is
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

    clk <= not clk after CLK_PERIOD / 2;

    key_exp_inst: aes256_key_expansion
        port map (
            clk        => clk,
            rst        => rst,
            key_in     => key_in,
            start      => key_start,
            round_keys => key_round_keys,
            done       => key_done,
            busy       => key_busy
        );

    enc_inst: aes256_cipher
        port map (
            clk        => clk,
            rst        => rst,
            plaintext  => enc_plaintext,
            round_keys => enc_round_keys,
            start      => enc_start,
            ciphertext => enc_ciphertext,
            done       => enc_done,
            busy       => enc_busy
        );

    dec_inst: aes256_decryption
        port map (
            clk        => clk,
            rst        => rst,
            ciphertext => dec_ciphertext,
            round_keys => dec_round_keys,
            start      => dec_start,
            plaintext  => dec_plaintext,
            done       => dec_done,
            busy       => dec_busy
        );

    process
        variable expected_ct : std_logic_vector(127 downto 0);
    begin
        rst <= '1';
        wait for CLK_PERIOD * 5;
        rst <= '0';
        wait for CLK_PERIOD * 2;

        report "========================================";
        report "TEST 1: NIST AES-256 Key Expansion";
        report "========================================";

        key_in <= x"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
        key_start <= '1';
        wait for CLK_PERIOD;
        key_start <= '0';

        wait until key_done = '1';
        wait for CLK_PERIOD;

        for i in 0 to 14 loop
            enc_round_keys(i) <= key_round_keys(i);
            dec_round_keys(i) <= key_round_keys(i);
        end loop;

        report "Key expansion tamamlandi.";

        report "========================================";
        report "TEST 2: AES-256 Encryption";
        report "========================================";

        enc_plaintext <= x"00112233445566778899aabbccddeeff";
        expected_ct := x"8ea2b7ca516745bfeafc49904b496089";

        enc_start <= '1';
        wait for CLK_PERIOD;
        enc_start <= '0';

        wait until enc_done = '1';
        wait for CLK_PERIOD;

        report "Plaintext  : 00112233445566778899aabbccddeeff";
        report "Ciphertext : " & to_hstring(enc_ciphertext);
        report "Expected   : 8ea2b7ca516745bfeafc49904b496089";

        if enc_ciphertext = expected_ct then
            report "TEST 2 BASARILI: Encryption dogru!";
        else
            report "TEST 2 BASARISIZ: Encryption yanlis!" severity error;
            test_passed <= false;
        end if;

        report "========================================";
        report "TEST 3: AES-256 Decryption";
        report "========================================";

        dec_ciphertext <= enc_ciphertext;

        dec_start <= '1';
        wait for CLK_PERIOD;
        dec_start <= '0';

        wait until dec_done = '1';
        wait for CLK_PERIOD;

        report "Ciphertext  : " & to_hstring(enc_ciphertext);
        report "Decrypted   : " & to_hstring(dec_plaintext);
        report "Expected PT : 00112233445566778899aabbccddeeff";

        if dec_plaintext = x"00112233445566778899aabbccddeeff" then
            report "TEST 3 BASARILI: Decryption dogru!";
        else
            report "TEST 3 BASARISIZ: Decryption yanlis!" severity error;
            test_passed <= false;
        end if;

        report "========================================";
        report "TEST 4: Encrypt-then-Decrypt Round Trip";
        report "========================================";

        enc_plaintext <= x"DEADBEEFCAFEBABE1234567890ABCDEF";
        enc_start <= '1';
        wait for CLK_PERIOD;
        enc_start <= '0';

        wait until enc_done = '1';
        wait for CLK_PERIOD;

        report "Orijinal PT : DEADBEEFCAFEBABE1234567890ABCDEF";
        report "Encrypted   : " & to_hstring(enc_ciphertext);

        dec_ciphertext <= enc_ciphertext;
        dec_start <= '1';
        wait for CLK_PERIOD;
        dec_start <= '0';

        wait until dec_done = '1';
        wait for CLK_PERIOD;

        report "Decrypted   : " & to_hstring(dec_plaintext);

        if dec_plaintext = x"DEADBEEFCAFEBABE1234567890ABCDEF" then
            report "TEST 4 BASARILI: Round-trip dogru!";
        else
            report "TEST 4 BASARISIZ: Round-trip yanlis!" severity error;
            test_passed <= false;
        end if;

        report "========================================";
        report "TEST 5: Farkli Anahtar ile Sifreleme";
        report "========================================";

        key_in <= x"603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4";
        key_start <= '1';
        wait for CLK_PERIOD;
        key_start <= '0';

        wait until key_done = '1';
        wait for CLK_PERIOD;

        for i in 0 to 14 loop
            enc_round_keys(i) <= key_round_keys(i);
            dec_round_keys(i) <= key_round_keys(i);
        end loop;

        enc_plaintext <= x"6BC1BEE22E409F96E93D7E117393172A";
        enc_start <= '1';
        wait for CLK_PERIOD;
        enc_start <= '0';

        wait until enc_done = '1';
        wait for CLK_PERIOD;

        report "Key         : 603DEB10...14DFF4";
        report "Plaintext   : 6BC1BEE22E409F96E93D7E117393172A";
        report "Encrypted   : " & to_hstring(enc_ciphertext);

        dec_ciphertext <= enc_ciphertext;
        dec_start <= '1';
        wait for CLK_PERIOD;
        dec_start <= '0';

        wait until dec_done = '1';
        wait for CLK_PERIOD;

        report "Decrypted   : " & to_hstring(dec_plaintext);

        if dec_plaintext = x"6BC1BEE22E409F96E93D7E117393172A" then
            report "TEST 5 BASARILI: Farkli anahtar round-trip dogru!";
        else
            report "TEST 5 BASARISIZ: Farkli anahtar round-trip yanlis!" severity error;
            test_passed <= false;
        end if;

        report "========================================";
        report "GENEL SONUC";
        report "========================================";

        if test_passed then
            report "TUM TESTLER BASARILI!";
        else
            report "BAZI TESTLER BASARISIZ OLDU!" severity error;
        end if;

        wait for CLK_PERIOD * 10;

        assert false report "Simulasyon tamamlandi." severity failure;
        wait;
    end process;

end architecture sim;
