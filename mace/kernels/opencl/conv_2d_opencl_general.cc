//
// Copyright (c) 2017 XiaoMi All rights reserved.
//

#include "mace/core/common.h"
#include "mace/core/runtime/opencl/opencl_runtime.h"
#include "mace/kernels/conv_2d.h"
#include "mace/kernels/opencl/helper.h"
#include "mace/utils/utils.h"

namespace mace {
namespace kernels {

void Conv2dOpencl(const Tensor *input, const Tensor *filter,
                  const Tensor *bias, const bool fused_relu,
                  const uint32_t stride, const int *padding,
                  const DataType dt, Tensor *output) {
  const index_t batch = output->dim(0);
  const index_t height = output->dim(1);
  const index_t width = output->dim(2);
  const index_t channels = output->dim(3);
  const index_t input_channels = input->dim(3);

  const index_t channel_blocks = RoundUpDiv4(channels);
  const index_t input_channel_blocks = RoundUpDiv4(input_channels);
  const index_t width_blocks = RoundUpDiv4(width);

  std::set<std::string> built_options;
  built_options.emplace("-DDATA_TYPE=" + DtToUpstreamCLDt(dt));
  built_options.emplace("-DCMD_DATA_TYPE=" + DtToUpstreamCLCMDDt(dt));
  built_options.emplace(bias != nullptr ? "-DBIAS" : "");
  built_options.emplace("-DSTRIDE=" + ToString(stride));
  if (fused_relu) {
    built_options.emplace("-DFUSED_RELU");
  }

  auto runtime = OpenCLRuntime::Get();
  auto program = runtime->program();

  auto conv_2d_kernel = runtime->BuildKernel("conv_2d", "conv_2d", built_options);
  const uint32_t kwg_size = runtime->GetKernelMaxWorkGroupSize(conv_2d_kernel);

  uint32_t idx = 0;
  conv_2d_kernel.setArg(idx++, *(static_cast<const cl::Image2D *>(input->buffer())));
  conv_2d_kernel.setArg(idx++, *(static_cast<const cl::Image2D *>(filter->buffer())));
  if (bias != nullptr) {
    conv_2d_kernel.setArg(idx++, *(static_cast<const cl::Image2D *>(bias->buffer())));
  }
  conv_2d_kernel.setArg(idx++, *(static_cast<const cl::Image2D *>(output->buffer())));
  conv_2d_kernel.setArg(idx++, static_cast<int>(input->dim(1)));
  conv_2d_kernel.setArg(idx++, static_cast<int>(input->dim(2)));
  conv_2d_kernel.setArg(idx++, static_cast<int>(input_channel_blocks));
  conv_2d_kernel.setArg(idx++, static_cast<int>(height));
  conv_2d_kernel.setArg(idx++, static_cast<int>(width));
  conv_2d_kernel.setArg(idx++, static_cast<int>(filter->dim(0)));
  conv_2d_kernel.setArg(idx++, static_cast<int>(filter->dim(1)));
  conv_2d_kernel.setArg(idx++, padding[0] / 2);
  conv_2d_kernel.setArg(idx++, padding[1] / 2);

  auto command_queue = runtime->command_queue();
  cl_int error;
  error = command_queue.enqueueNDRangeKernel(
      conv_2d_kernel, cl::NullRange,
      cl::NDRange(static_cast<uint32_t>(channel_blocks), static_cast<uint32_t>(width_blocks),
                  static_cast<uint32_t>(height * batch)),
      cl::NDRange(16, 16, 4),
      NULL, OpenCLRuntime::Get()->GetDefaultEvent());
  MACE_CHECK(error == CL_SUCCESS, error);

}

}  // namespace kernels
}  // namespace mace