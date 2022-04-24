/*=========================================================================

     Copyright (c) 2018-2019 IRSTEA
     Copyright (c) 2020-2021 INRAE


     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "itkFixedArray.h"
#include "itkObjectFactory.h"
#include "otbWrapperApplicationFactory.h"

// Application engine
#include "otbStandardFilterWatcher.h"
#include "itkFixedArray.h"

// Tensorflow SavedModel
#include "tensorflow/cc/saved_model/loader.h"

// Tensorflow model train
#include "otbTensorflowMultisourceModelTrain.h"
#include "otbTensorflowMultisourceModelValidate.h"

// Tensorflow graph load
#include "otbTensorflowGraphOperations.h"

// Layerstack
#include "otbTensorflowSource.h"

// Metrics
#include "otbConfusionMatrixMeasurements.h"

namespace otb
{

namespace Wrapper
{

class TensorflowModelTrain : public Application
{
public:

  /** Standard class typedefs. */
  typedef TensorflowModelTrain                       Self;
  typedef Application                                Superclass;
  typedef itk::SmartPointer<Self>                    Pointer;
  typedef itk::SmartPointer<const Self>              ConstPointer;

  /** Standard macro */
  itkNewMacro(Self);
  itkTypeMacro(TensorflowModelTrain, Application);

  /** Typedefs for TensorFlow */
  typedef otb::TensorflowMultisourceModelTrain<FloatVectorImageType>    TrainModelFilterType;
  typedef otb::TensorflowMultisourceModelValidate<FloatVectorImageType> ValidateModelFilterType;
  typedef otb::TensorflowSource<FloatVectorImageType>                   TFSource;

  /* Typedefs for evaluation metrics */
  typedef ValidateModelFilterType::ConfMatType                          ConfMatType;
  typedef ValidateModelFilterType::MapOfClassesType                     MapOfClassesType;
  typedef ValidateModelFilterType::LabelValueType                       LabelValueType;
  typedef otb::ConfusionMatrixMeasurements<ConfMatType, LabelValueType> ConfusionMatrixCalculatorType;

  //
  // Store stuff related to one source
  //
  struct ProcessObjectsBundle
  {
    TFSource tfSource;
    TFSource tfSourceForValidation;

    // Parameters keys
    std::string m_KeyInForTrain;     // Key of input image list (training)
    std::string m_KeyInForValid;     // Key of input image list (validation)
    std::string m_KeyPHNameForTrain; // Key for placeholder name in the TensorFlow model (training)
    std::string m_KeyPHNameForValid; // Key for placeholder name in the TensorFlow model (validation)
    std::string m_KeyPszX;   // Key for samples sizes X
    std::string m_KeyPszY;   // Key for samples sizes Y
  };

  /** Typedefs for the app */
  typedef std::vector<ProcessObjectsBundle>           BundleList;
  typedef std::vector<FloatVectorImageType::SizeType> SizeList;
  typedef std::vector<std::string>                    StringList;

  void DoUpdateParameters()
  {
  }

  //
  // Add an input source, which includes:
  // -an input image list        (for training)
  // -an input image placeholder (for training)
  // -an input image list        (for validation)
  // -an input image placeholder (for validation)
  // -an input patchsize, which is the dimensions of samples. Same for training and validation.
  //
  void AddAnInputImage()
  {
    // Number of source
    unsigned int inputNumber = m_Bundles.size() + 1;

    // Create keys and descriptions
    std::stringstream ss_key_tr_group, ss_desc_tr_group,
    ss_key_val_group, ss_desc_val_group,
    ss_key_tr_in, ss_desc_tr_in,
    ss_key_val_in, ss_desc_val_in,
    ss_key_dims_x, ss_desc_dims_x,
    ss_key_dims_y, ss_desc_dims_y,
    ss_key_tr_ph, ss_desc_tr_ph,
    ss_key_val_ph, ss_desc_val_ph;

    // Parameter group key/description
    ss_key_tr_group   << "training.source"         << inputNumber;
    ss_key_val_group  << "validation.source"       << inputNumber;
    ss_desc_tr_group  << "Parameters for source #" << inputNumber << " (training)";
    ss_desc_val_group << "Parameters for source #" << inputNumber << " (validation)";

    // Parameter group keys
    ss_key_tr_in   << ss_key_tr_group.str()  << ".il";
    ss_key_val_in  << ss_key_val_group.str() << ".il";
    ss_key_dims_x  << ss_key_tr_group.str()  << ".patchsizex";
    ss_key_dims_y  << ss_key_tr_group.str()  << ".patchsizey";
    ss_key_tr_ph   << ss_key_tr_group.str()  << ".placeholder";
    ss_key_val_ph  << ss_key_val_group.str() << ".name";

    // Parameter group descriptions
    ss_desc_tr_in  << "Input image (or list to stack) for source #" << inputNumber << " (training)";
    ss_desc_val_in << "Input image (or list to stack) for source #" << inputNumber << " (validation)";
    ss_desc_dims_x << "Patch size (x) for source #"                 << inputNumber;
    ss_desc_dims_y << "Patch size (y) for source #"                 << inputNumber;
    ss_desc_tr_ph  << "Name of the input placeholder for source #"  << inputNumber << " (training)";
    ss_desc_val_ph << "Name of the input placeholder "
        "or output tensor for source #"                             << inputNumber << " (validation)";

    // Populate group
    AddParameter(ParameterType_Group,          ss_key_tr_group.str(),  ss_desc_tr_group.str());
    AddParameter(ParameterType_InputImageList, ss_key_tr_in.str(),     ss_desc_tr_in.str() );
    AddParameter(ParameterType_Int,            ss_key_dims_x.str(),    ss_desc_dims_x.str());
    SetMinimumParameterIntValue               (ss_key_dims_x.str(),    1);
    AddParameter(ParameterType_Int,            ss_key_dims_y.str(),    ss_desc_dims_y.str());
    SetMinimumParameterIntValue               (ss_key_dims_y.str(),    1);
    AddParameter(ParameterType_String,         ss_key_tr_ph.str(),     ss_desc_tr_ph.str());
    AddParameter(ParameterType_Group,          ss_key_val_group.str(), ss_desc_val_group.str());
    AddParameter(ParameterType_InputImageList, ss_key_val_in.str(),    ss_desc_val_in.str() );
    AddParameter(ParameterType_String,         ss_key_val_ph.str(),    ss_desc_val_ph.str());

    // Add a new bundle
    ProcessObjectsBundle bundle;
    bundle.m_KeyInForTrain     = ss_key_tr_in.str();
    bundle.m_KeyInForValid     = ss_key_val_in.str();
    bundle.m_KeyPHNameForTrain = ss_key_tr_ph.str();
    bundle.m_KeyPHNameForValid = ss_key_val_ph.str();
    bundle.m_KeyPszX           = ss_key_dims_x.str();
    bundle.m_KeyPszY           = ss_key_dims_y.str();

    m_Bundles.push_back(bundle);
  }

  void DoInit()
  {

    // Documentation
    SetName("TensorflowModelTrain");
    SetDescription("Train a multisource deep learning net using Tensorflow. Change "
        "the " + tf::ENV_VAR_NAME_NSOURCES + " environment variable to set the number of "
        "sources.");
    SetDocLongDescription("The application trains a Tensorflow model over multiple data sources. "
        "The number of input sources can be changed at runtime by setting the "
        "system environment variable " + tf::ENV_VAR_NAME_NSOURCES + ". "
        "For each source, you have to set (1) the tensor placeholder name, as named in "
        "the tensorflow model, (2) the patch size and (3) the image(s) source. ");
    SetDocAuthors("Remi Cresson");

    AddDocTag(Tags::Learning);

    // Input model
    AddParameter(ParameterType_Group,       "model",              "Model parameters");
    AddParameter(ParameterType_Directory,   "model.dir",          "Tensorflow model_save directory");
    MandatoryOn                            ("model.dir");
    AddParameter(ParameterType_String,      "model.restorefrom",  "Restore model from path");
    MandatoryOff                           ("model.restorefrom");
    AddParameter(ParameterType_String,      "model.saveto",       "Save model to path");
    MandatoryOff                           ("model.saveto");
    AddParameter(ParameterType_StringList,  "model.tagsets",    "Which tags (i.e. v1.MetaGraphDefs) to load from the saved model. Currently, only one tag is supported. Can be retrieved by running `saved_model_cli  show --dir your_model_dir --all`");
    MandatoryOff                           ("model.tagsets");

    // Training parameters group
    AddParameter(ParameterType_Group,       "training",           "Training parameters");
    AddParameter(ParameterType_Int,         "training.batchsize", "Batch size");
    SetMinimumParameterIntValue            ("training.batchsize", 1);
    SetDefaultParameterInt                 ("training.batchsize", 100);
    AddParameter(ParameterType_Int,         "training.epochs",    "Number of epochs");
    SetMinimumParameterIntValue            ("training.epochs",    1);
    SetDefaultParameterInt                 ("training.epochs",    100);
    AddParameter(ParameterType_StringList,  "training.userplaceholders",
                 "Additional single-valued placeholders for training. Supported types: int, float, bool.");
    MandatoryOff                           ("training.userplaceholders");
    AddParameter(ParameterType_StringList,  "training.targetnodes",    "Names of the target nodes");
    MandatoryOn                            ("training.targetnodes");
    AddParameter(ParameterType_StringList,  "training.outputtensors",  "Names of the output tensors to display");
    MandatoryOff                           ("training.outputtensors");
    AddParameter(ParameterType_Bool,        "training.usestreaming",   "Use the streaming through patches (slower but can process big dataset)");
    MandatoryOff                           ("training.usestreaming");

    // Metrics
    AddParameter(ParameterType_Group,       "validation",              "Validation parameters");
    MandatoryOff                           ("validation");
    AddParameter(ParameterType_Int,         "validation.step",         "Perform the validation every Nth epochs");
    SetMinimumParameterIntValue            ("validation.step",         1);
    SetDefaultParameterInt                 ("validation.step",         10);
    AddParameter(ParameterType_Choice,      "validation.mode",         "Metrics to compute");
    AddChoice                              ("validation.mode.none",    "No validation step");
    AddChoice                              ("validation.mode.class",   "Classification metrics");
    AddChoice                              ("validation.mode.rmse",    "Root mean square error");
    AddParameter(ParameterType_StringList,  "validation.userplaceholders",
                 "Additional single-valued placeholders for validation. Supported types: int, float, bool.");
    MandatoryOff                           ("validation.userplaceholders");
    AddParameter(ParameterType_Bool,        "validation.usestreaming", "Use the streaming through patches (slower but can process big dataset)");
    MandatoryOff                           ("validation.usestreaming");

    // Input/output images
    AddAnInputImage();
    for (int i = 1; i < tf::GetNumberOfSources() + 1 ; i++) // +1 because we have at least 1 source more for training
      {
      AddAnInputImage();
      }

    // Example
    SetDocExampleParameterValue("source1.il",                "spot6pms.tif");
    SetDocExampleParameterValue("source1.placeholder",       "x1");
    SetDocExampleParameterValue("source1.patchsizex",        "16");
    SetDocExampleParameterValue("source1.patchsizey",        "16");
    SetDocExampleParameterValue("source2.il",                "labels.tif");
    SetDocExampleParameterValue("source2.placeholder",       "y1");
    SetDocExampleParameterValue("source2.patchsizex",        "1");
    SetDocExampleParameterValue("source2.patchsizex",        "1");
    SetDocExampleParameterValue("model.dir",                 "/tmp/my_saved_model/");
    SetDocExampleParameterValue("training.userplaceholders", "is_training=true dropout=0.2");
    SetDocExampleParameterValue("training.targetnodes",      "optimizer");
    SetDocExampleParameterValue("model.saveto",              "/tmp/my_saved_model/variables/variables");

  }

  //
  // Prepare bundles
  // Here, we populate the two following groups:
  // 1.Training :
  //   -Placeholders
  //   -PatchSize
  //   -ImageSource
  // 2.Learning/Validation
  //   -Placeholders (if input) or Tensor name (if target)
  //   -PatchSize (which is the same as for training)
  //   -ImageSource (depending if it's for learning or validation)
  //
  // TODO: a bit of refactoring. We could simply rely on m_Bundles
  //       if we can keep trace of indices of sources for
  //       training / test / validation
  //
  void PrepareInputs()
  {
    // Clear placeholder names
    m_InputPlaceholdersForTraining.clear();
    m_InputPlaceholdersForValidation.clear();

    // Clear patches sizes
    m_InputPatchesSizeForTraining.clear();
    m_InputPatchesSizeForValidation.clear();
    m_TargetPatchesSize.clear();

    // Clear bundles
    m_InputSourcesForTraining.clear();
    m_InputSourcesForEvaluationAgainstLearningData.clear();
    m_InputSourcesForEvaluationAgainstValidationData.clear();

    m_TargetTensorsNames.clear();
    m_InputTargetsForEvaluationAgainstValidationData.clear();
    m_InputTargetsForEvaluationAgainstLearningData.clear();


    // Prepare the bundles
    for (auto& bundle: m_Bundles)
      {
      // Source
      FloatVectorImageListType::Pointer trainStack = GetParameterImageList(bundle.m_KeyInForTrain);
      bundle.tfSource.Set(trainStack);
      m_InputSourcesForTraining.push_back(bundle.tfSource.Get());

      // Placeholder
      std::string placeholderForTraining = GetParameterAsString(bundle.m_KeyPHNameForTrain);
      m_InputPlaceholdersForTraining.push_back(placeholderForTraining);

      // Patch size
      FloatVectorImageType::SizeType patchSize;
      patchSize[0] = GetParameterInt(bundle.m_KeyPszX);
      patchSize[1] = GetParameterInt(bundle.m_KeyPszY);
      m_InputPatchesSizeForTraining.push_back(patchSize);

      otbAppLogINFO("New source:");
      otbAppLogINFO("Patch size               : "<< patchSize);
      otbAppLogINFO("Placeholder (training)   : "<< placeholderForTraining);

      // Prepare validation sources
      if (GetParameterInt("validation.mode") != 0)
        {
        // Get the stack
        if (!HasValue(bundle.m_KeyInForValid))
          {
          otbAppLogFATAL("No validation input is set for this source");
          }
        FloatVectorImageListType::Pointer validStack = GetParameterImageList(bundle.m_KeyInForValid);
        bundle.tfSourceForValidation.Set(validStack);

        // We check if the placeholder is the same for training and for validation
        // If yes, it means that its not an output tensor on which perform the validation
        std::string placeholderForValidation = GetParameterAsString(bundle.m_KeyPHNameForValid);
        if (placeholderForValidation.empty())
          {
          placeholderForValidation = placeholderForTraining;
          }
        // Same placeholder name ==> is a source for validation
        if (placeholderForValidation.compare(placeholderForTraining) == 0)
          {
          // Source
          m_InputSourcesForEvaluationAgainstValidationData.push_back(bundle.tfSourceForValidation.Get());
          m_InputSourcesForEvaluationAgainstLearningData.push_back(bundle.tfSource.Get());

          // Placeholder
          m_InputPlaceholdersForValidation.push_back(placeholderForValidation);

          // Patch size
          m_InputPatchesSizeForValidation.push_back(patchSize);

          otbAppLogINFO("Placeholder (validation) : "<< placeholderForValidation);

          }
        // Different placeholder ==> is a target to validate
        else
          {
          // Source
          m_InputTargetsForEvaluationAgainstValidationData.push_back(bundle.tfSourceForValidation.Get());
          m_InputTargetsForEvaluationAgainstLearningData.push_back(bundle.tfSource.Get());

          // Placeholder
          m_TargetTensorsNames.push_back(placeholderForValidation);

          // Patch size
          m_TargetPatchesSize.push_back(patchSize);

          otbAppLogINFO("Tensor name (validation) : "<< placeholderForValidation);
          }

        }

      }
  }

  //
  // Get user placeholders
  //
  TrainModelFilterType::DictType GetUserPlaceholders(const std::string & key)
  {
    TrainModelFilterType::DictType dict;
    TrainModelFilterType::StringList expressions = GetParameterStringList(key);
    for (auto& exp: expressions)
      {
      TrainModelFilterType::DictElementType entry = tf::ExpressionToTensor(exp);
      dict.push_back(entry);

      otbAppLogINFO("Using placeholder " << entry.first << " with " << tf::PrintTensorInfos(entry.second));
      }
    return dict;
  }

  //
  // Print some classification metrics
  //
  void PrintClassificationMetrics(const ConfMatType & confMat, const MapOfClassesType & mapOfClassesRef)
  {
    ConfusionMatrixCalculatorType::Pointer confMatMeasurements = ConfusionMatrixCalculatorType::New();
    confMatMeasurements->SetConfusionMatrix(confMat);
    confMatMeasurements->SetMapOfClasses(mapOfClassesRef);
    confMatMeasurements->Compute();

    for (auto const& itMapOfClassesRef : mapOfClassesRef)
      {
      LabelValueType labelRef = itMapOfClassesRef.first;
      LabelValueType indexLabelRef = itMapOfClassesRef.second;

      otbAppLogINFO("Precision of class [" << labelRef << "] vs all: " << confMatMeasurements->GetPrecisions()[indexLabelRef]);
      otbAppLogINFO("Recall of class [" << labelRef << "] vs all: " << confMatMeasurements->GetRecalls()[indexLabelRef]);
      otbAppLogINFO("F-score of class [" << labelRef << "] vs all: " << confMatMeasurements->GetFScores()[indexLabelRef]);
      otbAppLogINFO("\t");
      }
    otbAppLogINFO("Precision of the different classes: " << confMatMeasurements->GetPrecisions());
    otbAppLogINFO("Recall of the different classes: " << confMatMeasurements->GetRecalls());
    otbAppLogINFO("F-score of the different classes: " << confMatMeasurements->GetFScores());
    otbAppLogINFO("\t");
    otbAppLogINFO("Kappa index: " << confMatMeasurements->GetKappaIndex());
    otbAppLogINFO("Overall accuracy index: " << confMatMeasurements->GetOverallAccuracy());
    otbAppLogINFO("Confusion matrix:\n" << confMat);
  }

  void DoExecute()
  {

    // Load the Tensorflow bundle
    tf::LoadModel(GetParameterAsString("model.dir"), m_SavedModel, GetParameterStringList("model.tagsets"));

    // Check if we have to restore variables from somewhere else
    if (HasValue("model.restorefrom"))
      {
      const std::string path = GetParameterAsString("model.restorefrom");
      otbAppLogINFO("Restoring model from " + path);

      // Load SavedModel variables
      tf::RestoreModel(path, m_SavedModel);
      }

    // Prepare inputs
    PrepareInputs();

    // Setup training filter
    m_TrainModelFilter = TrainModelFilterType::New();
    m_TrainModelFilter->SetSavedModel(&m_SavedModel);
    m_TrainModelFilter->SetOutputTensors(GetParameterStringList("training.outputtensors"));
    m_TrainModelFilter->SetTargetNodesNames(GetParameterStringList("training.targetnodes"));
    m_TrainModelFilter->SetBatchSize(GetParameterInt("training.batchsize"));
    m_TrainModelFilter->SetUserPlaceholders(GetUserPlaceholders("training.userplaceholders"));
    m_TrainModelFilter->SetUseStreaming(GetParameterInt("training.usestreaming"));

    // Set inputs
    for (unsigned int i = 0 ; i < m_InputSourcesForTraining.size() ; i++)
      {
      m_TrainModelFilter->PushBackInputTensorBundle(
          m_InputPlaceholdersForTraining[i],
          m_InputPatchesSizeForTraining[i],
          m_InputSourcesForTraining[i]);
      }

    // Setup the validation filter
    const bool do_validation = HasUserValue("validation.mode");
    if (GetParameterInt("validation.mode")==1) // class
      {
      otbAppLogINFO("Set validation mode to classification validation");

      m_ValidateModelFilter = ValidateModelFilterType::New();
      m_ValidateModelFilter->SetSavedModel(&m_SavedModel);
      m_ValidateModelFilter->SetBatchSize(GetParameterInt("training.batchsize"));
      m_ValidateModelFilter->SetUserPlaceholders(GetUserPlaceholders("validation.userplaceholders"));
      m_ValidateModelFilter->SetInputPlaceholders(m_InputPlaceholdersForValidation);
      m_ValidateModelFilter->SetInputReceptiveFields(m_InputPatchesSizeForValidation);
      m_ValidateModelFilter->SetOutputTensors(m_TargetTensorsNames);
      m_ValidateModelFilter->SetOutputExpressionFields(m_TargetPatchesSize);
      }
    else if (GetParameterInt("validation.mode")==2) // rmse)
      {
      otbAppLogINFO("Set validation mode to classification RMSE evaluation");
      otbAppLogFATAL("Not implemented yet !"); // XD

      // TODO
      }

    // Epoch
    for (int epoch = 1 ; epoch <= GetParameterInt("training.epochs") ; epoch++)
      {
      // Train the model
      AddProcess(m_TrainModelFilter, "Training epoch #" + std::to_string(epoch));
      m_TrainModelFilter->Update();

      if (do_validation)
      {
        // Validate the model
        if (epoch % GetParameterInt("validation.step") == 0)
        {
          // 1. Evaluate the metrics against the learning data

          for (unsigned int i = 0 ; i < m_InputSourcesForEvaluationAgainstLearningData.size() ; i++)
          {
            m_ValidateModelFilter->SetInput(i, m_InputSourcesForEvaluationAgainstLearningData[i]);
          }
          m_ValidateModelFilter->SetInputReferences(m_InputTargetsForEvaluationAgainstLearningData);

          // As we use the learning data here, it's rational to use the same option as streaming during training
          m_ValidateModelFilter->SetUseStreaming(GetParameterInt("training.usestreaming"));

          // Update
          AddProcess(m_ValidateModelFilter, "Evaluate model (Learning data)");
          m_ValidateModelFilter->Update();

          for (unsigned int i = 0 ; i < m_TargetTensorsNames.size() ; i++)
          {
            otbAppLogINFO("Metrics for target \"" << m_TargetTensorsNames[i] << "\":");
            PrintClassificationMetrics(m_ValidateModelFilter->GetConfusionMatrix(i), m_ValidateModelFilter->GetMapOfClasses(i));
          }

          // 2. Evaluate the metrics against the validation data

          // Here we just change the input sources and references
          for (unsigned int i = 0 ; i < m_InputSourcesForEvaluationAgainstValidationData.size() ; i++)
          {
            m_ValidateModelFilter->SetInput(i, m_InputSourcesForEvaluationAgainstValidationData[i]);
          }
          m_ValidateModelFilter->SetInputReferences(m_InputTargetsForEvaluationAgainstValidationData);
          m_ValidateModelFilter->SetUseStreaming(GetParameterInt("validation.usestreaming"));

          // Update
          AddProcess(m_ValidateModelFilter, "Evaluate model (Validation data)");
          m_ValidateModelFilter->Update();

          for (unsigned int i = 0 ; i < m_TargetTensorsNames.size() ; i++)
          {
            otbAppLogINFO("Metrics for target \"" << m_TargetTensorsNames[i] << "\":");
            PrintClassificationMetrics(m_ValidateModelFilter->GetConfusionMatrix(i), m_ValidateModelFilter->GetMapOfClasses(i));
          }
        } // Step is OK to perform validation
      } // Do the validation against the validation data

      } // Next epoch

    // Check if we have to save variables to somewhere
    if (HasValue("model.saveto"))
      {
      const std::string path = GetParameterAsString("model.saveto");
      otbAppLogINFO("Saving model to " + path);
      tf::SaveModel(path, m_SavedModel);
      }

  }

