library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity ring_oscillator is
    generic (
        NUM_INVERTERS : integer := 3;
        CHAIN_ID      : integer := 0
    );
    port (
        enable      : in  std_logic;
        osc_out     : out std_logic
    );
end entity ring_oscillator;

architecture rtl of ring_oscillator is

    signal chain : std_logic_vector(NUM_INVERTERS - 1 downto 0);

    attribute DONT_TOUCH : string;
    attribute DONT_TOUCH of chain : signal is "TRUE";

    attribute KEEP : string;
    attribute KEEP of chain : signal is "TRUE";

    attribute ALLOW_COMBINATORIAL_LOOPS : string;
    attribute ALLOW_COMBINATORIAL_LOOPS of chain : signal is "TRUE";

begin

    chain(0) <= not chain(NUM_INVERTERS - 1) when enable = '1' else '0';

    gen_inverters: for i in 1 to NUM_INVERTERS - 1 generate
        chain(i) <= not chain(i - 1);
    end generate gen_inverters;

    osc_out <= chain(NUM_INVERTERS - 1);

end architecture rtl;
