import pytest
import random

def encode_hamming74(data_nibble):
    """4-bit veriyi 7-bit Hamming(7,4) koduna cevirir."""
    d1 = (data_nibble >> 3) & 1
    d2 = (data_nibble >> 2) & 1
    d3 = (data_nibble >> 1) & 1
    d4 = data_nibble & 1
    
    p1 = d1 ^ d2 ^ d4
    p2 = d1 ^ d3 ^ d4
    p3 = d2 ^ d3 ^ d4
    
    return (p1 << 6) | (p2 << 5) | (d1 << 4) | (p3 << 3) | (d2 << 2) | (d3 << 1) | d4

def decode_hamming74(codeword):
    """7-bit Hamming(7,4) kodunu cozer ve tek bit hatalarini duzeltir."""
    p1 = (codeword >> 6) & 1
    p2 = (codeword >> 5) & 1
    d1 = (codeword >> 4) & 1
    p3 = (codeword >> 3) & 1
    d2 = (codeword >> 2) & 1
    d3 = (codeword >> 1) & 1
    d4 = codeword & 1
    
    s1 = p1 ^ d1 ^ d2 ^ d4
    s2 = p2 ^ d1 ^ d3 ^ d4
    s3 = p3 ^ d2 ^ d3 ^ d4
    syndrome = (s1 << 2) | (s2 << 1) | s3
    
    error_bit = -1
    if syndrome == 0b110: error_bit = 4 # d1
    elif syndrome == 0b101: error_bit = 2 # d2
    elif syndrome == 0b011: error_bit = 1 # d3
    elif syndrome == 0b111: error_bit = 0 # d4
    elif syndrome == 0b100: error_bit = 6 # p1
    elif syndrome == 0b010: error_bit = 5 # p2
    elif syndrome == 0b001: error_bit = 3 # p3
    
    if error_bit != -1:
        codeword ^= (1 << error_bit)
        
    d1 = (codeword >> 4) & 1
    d2 = (codeword >> 2) & 1
    d3 = (codeword >> 1) & 1
    d4 = codeword & 1
    return (d1 << 3) | (d2 << 2) | (d3 << 1) | d4

def test_no_error():
    for data in range(16):
        encoded = encode_hamming74(data)
        decoded = decode_hamming74(encoded)
        assert decoded == data, f"Hata: {data} -> {encoded} -> {decoded}"

def test_single_bit_error():
    for data in range(16):
        encoded = encode_hamming74(data)
        for i in range(7):
            noisy_codeword = encoded ^ (1 << i)
            decoded = decode_hamming74(noisy_codeword)
            assert decoded == data, f"Veri {data}, Hata bit indexi {i} icin duzeltme basarisiz."

def test_puf_fuzzy_extraction_simulation():
    """PUF gurultusunu simule ederek 256-bit anahtarin Hamming(7,4) ile korunmasi"""
    key_bytes = [random.randint(0, 255) for _ in range(32)]
    
    encoded_blocks = []
    for byte in key_bytes:
        high_nibble = (byte >> 4) & 0x0F
        low_nibble = byte & 0x0F
        encoded_blocks.append(encode_hamming74(high_nibble))
        encoded_blocks.append(encode_hamming74(low_nibble))
        
    # Gurultu ekle (her blockta maksimum 1 bit)
    noisy_blocks = []
    for block in encoded_blocks:
        error_pos = random.randint(0, 6)
        noisy_blocks.append(block ^ (1 << error_pos))
        
    # Kurtarma
    recovered_bytes = []
    for i in range(0, len(noisy_blocks), 2):
        high_rec = decode_hamming74(noisy_blocks[i])
        low_rec = decode_hamming74(noisy_blocks[i+1])
        recovered_bytes.append((high_rec << 4) | low_rec)
        
    assert key_bytes == recovered_bytes, "256-bit PUF anahtari bit hatalarina ragmen onarilamadi!"

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
