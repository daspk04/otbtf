"""
This example shows how to use the otbtf python API to train a deep net from patches-images.
"""
import helper
from otbtf import DatasetFromPatchesImages
import fcnn_model

parser = helper.base_parser()
parser.add_argument("--train_xs", required=True, nargs="+", default=[],
                    help="A list of patches-images for the XS image (training dataset)")
parser.add_argument("--train_labels", required=True, nargs="+", default=[],
                    help="A list of patches-images for the labels (training dataset)")
parser.add_argument("--valid_xs", required=True, nargs="+", default=[],
                    help="A list of patches-images for the XS image (validation dataset)")
parser.add_argument("--valid_labels", required=True, nargs="+", default=[],
                    help="A list of patches-images for the labels (validation dataset)")
parser.add_argument("--test_xs", required=False, nargs="+", default=[],
                    help="A list of patches-images for the XS image (test dataset)")
parser.add_argument("--test_labels", required=False, nargs="+", default=[],
                    help="A list of patches-images for the labels (test dataset)")


def create_dataset(xs_filenames, labels_filenames, targets_keys=["predictions"]):
    """
    Returns a TF dataset generated from an `otbtf.DatasetFromPatchesImages` instance
    """
    # Sort patches and labels
    xs_filenames.sort()
    labels_filenames.sort()

    # Check patches and labels are correctly sorted
    helper.check_files_order(xs_filenames, labels_filenames)

    # Create dataset from the filename dict
    # You can add the `use_streaming` option here, is you want to lower the memory budget.
    # However, this can slow down your process since the patches are read on-the-fly on the filesystem.
    # Good when one batch computation is slower than one batch gathering!
    # You can also use a custom `Iterator` of your own (default is `RandomIterator`). See `otbtf.dataset.Iterator`.
    ds = DatasetFromPatchesImages(filenames_dict={"input_xs": xs_filenames, "labels": labels_filenames})

    # We generate the TF dataset, and we use a preprocessing option to put the labels into one hot encoding (see the
    # `fcnn_model.dataset_preprocessing_fn` function). Also, we set the `target_keys` parameter to ask the dataset to
    # deliver samples in the form expected by keras, i.e. a tuple of dicts (inputs_dict, target_dict).
    tf_ds = ds.get_tf_dataset(batch_size=params.batch_size, preprocessing_fn=fcnn_model.dataset_preprocessing_fn,
                              targets_keys=targets_keys)

    return tf_ds


if __name__ == "__main__":
    params = parser.parse_args()

    # Create TF datasets
    ds_train = create_dataset(params.train_xs, params.train_labels)
    ds_valid = create_dataset(params.valid_xs, params.valid_labels)
    ds_test = create_dataset(params.test_xs, params.test_labels) if params.test_xs else None

    # Train the model
    fcnn_model.train(params, ds_train, ds_valid, ds_test)
