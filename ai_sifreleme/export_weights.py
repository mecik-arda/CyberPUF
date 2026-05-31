import os
import sys
import json
import struct
import hashlib
import numpy as np
import tensorflow as tf
from tensorflow import keras


def load_trained_model(model_path):
    print(f"  Model yukleniyor: {model_path}")
    model = keras.models.load_model(model_path)
    print(f"  Model basariyla yuklendi.")
    print(f"  Toplam katman sayisi: {len(model.layers)}")
    return model


def extract_all_weights(model):
    weight_manifest = []
    all_weight_arrays = []
    total_params = 0

    for layer_idx, layer in enumerate(model.layers):
        layer_weights = layer.get_weights()

        if len(layer_weights) == 0:
            continue

        layer_entry = {
            'layer_index': layer_idx,
            'layer_name': layer.name,
            'layer_type': layer.__class__.__name__,
            'arrays': []
        }

        for arr_idx, weight_array in enumerate(layer_weights):
            array_info = {
                'array_index': arr_idx,
                'original_shape': list(weight_array.shape),
                'dtype': str(weight_array.dtype),
                'num_elements': int(np.prod(weight_array.shape)),
                'size_bytes': int(weight_array.nbytes),
                'min_value': float(np.min(weight_array)),
                'max_value': float(np.max(weight_array)),
                'mean_value': float(np.mean(weight_array)),
                'std_value': float(np.std(weight_array))
            }

            layer_entry['arrays'].append(array_info)
            all_weight_arrays.append(weight_array)
            total_params += array_info['num_elements']

        weight_manifest.append(layer_entry)

    return all_weight_arrays, weight_manifest, total_params


def serialize_weights_to_binary(weight_arrays, weight_manifest):
    MAGIC_NUMBER = b'CPUF'
    VERSION_MAJOR = 1
    VERSION_MINOR = 0
    HEADER_RESERVED = b'\x00' * 16

    binary_chunks = []

    header = bytearray()
    header.extend(MAGIC_NUMBER)
    header.extend(struct.pack('<B', VERSION_MAJOR))
    header.extend(struct.pack('<B', VERSION_MINOR))

    total_arrays = len(weight_arrays)
    header.extend(struct.pack('<I', total_arrays))

    total_elements = sum(np.prod(arr.shape) for arr in weight_arrays)
    header.extend(struct.pack('<Q', total_elements))

    header.extend(HEADER_RESERVED)

    binary_chunks.append(bytes(header))

    array_metadata_block = bytearray()

    for manifest_entry in weight_manifest:
        for arr_info in manifest_entry['arrays']:
            shape = arr_info['original_shape']
            ndim = len(shape)
            array_metadata_block.extend(struct.pack('<B', ndim))
            for dim in shape:
                array_metadata_block.extend(struct.pack('<I', dim))
            array_metadata_block.extend(struct.pack('<I', arr_info['num_elements']))
            array_metadata_block.extend(struct.pack('<I', arr_info['size_bytes']))

    binary_chunks.append(bytes(array_metadata_block))

    weight_data_block = bytearray()
    for arr in weight_arrays:
        flat_arr = arr.astype(np.float32).flatten()
        weight_data_block.extend(flat_arr.tobytes())

    binary_chunks.append(bytes(weight_data_block))

    full_binary = b''.join(binary_chunks)

    sha256_hash = hashlib.sha256(full_binary).hexdigest()

    return full_binary, sha256_hash


def save_weights_as_numpy(weight_arrays, weight_manifest, output_dir):
    numpy_dir = os.path.join(output_dir, 'numpy_weights')
    os.makedirs(numpy_dir, exist_ok=True)

    saved_files = []

    for manifest_entry in weight_manifest:
        layer_name = manifest_entry['layer_name']
        layer_idx = manifest_entry['layer_index']

        for arr_info in manifest_entry['arrays']:
            arr_idx = arr_info['array_index']

            global_idx = sum(
                len(m['arrays']) for m in weight_manifest
                if m['layer_index'] < layer_idx
            ) + arr_idx

            filename = f"layer_{layer_idx:03d}_{layer_name}_arr{arr_idx}.npy"
            filepath = os.path.join(numpy_dir, filename)
            np.save(filepath, weight_arrays[global_idx])
            saved_files.append(filepath)

    combined_path = os.path.join(numpy_dir, 'all_weights_combined.npz')
    weight_dict = {}
    global_counter = 0
    for manifest_entry in weight_manifest:
        layer_name = manifest_entry['layer_name']
        for arr_info in manifest_entry['arrays']:
            arr_idx = arr_info['array_index']
            key = f"{layer_name}_arr{arr_idx}"
            weight_dict[key] = weight_arrays[global_counter]
            global_counter += 1

    np.savez(combined_path, **weight_dict)
    saved_files.append(combined_path)

    return saved_files


def generate_weight_statistics(weight_arrays, weight_manifest):
    stats = {
        'total_arrays': int(len(weight_arrays)),
        'total_parameters': int(sum(np.prod(arr.shape) for arr in weight_arrays)),
        'total_size_bytes': int(sum(arr.nbytes for arr in weight_arrays)),
        'total_size_float32_bytes': int(sum(np.prod(arr.shape) * 4 for arr in weight_arrays)),
        'global_min': float(min(np.min(arr) for arr in weight_arrays)),
        'global_max': float(max(np.max(arr) for arr in weight_arrays)),
        'global_mean': float(np.mean([np.mean(arr) for arr in weight_arrays])),
        'per_layer_stats': []
    }

    global_counter = 0
    for manifest_entry in weight_manifest:
        layer_stats = {
            'layer_name': manifest_entry['layer_name'],
            'layer_type': manifest_entry['layer_type'],
            'arrays': []
        }

        for arr_info in manifest_entry['arrays']:
            arr = weight_arrays[global_counter]
            array_stats = {
                'shape': list(arr.shape),
                'min': float(np.min(arr)),
                'max': float(np.max(arr)),
                'mean': float(np.mean(arr)),
                'std': float(np.std(arr)),
                'sparsity': float(np.sum(np.abs(arr) < 1e-7) / np.prod(arr.shape)),
                'l1_norm': float(np.sum(np.abs(arr))),
                'l2_norm': float(np.sqrt(np.sum(arr ** 2)))
            }
            layer_stats['arrays'].append(array_stats)
            global_counter += 1

        stats['per_layer_stats'].append(layer_stats)

    return stats


