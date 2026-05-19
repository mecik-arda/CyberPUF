import os
import sys
import json
import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers, regularizers, callbacks


def build_cypherpuf_cnn(input_shape=(32, 32, 3), num_classes=10):
    model = keras.Sequential()

    model.add(layers.Input(shape=input_shape))

    model.add(layers.Conv2D(
        filters=64,
        kernel_size=(3, 3),
        padding='same',
        kernel_regularizer=regularizers.l2(1e-4)
    ))
    model.add(layers.BatchNormalization())
    model.add(layers.Activation('relu'))
    model.add(layers.Conv2D(
        filters=64,
        kernel_size=(3, 3),
        padding='same',
        kernel_regularizer=regularizers.l2(1e-4)
    ))
    model.add(layers.BatchNormalization())
    model.add(layers.Activation('relu'))
    model.add(layers.MaxPooling2D(pool_size=(2, 2)))
    model.add(layers.Dropout(0.25))

    model.add(layers.Conv2D(
        filters=128,
        kernel_size=(3, 3),
        padding='same',
        kernel_regularizer=regularizers.l2(1e-4)
    ))
    model.add(layers.BatchNormalization())
    model.add(layers.Activation('relu'))
    model.add(layers.Conv2D(
        filters=128,
        kernel_size=(3, 3),
        padding='same',
        kernel_regularizer=regularizers.l2(1e-4)
    ))
    model.add(layers.BatchNormalization())
    model.add(layers.Activation('relu'))
    model.add(layers.MaxPooling2D(pool_size=(2, 2)))
    model.add(layers.Dropout(0.30))

    model.add(layers.Conv2D(
        filters=256,
        kernel_size=(3, 3),
        padding='same',
        kernel_regularizer=regularizers.l2(1e-4)
    ))
    model.add(layers.BatchNormalization())
    model.add(layers.Activation('relu'))
    model.add(layers.Conv2D(
        filters=256,
        kernel_size=(3, 3),
        padding='same',
        kernel_regularizer=regularizers.l2(1e-4)
    ))
    model.add(layers.BatchNormalization())
    model.add(layers.Activation('relu'))
    model.add(layers.MaxPooling2D(pool_size=(2, 2)))
    model.add(layers.Dropout(0.35))

    model.add(layers.Flatten())

    model.add(layers.Dense(
        512,
        kernel_regularizer=regularizers.l2(1e-4)
    ))
    model.add(layers.BatchNormalization())
    model.add(layers.Activation('relu'))
    model.add(layers.Dropout(0.5))

    model.add(layers.Dense(
        256,
        kernel_regularizer=regularizers.l2(1e-4)
    ))
    model.add(layers.BatchNormalization())
    model.add(layers.Activation('relu'))
    model.add(layers.Dropout(0.5))

    model.add(layers.Dense(num_classes, activation='softmax'))

    return model


def load_and_preprocess_cifar10():
    (x_train, y_train), (x_test, y_test) = keras.datasets.cifar10.load_data()

    x_train = x_train.astype('float32') / 255.0
    x_test = x_test.astype('float32') / 255.0

    channel_mean = np.mean(x_train, axis=(0, 1, 2))
    channel_std = np.std(x_train, axis=(0, 1, 2))

    x_train = (x_train - channel_mean) / (channel_std + 1e-7)
    x_test = (x_test - channel_mean) / (channel_std + 1e-7)

    y_train = keras.utils.to_categorical(y_train, 10)
    y_test = keras.utils.to_categorical(y_test, 10)

    normalization_params = {
        'channel_mean': channel_mean.tolist(),
        'channel_std': channel_std.tolist()
    }

    return (x_train, y_train), (x_test, y_test), normalization_params


def create_data_augmentation():
    datagen = keras.preprocessing.image.ImageDataGenerator(
        rotation_range=15,
        width_shift_range=0.1,
        height_shift_range=0.1,
        horizontal_flip=True,
        fill_mode='nearest'
    )
    return datagen


