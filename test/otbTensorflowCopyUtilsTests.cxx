/*=========================================================================

     Copyright (c) 2018-2019 IRSTEA
     Copyright (c) 2020-2020 INRAE


     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "otbTensorflowCopyUtils.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "itkMacro.h"

int compare(tensorflow::Tensor & t1, tensorflow::Tensor & t2)
{
  std::cout << "Compare " << t1.DebugString() << " and " << t2.DebugString() << std::endl;
  if (t1.dims() != t2.dims())
    {
    std::cout << "dims() differ!" << std::endl;
    return EXIT_FAILURE;
    }
  if (t1.dtype() != t2.dtype())
    {
    std::cout << "dtype() differ!" << std::endl;
    return EXIT_FAILURE;
    }
  if (t1.NumElements() != t2.NumElements())
    {
    std::cout << "NumElements() differ!" << std::endl;
    return EXIT_FAILURE;
    }
  for (unsigned int i = 0; i < t1.NumElements(); i++)
    if (t1.scalar<float>()(i) != t2.scalar<float>()(i))
      {
      std::cout << "scalar " << i << " differ!" << std::endl;
      return EXIT_FAILURE;
      }
  // Else
  std::cout << "Tensors are equals :)" << std::endl;
  return EXIT_SUCCESS;
}

template<typename T>
int genericValueToTensorTest(tensorflow::DataType dt, std::string & expr, T & value)
{
  tensorflow::Tensor t = otb::tf::ValueToTensor(expr);
  tensorflow::Tensor t_ref(dt, tensorflow::TensorShape({}));
  t_ref.scalar<T>()() = value;

  return compare(t, t_ref);
}

int floatValueToTensorTest(int itkNotUsed(argc), char * itkNotUsed(argv)[])
{
  return genericValueToTensorTest<float>(tensorflow::DT_FLOAT, "0.1234", 0.1234);
}