private:

  tensorflow::SavedModelBundle     m_SavedModel; // must be alive during all the execution of the application !

  // Filters
  TrainModelFilterType::Pointer    m_TrainModelFilter;
  ValidateModelFilterType::Pointer m_ValidateModelFilter;

  // Inputs
  BundleList m_Bundles;

  // Patches size
  SizeList   m_InputPatchesSizeForTraining;
  SizeList   m_InputPatchesSizeForValidation;
  SizeList   m_TargetPatchesSize;

  // Placeholders and Tensors names
  StringList m_InputPlaceholdersForTraining;
  StringList m_InputPlaceholdersForValidation;
  StringList m_TargetTensorsNames;

  // Image sources
  std::vector<FloatVectorImageType::Pointer> m_InputSourcesForTraining;
  std::vector<FloatVectorImageType::Pointer> m_InputSourcesForEvaluationAgainstLearningData;
  std::vector<FloatVectorImageType::Pointer> m_InputSourcesForEvaluationAgainstValidationData;
  std::vector<FloatVectorImageType::Pointer> m_InputTargetsForEvaluationAgainstLearningData;
  std::vector<FloatVectorImageType::Pointer> m_InputTargetsForEvaluationAgainstValidationData;

}; // end of class

} // namespace wrapper
} // namespace otb

OTB_APPLICATION_EXPORT( otb::Wrapper::TensorflowModelTrain )
