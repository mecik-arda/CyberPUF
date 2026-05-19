# GHDL compile script
$ghdl_cmd = "ghdl"

Remove-Item -Path "*.cf" -ErrorAction SilentlyContinue

& $ghdl_cmd -a --std=08 vhdl\aes_pkg.vhd
& $ghdl_cmd -a --std=08 vhdl\ring_oscillator.vhd
& $ghdl_cmd -a --std=08 vhdl\ro_puf_core.vhd
& $ghdl_cmd -a --std=08 vhdl\puf_key_generator.vhd
& $ghdl_cmd -a --std=08 vhdl\aes256_key_expansion.vhd
& $ghdl_cmd -a --std=08 vhdl\aes256_cipher.vhd
& $ghdl_cmd -a --std=08 vhdl\aes256_decryption.vhd
& $ghdl_cmd -a --std=08 vhdl\cypherpuf_top.vhd
& $ghdl_cmd -a --std=08 vhdl\axi4_lite_wrapper.vhd
& $ghdl_cmd -a --std=08 testbench\tb_aes256.vhd
& $ghdl_cmd -a --std=08 testbench\tb_cypherpuf_top.vhd
& $ghdl_cmd -a --std=08 testbench\tb_axi4_lite.vhd

& $ghdl_cmd -m --std=08 tb_aes256
& $ghdl_cmd -m --std=08 tb_cypherpuf_top
& $ghdl_cmd -m --std=08 tb_axi4_lite

echo "Compilation done!"
