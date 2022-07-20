"""
Implementation of a small U-Net like model
"""
from otbtf.model import ModelBase
import tensorflow as tf
import tensorflow.keras.layers as layers
import logging

logging.basicConfig(format='%(asctime)s %(levelname)-8s %(message)s', level=logging.INFO, datefmt='%Y-%m-%d %H:%M:%S')
N_CLASSES = 6


class FCNNModel(ModelBase):
    """
    A Simple Fully Convolutional U-Net like model
    """

    def normalize_inputs(self, inputs):
        """
        The model will use this function internally to normalize its inputs, before applying the `get_outputs()`
        function that actually builds the operations graph (convolutions, etc).
        This function will hence work at training time and inference time.

        In this example, we assume that we have an input 12 bits multispectral image with values ranging from
        [0, 10000], that we process using a simple stretch to roughly match the [0, 1] range.

        :param inputs: dict of inputs
        :return: dict of normalized inputs, ready to be used from the `get_outputs()` function of the model
        """
        return {"input_xs": inputs["input_xs"] * 0.0001}

    def get_outputs(self, normalized_inputs):
        """
        This small model produces an output which has the same physical spacing as the input.
        The model generates [1 x 1 x N_CLASSES] output pixel for [32 x 32 x <nb channels>] input pixels.

        :param normalized_inputs: dict of normalized inputs`
        :return: activation values
        """

        # Model input
        norm_inp = normalized_inputs["input_xs"]

        # Encoder
        def _conv(inp, depth, name):
            return layers.Conv2D(filters=depth, kernel_size=3, activation="relu", name=name)(inp)

        def _tconv(inp, depth, name, activation="relu"):
            return layers.Conv2DTranspose(filters=depth, kernel_size=3, activation=activation, name=name)(inp)

        out_conv1 = _conv(norm_inp, 16, "conv1")
        out_conv2 = _conv(out_conv1, 32, "conv2")
        out_conv3 = _conv(out_conv2, 64, "conv3")
        out_conv4 = _conv(out_conv3, 64, "conv4")
        out_tconv1 = _tconv(out_conv4, 64, "tconv1") + out_conv3
        out_tconv2 = _tconv(out_tconv1, 32, "tconv2") + out_conv2
        out_tconv3 = _tconv(out_tconv2, 16, "tconv3") + out_conv1
        out_tconv4 = _tconv(out_tconv3, N_CLASSES, "classifier", None)

        # final layers
        net = tf.keras.activations.softmax(out_tconv4)
        net = tf.keras.layers.Cropping2D(cropping=32, name="predictions_softmax_tensor")(net)

        return {"predictions": net}


def dataset_preprocessing_fn(examples):
    """
    Preprocessing function for the training dataset.
    This function is only used at training time, to put the data in the expected format.
    DO NOT USE THIS FUNCTION TO NORMALIZE THE INPUTS ! (see `otbtf.ModelBase.normalize_inputs` for that).
    Note that this function is not called here, but in the code that prepares the datasets.

    :param examples: dict for examples (i.e. inputs and targets stored in a single dict)
    :return: preprocessed examples
    """

    def _to_categorical(x):
        return tf.one_hot(tf.squeeze(x, axis=-1), depth=N_CLASSES)

    return {"input_xs": examples["input_xs"],
            "predictions": _to_categorical(examples["labels"])}


def train(params, ds_train, ds_valid, ds_test):
    """
    Create, train, and save the model.

    :param params: contains batch_size, learning_rate, nb_epochs, and model_dir
    :param ds_train: training dataset
    :param ds_valid: validation dataset
    :param ds_test: testing dataset
    """

    strategy = tf.distribute.MirroredStrategy()  # For single or multi-GPUs
    with strategy.scope():
        # Model instantiation. Note that the normalize_fn is now part of the model
        model = FCNNModel(dataset_element_spec=ds_train.element_spec)

        # Compile the model
        model.compile(loss=tf.keras.losses.CategoricalCrossentropy(),
                      optimizer=tf.keras.optimizers.Adam(learning_rate=params.learning_rate),
                      metrics=[tf.keras.metrics.Precision(), tf.keras.metrics.Recall()])

        # Summarize the model (in CLI)
        model.summary()

        # Train
        model.fit(ds_train, epochs=params.nb_epochs, validation_data=ds_valid)

        # Evaluate against test data
        if ds_test is not None:
            model.evaluate(ds_test, batch_size=params.batch_size)

        # Save trained model as SavedModel
        model.save(params.model_dir)