def export_weights(model_path=None):
    base_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'output')
    model_dir = os.path.join(base_dir, 'model')
    export_dir = os.path.join(base_dir, 'exported_weights')
    os.makedirs(export_dir, exist_ok=True)

    if model_path is None:
        model_path = os.path.join(model_dir, 'cypherpuf_cifar10_model.h5')

    if not os.path.exists(model_path):
        print(f"HATA: Model dosyasi bulunamadi: {model_path}")
        print("Lutfen once train_model.py betigini calistirin.")
        sys.exit(1)

    print("=" * 70)
    print("CypherPUF - Faz 1: Agirlik Disa Aktarma (Weight Export)")
    print("Gelistirici: Arda Mecik")
    print("=" * 70)

    print("\n[1/5] Egitilmis model yukleniyor...")
    model = load_trained_model(model_path)

    print("\n[2/5] Tum agirliklar ve sapmalar cikariliyor...")
    weight_arrays, weight_manifest, total_params = extract_all_weights(model)
    print(f"  Toplam agirlik dizisi : {len(weight_arrays)}")
    print(f"  Toplam parametre      : {total_params:,}")
    print(f"  Toplam boyut (float32): {(total_params * 4) / (1024 * 1024):.2f} MB")

    for entry in weight_manifest:
        print(f"  Katman: {entry['layer_name']:30s} | Tip: {entry['layer_type']:20s} | Dizi sayisi: {entry['arrays'].__len__()}")
        for arr_info in entry['arrays']:
            print(f"    -> Sekil: {arr_info['original_shape']} | Eleman: {arr_info['num_elements']:>8,} | Min: {arr_info['min_value']:+.6f} | Max: {arr_info['max_value']:+.6f}")

    print("\n[3/5] Agirliklar ikili (binary) formata serilestriliyor...")
    binary_data, sha256_hash = serialize_weights_to_binary(weight_arrays, weight_manifest)
    print(f"  Ikili veri boyutu    : {len(binary_data):,} byte ({len(binary_data) / (1024 * 1024):.2f} MB)")
    print(f"  SHA-256 ozeti        : {sha256_hash}")

    binary_path = os.path.join(export_dir, 'cypherpuf_weights.bin')
    with open(binary_path, 'wb') as f:
        f.write(binary_data)
    print(f"  Ikili dosya kaydedildi: {binary_path}")

    print("\n[4/5] Agirliklar NumPy formatinda kaydediliyor...")
    numpy_files = save_weights_as_numpy(weight_arrays, weight_manifest, export_dir)
    print(f"  {len(numpy_files)} dosya kaydedildi.")
    for nf in numpy_files:
        print(f"    -> {nf}")

    print("\n[5/5] Agirlik istatistikleri hesaplaniyor ve manifest kaydediliyor...")
    stats = generate_weight_statistics(weight_arrays, weight_manifest)

    stats_path = os.path.join(export_dir, 'weight_statistics.json')
    with open(stats_path, 'w') as f:
        json.dump(stats, f, indent=2)
    print(f"  Istatistik dosyasi: {stats_path}")

    manifest_serializable = []
    for entry in weight_manifest:
        s_entry = {
            'layer_index': entry['layer_index'],
            'layer_name': entry['layer_name'],
            'layer_type': entry['layer_type'],
            'arrays': []
        }
        for arr_info in entry['arrays']:
            s_arr = {
                'array_index': arr_info['array_index'],
                'original_shape': arr_info['original_shape'],
                'dtype': arr_info['dtype'],
                'num_elements': arr_info['num_elements'],
                'size_bytes': arr_info['size_bytes'],
                'min_value': arr_info['min_value'],
                'max_value': arr_info['max_value'],
                'mean_value': arr_info['mean_value'],
                'std_value': arr_info['std_value']
            }
            s_entry['arrays'].append(s_arr)
        manifest_serializable.append(s_entry)

    manifest_data = {
        'project': 'CypherPUF',
        'developer': 'Arda Mecik',
        'phase': 'Faz 1 - Weight Export',
        'total_arrays': len(weight_arrays),
        'total_parameters': int(total_params),
        'total_size_bytes': int(total_params * 4),
        'binary_file': 'cypherpuf_weights.bin',
        'binary_sha256': sha256_hash,
        'manifest': manifest_serializable
    }

    manifest_path = os.path.join(export_dir, 'weight_manifest.json')
    with open(manifest_path, 'w') as f:
        json.dump(manifest_data, f, indent=2)
    print(f"  Manifest dosyasi: {manifest_path}")

    print("\n" + "=" * 70)
    print("FAZ 1 - ADIM 2 TAMAMLANDI: Agirliklar disa aktarildi.")
    print("Sonraki adim: encrypt_weights.py ile agirliklari sifreleyin.")
    print("=" * 70)

    return binary_data, weight_manifest, sha256_hash


if __name__ == '__main__':
    custom_model_path = None

    if len(sys.argv) > 1:
        custom_model_path = sys.argv[1]

    export_weights(model_path=custom_model_path)
