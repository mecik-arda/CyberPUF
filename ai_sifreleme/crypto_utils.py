import os
import hashlib

def get_puf_key():
    """Returns the PUF key from environment variable."""
    env_key = os.environ.get('CYBERPUF_AES_KEY')
    if env_key:
        try:
            return bytes.fromhex(env_key)
        except ValueError:
            raise ValueError("CYBERPUF_AES_KEY must be a valid hex string.")
            
    raise EnvironmentError("CYBERPUF_AES_KEY environment variable is not set. Fail-fast triggered.")

def derive_key_from_puf_simulation(raw_puf_key):
    """Derives a 256-bit AES key from the raw PUF response using PBKDF2-HMAC-SHA256."""
    salt = b'CyberPUF_Phase1_Salt_Constant'
    return hashlib.pbkdf2_hmac('sha256', raw_puf_key, salt, 100000, dklen=32)