def setup_callbacks(output_dir):
    checkpoint_path = os.path.join(output_dir, 'best_model.weights.h5')

    cb_list = []

    cb_list.append(callbacks.ModelCheckpoint(
        filepath=checkpoint_path,
        monitor='val_accuracy',
        save_best_only=True,
        save_weights_only=True,
        mode='max',
        verbose=1
    ))

    cb_list.append(callbacks.ReduceLROnPlateau(
        monitor='val_loss',
        factor=0.5,
        patience=5,
        min_lr=1e-6,
        verbose=1
    ))

    cb_list.append(callbacks.EarlyStopping(
        monitor='val_accuracy',
        patience=15,
        restore_best_weights=True,
        verbose=1
    ))

    csv_log_path = os.path.join(output_dir, 'training_log.csv')
    cb_list.append(callbacks.CSVLogger(
        csv_log_path,
        separator=',',
        append=False
    ))

    return cb_list


def train_model(epochs=100, batch_size=128, learning_rate=0.001):
    output_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'output', 'model')
    os.makedirs(output_dir, exist_ok=True)

    print("=" * 70)
    print("CypherPUF - Faz 1: CIFAR-10 CNN Model Egitimi")
    print("Gelistirici: Arda Mecik")
    print("=" * 70)

    print("\n[1/6] CIFAR-10 veri seti yukleniyor ve on isleniyor...")
    (x_train, y_train), (x_test, y_test), norm_params = load_and_preprocess_cifar10()
    print(f"  Egitim seti boyutu  : {x_train.shape}")
    print(f"  Test seti boyutu    : {x_test.shape}")
    print(f"  Kanal ortalamasi    : {norm_params['channel_mean']}")
    print(f"  Kanal std sapmasi   : {norm_params['channel_std']}")

    norm_params_path = os.path.join(output_dir, 'normalization_params.json')
    with open(norm_params_path, 'w') as f:
        json.dump(norm_params, f, indent=2)
    print(f"  Normalizasyon parametreleri kaydedildi: {norm_params_path}")

    print("\n[2/6] CNN modeli olusturuluyor...")
    model = build_cypherpuf_cnn(input_shape=(32, 32, 3), num_classes=10)
    model.summary()

    print("\n[3/6] Model derleniyor...")
    optimizer = keras.optimizers.Adam(
        learning_rate=learning_rate,
        beta_1=0.9,
        beta_2=0.999,
        epsilon=1e-07
    )

    model.compile(
        optimizer=optimizer,
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )

    print("\n[4/6] Veri artirma (data augmentation) hazirlaniyor...")
    datagen = create_data_augmentation()
    datagen.fit(x_train)

    print("\n[5/6] Egitim basliyor...")
    print(f"  Epoch sayisi        : {epochs}")
    print(f"  Batch boyutu        : {batch_size}")
    print(f"  Baslangic ogrenme h.: {learning_rate}")
    print("-" * 70)

    cb_list = setup_callbacks(output_dir)

    history = model.fit(
        datagen.flow(x_train, y_train, batch_size=batch_size),
        steps_per_epoch=len(x_train) // batch_size,
        epochs=epochs,
        validation_data=(x_test, y_test),
        callbacks=cb_list,
        verbose=1
    )

    print("\n[6/6] Egitim tamamlandi. Sonuclar degerlendiriliyor...")
    test_loss, test_accuracy = model.evaluate(x_test, y_test, verbose=0)
    print(f"  Test kaybi (loss)   : {test_loss:.4f}")
    print(f"  Test dogrulugu (acc): {test_accuracy:.4f}")

    full_model_path = os.path.join(output_dir, 'cypherpuf_cifar10_model.h5')
    model.save(full_model_path)
    print(f"  Tam model kaydedildi: {full_model_path}")

    weights_only_path = os.path.join(output_dir, 'cypherpuf_cifar10_weights.weights.h5')
    model.save_weights(weights_only_path)
    print(f"  Agirliklar kaydedildi: {weights_only_path}")

    training_summary = {
        'project': 'CypherPUF',
        'developer': 'Arda Mecik',
        'phase': 'Faz 1 - AI Model Egitimi',
        'dataset': 'CIFAR-10',
        'input_shape': [32, 32, 3],
        'num_classes': 10,
        'total_epochs_trained': len(history.history['loss']),
        'final_train_loss': float(history.history['loss'][-1]),
        'final_train_accuracy': float(history.history['accuracy'][-1]),
        'final_val_loss': float(history.history['val_loss'][-1]),
        'final_val_accuracy': float(history.history['val_accuracy'][-1]),
        'test_loss': float(test_loss),
        'test_accuracy': float(test_accuracy),
        'batch_size': batch_size,
        'initial_learning_rate': learning_rate,
        'optimizer': 'Adam',
        'regularization': 'L2(1e-4) + Dropout',
        'data_augmentation': True,
        'model_file': full_model_path,
        'weights_file': weights_only_path,
        'normalization_params': norm_params
    }

    summary_path = os.path.join(output_dir, 'training_summary.json')
    with open(summary_path, 'w') as f:
        json.dump(training_summary, f, indent=2)
    print(f"  Egitim ozeti kaydedildi: {summary_path}")

    total_params = model.count_params()
    trainable_params = sum(
        tf.keras.backend.count_params(w) for w in model.trainable_weights
    )
    non_trainable_params = total_params - trainable_params

    print("\n" + "=" * 70)
    print("MODEL ISTATISTIKLERI")
    print("=" * 70)
    print(f"  Toplam parametre    : {total_params:,}")
    print(f"  Egitilebiir param.  : {trainable_params:,}")
    print(f"  Dondurulan param.   : {non_trainable_params:,}")
    print(f"  Tahm. bellek (MB)   : {(total_params * 4) / (1024 * 1024):.2f}")
    print("=" * 70)

    layer_info = []
    for layer in model.layers:
        layer_weights = layer.get_weights()
        if len(layer_weights) > 0:
            layer_detail = {
                'name': layer.name,
                'type': layer.__class__.__name__,
                'num_arrays': len(layer_weights),
                'shapes': [w.shape for w in layer_weights],
                'total_params': sum(np.prod(w.shape) for w in layer_weights),
                'dtypes': [str(w.dtype) for w in layer_weights]
            }
            layer_info.append(layer_detail)
            print(f"  Katman: {layer.name:30s} | Tip: {layer.__class__.__name__:20s} | Parametre: {layer_detail['total_params']:>10,}")

    layer_info_serializable = []
    for info in layer_info:
        serializable = {
            'name': info['name'],
            'type': info['type'],
            'num_arrays': info['num_arrays'],
            'shapes': [list(s) for s in info['shapes']],
            'total_params': int(info['total_params']),
            'dtypes': info['dtypes']
        }
        layer_info_serializable.append(serializable)

    layer_info_path = os.path.join(output_dir, 'layer_info.json')
    with open(layer_info_path, 'w') as f:
        json.dump(layer_info_serializable, f, indent=2)
    print(f"\n  Katman bilgileri kaydedildi: {layer_info_path}")

    print("\n" + "=" * 70)
    print("FAZ 1 - ADIM 1 TAMAMLANDI: Model egitildi ve kaydedildi.")
    print("Sonraki adim: export_weights.py ile agirliklari disa aktarin.")
    print("=" * 70)

    return model, history


if __name__ == '__main__':
    custom_epochs = 100
    custom_batch_size = 128
    custom_lr = 0.001

    if len(sys.argv) > 1:
        custom_epochs = int(sys.argv[1])
    if len(sys.argv) > 2:
        custom_batch_size = int(sys.argv[2])
    if len(sys.argv) > 3:
        custom_lr = float(sys.argv[3])

    trained_model, training_history = train_model(
        epochs=custom_epochs,
        batch_size=custom_batch_size,
        learning_rate=custom_lr
    )
