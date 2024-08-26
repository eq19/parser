//===- LowerGpuOpsToNVVMOps.cpp - MLIR GPU to NVVM lowering passes --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass to generate NVVMIR operations for higher-level
// GPU operations.
//
//===----------------------------------------------------------------------===//

#include "mlir/Conversion/GPUToNVVM/GPUToNVVMPass.h"

#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/GPUCommon/GPUCommonPass.h"
#include "mlir/Conversion/LLVMCommon/ConversionTarget.h"
#include "mlir/Conversion/LLVMCommon/LoweringOptions.h"
#include "mlir/Conversion/LLVMCommon/TypeConverter.h"
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Conversion/VectorToLLVM/ConvertVectorToLLVM.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/GPU/Transforms/Passes.h"
#include "mlir/Dialect/LLVMIR/NVVMDialect.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "../GPUCommon/GPUOpsLowering.h"
#include "../GPUCommon/IndexIntrinsicsOpLowering.h"
#include "../GPUCommon/OpToFuncCallLowering.h"
#include <optional>

namespace mlir {
#define GEN_PASS_DEF_CONVERTGPUOPSTONVVMOPS
#include "mlir/Conversion/Passes.h.inc"
} // namespace mlir

using namespace mlir;

namespace {

/// Convert gpu dialect shfl mode enum to the equivalent nvvm one.
static NVVM::ShflKind convertShflKind(gpu::ShuffleMode mode) {
  switch (mode) {
  case gpu::ShuffleMode::XOR:
    return NVVM::ShflKind::bfly;
  case gpu::ShuffleMode::UP:
    return NVVM::ShflKind::up;
  case gpu::ShuffleMode::DOWN:
    return NVVM::ShflKind::down;
  case gpu::ShuffleMode::IDX:
    return NVVM::ShflKind::idx;
  }
  llvm_unreachable("unknown shuffle mode");
}

static std::optional<NVVM::ReduxKind>
convertReduxKind(gpu::AllReduceOperation mode) {
  switch (mode) {
  case gpu::AllReduceOperation::ADD:
    return NVVM::ReduxKind::ADD;
  case gpu::AllReduceOperation::MUL:
    return std::nullopt;
  case gpu::AllReduceOperation::MINSI:
    return NVVM::ReduxKind::MIN;
  case gpu::AllReduceOperation::MINUI:
    return std::nullopt;
  case gpu::AllReduceOperation::MINNUMF:
    return NVVM::ReduxKind::MIN;
  case gpu::AllReduceOperation::MAXSI:
    return NVVM::ReduxKind::MAX;
  case gpu::AllReduceOperation::MAXUI:
    return std::nullopt;
  case gpu::AllReduceOperation::MAXNUMF:
    return NVVM::ReduxKind::MAX;
  case gpu::AllReduceOperation::AND:
    return NVVM::ReduxKind::AND;
  case gpu::AllReduceOperation::OR:
    return NVVM::ReduxKind::OR;
  case gpu::AllReduceOperation::XOR:
    return NVVM::ReduxKind::XOR;
  case gpu::AllReduceOperation::MINIMUMF:
  case gpu::AllReduceOperation::MAXIMUMF:
    return std::nullopt;
  }
  return std::nullopt;
}

/// This pass lowers gpu.subgroup_reduce op into to the nvvm.redux op. The op
/// must be run by the entire subgroup, otherwise it is undefined behaviour.
struct GPUSubgroupReduceOpLowering
    : public ConvertOpToLLVMPattern<gpu::SubgroupReduceOp> {
  using ConvertOpToLLVMPattern<gpu::SubgroupReduceOp>::ConvertOpToLLVMPattern;
  LogicalResult

  matchAndRewrite(gpu::SubgroupReduceOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    if (op.getClusterSize())
      return rewriter.notifyMatchFailure(
          op, "lowering for clustered reduce not implemented");

    if (!op.getUniform())
      return rewriter.notifyMatchFailure(
          op, "cannot be lowered to redux as the op must be run "
              "uniformly (entire subgroup).");
    if (!op.getValue().getType().isInteger(32))
      return rewriter.notifyMatchFailure(op, "unsupported data type");

    std::optional<NVVM::ReduxKind> mode = convertReduxKind(op.getOp());
    if (!mode.has_value())
      return rewriter.notifyMatchFailure(
          op, "unsupported reduction mode for redux");

    Location loc = op->getLoc();
    auto int32Type = IntegerType::get(rewriter.getContext(), 32);
    Value offset = rewriter.create<LLVM::ConstantOp>(loc, int32Type, -1);

    auto reduxOp = rewriter.create<NVVM::ReduxOp>(loc, int32Type, op.getValue(),
                                                  mode.value(), offset);

    rewriter.replaceOp(op, reduxOp->getResult(0));
    return success();
  }
};

struct GPUShuffleOpLowering : public ConvertOpToLLVMPattern<gpu::ShuffleOp> {
  using ConvertOpToLLVMPattern<gpu::ShuffleOp>::ConvertOpToLLVMPattern;

  /// Lowers a shuffle to the corresponding NVVM op.
  ///
  /// Convert the `width` argument into an activeMask (a bitmask which specifies
  /// which threads participate in the shuffle) and a maskAndClamp (specifying
  /// the highest lane which participates in the shuffle).
  ///
  ///     %one = llvm.constant(1 : i32) : i32
  ///     %minus_one = llvm.constant(-1 : i32) : i32
  ///     %thirty_two = llvm.constant(32 : i32) : i32
  ///     %num_lanes = llvm.sub %thirty_two, %width : i32
  ///     %active_mask = llvm.lshr %minus_one, %num_lanes : i32
  ///     %mask_and_clamp = llvm.sub %width, %one : i32
  ///     %shfl = nvvm.shfl.sync.bfly %active_mask, %value, %offset,
  ///         %mask_and_clamp : !llvm<"{ float, i1 }">
  ///     %shfl_value = llvm.extractvalue %shfl[0] :
  ///         !llvm<"{ float, i1 }">
  ///     %shfl_pred = llvm.extractvalue %shfl[1] :
  ///         !llvm<"{ float, i1 }">
  LogicalResult
  matchAndRewrite(gpu::ShuffleOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    Location loc = op->getLoc();

    auto valueTy = adaptor.getValue().getType();
    auto int32Type = IntegerType::get(rewriter.getContext(), 32);
    auto predTy = IntegerType::get(rewriter.getContext(), 1);

    Value one = rewriter.create<LLVM::ConstantOp>(loc, int32Type, 1);
    Value minusOne = rewriter.create<LLVM::ConstantOp>(loc, int32Type, -1);
    Value thirtyTwo = rewriter.create<LLVM::ConstantOp>(loc, int32Type, 32);
    Value numLeadInactiveLane = rewriter.create<LLVM::SubOp>(
        loc, int32Type, thirtyTwo, adaptor.getWidth());
    // Bit mask of active lanes: `(-1) >> (32 - activeWidth)`.
    Value activeMask = rewriter.create<LLVM::LShrOp>(loc, int32Type, minusOne,
                                                     numLeadInactiveLane);
    Value maskAndClamp;
    if (op.getMode() == gpu::ShuffleMode::UP) {
      // Clamp lane: `32 - activeWidth`
      maskAndClamp = numLeadInactiveLane;
    } else {
      // Clamp lane: `activeWidth - 1`
      maskAndClamp =
          rewriter.create<LLVM::SubOp>(loc, int32Type, adaptor.getWidth(), one);
    }

    bool predIsUsed = !op->getResult(1).use_empty();
    UnitAttr returnValueAndIsValidAttr = nullptr;
    Type resultTy = valueTy;
    if (predIsUsed) {
      returnValueAndIsValidAttr = rewriter.getUnitAttr();
      resultTy = LLVM::LLVMStructType::getLiteral(rewriter.getContext(),
                                                  {valueTy, predTy});
    }
    Value shfl = rewriter.create<NVVM::ShflOp>(
        loc, resultTy, activeMask, adaptor.getValue(), adaptor.getOffset(),
        maskAndClamp, convertShflKind(op.getMode()), returnValueAndIsValidAttr);
    if (predIsUsed) {
      Value shflValue = rewriter.create<LLVM::ExtractValueOp>(loc, shfl, 0);
      Value isActiveSrcLane =
          rewriter.create<LLVM::ExtractValueOp>(loc, shfl, 1);
      rewriter.replaceOp(op, {shflValue, isActiveSrcLane});
    } else {
      rewriter.replaceOp(op, {shfl, nullptr});
    }
    return success();
  }
};

struct GPULaneIdOpToNVVM : ConvertOpToLLVMPattern<gpu::LaneIdOp> {
  using ConvertOpToLLVMPattern<gpu::LaneIdOp>::ConvertOpToLLVMPattern;

  LogicalResult
  matchAndRewrite(gpu::LaneIdOp op, gpu::LaneIdOp::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto loc = op->getLoc();
    MLIRContext *context = rewriter.getContext();
    Value newOp = rewriter.create<NVVM::LaneIdOp>(loc, rewriter.getI32Type());
    // Truncate or extend the result depending on the index bitwidth specified
    // by the LLVMTypeConverter options.
    const unsigned indexBitwidth = getTypeConverter()->getIndexTypeBitwidth();
    if (indexBitwidth > 32) {
      newOp = rewriter.create<LLVM::SExtOp>(
          loc, IntegerType::get(context, indexBitwidth), newOp);
    } else if (indexBitwidth < 32) {
      newOp = rewriter.create<LLVM::TruncOp>(
          loc, IntegerType::get(context, indexBitwidth), newOp);
    }
    rewriter.replaceOp(op, {newOp});
    return success();
  }
};

/// Import the GPU Ops to NVVM Patterns.
#include "GPUToNVVM.cpp.inc"

/// A pass that replaces all occurrences of GPU device operations with their
/// corresponding NVVM equivalent.
///
/// This pass only handles device code and is not meant to be run on GPU host
/// code.
struct LowerGpuOpsToNVVMOpsPass
    : public impl::ConvertGpuOpsToNVVMOpsBase<LowerGpuOpsToNVVMOpsPass> {
  using Base::Base;

  void runOnOperation() override {
    gpu::GPUModuleOp m = getOperation();

    // Request C wrapper emission.
    for (auto func : m.getOps<func::FuncOp>()) {
      func->setAttr(LLVM::LLVMDialect::getEmitCWrapperAttrName(),
                    UnitAttr::get(&getContext()));
    }

    // Customize the bitwidth used for the device side index computations.
    LowerToLLVMOptions options(
        m.getContext(),
        DataLayout(cast<DataLayoutOpInterface>(m.getOperation())));
    if (indexBitwidth != kDeriveIndexBitwidthFromDataLayout)
      options.overrideIndexBitwidth(indexBitwidth);
    options.useBarePtrCallConv = useBarePtrCallConv;

    // Apply in-dialect lowering. In-dialect lowering will replace
    // ops which need to be lowered further, which is not supported by a
    // single conversion pass.
    {
      RewritePatternSet patterns(m.getContext());
      populateGpuRewritePatterns(patterns);
      if (failed(applyPatternsAndFoldGreedily(m, std::move(patterns))))
        return signalPassFailure();
    }

    LLVMTypeConverter converter(m.getContext(), options);
    // NVVM uses alloca in the default address space to represent private
    // memory allocations, so drop private annotations. NVVM uses address
    // space 3 for shared memory. NVVM uses the default address space to
    // represent global memory.
    populateGpuMemorySpaceAttributeConversions(
        converter, [](gpu::AddressSpace space) -> unsigned {
          switch (space) {
          case gpu::AddressSpace::Global:
            return static_cast<unsigned>(
                NVVM::NVVMMemorySpace::kGlobalMemorySpace);
          case gpu::AddressSpace::Workgroup:
            return static_cast<unsigned>(
                NVVM::NVVMMemorySpace::kSharedMemorySpace);
          case gpu::AddressSpace::Private:
            return 0;
          }
          llvm_unreachable("unknown address space enum value");
          return 0;
        });
    // Lowering for MMAMatrixType.
    converter.addConversion([&](gpu::MMAMatrixType type) -> Type {
      return convertMMAToLLVMType(type);
    });
    RewritePatternSet llvmPatterns(m.getContext());

    arith::populateArithToLLVMConversionPatterns(converter, llvmPatterns);
    cf::populateControlFlowToLLVMConversionPatterns(converter, llvmPatterns);
    populateFuncToLLVMConversionPatterns(converter, llvmPatterns);
    populateFinalizeMemRefToLLVMConversionPatterns(converter, llvmPatterns);
    populateGpuToNVVMConversionPatterns(converter, llvmPatterns);
    populateGpuWMMAToNVVMConversionPatterns(converter, llvmPatterns);
    populateVectorToLLVMConversionPatterns(converter, llvmPatterns);
    if (this->hasRedux)
      populateGpuSubgroupReduceOpLoweringPattern(converter, llvmPatterns);
    LLVMConversionTarget target(getContext());
    configureGpuToNVVMConversionLegality(target);
    if (failed(applyPartialConversion(m, target, std::move(llvmPatterns))))
      signalPassFailure();
  }
};

} // namespace

void mlir::configureGpuToNVVMConversionLegality(ConversionTarget &target) {
  target.addIllegalOp<func::FuncOp>();
  target.addLegalDialect<::mlir::LLVM::LLVMDialect>();
  target.addLegalDialect<::mlir::NVVM::NVVMDialect>();
  target.addIllegalDialect<gpu::GPUDialect>();
  target.addIllegalOp<LLVM::CopySignOp, LLVM::CosOp, LLVM::ExpOp, LLVM::Exp2Op,
                      LLVM::FAbsOp, LLVM::FCeilOp, LLVM::FFloorOp, LLVM::FMAOp,
                      LLVM::FRemOp, LLVM::LogOp, LLVM::Log10Op, LLVM::Log2Op,
                      LLVM::PowOp, LLVM::RoundEvenOp, LLVM::RoundOp,
                      LLVM::SinOp, LLVM::SqrtOp>();

  // TODO: Remove once we support replacing non-root ops.
  target.addLegalOp<gpu::YieldOp, gpu::GPUModuleOp>();
}

template <typename OpTy>
static void populateOpPatterns(LLVMTypeConverter &converter,
                               RewritePatternSet &patterns, StringRef f32Func,
                               StringRef f64Func,
                               StringRef f32ApproxFunc = "") {
  patterns.add<ScalarizeVectorOpLowering<OpTy>>(converter);
  patterns.add<OpToFuncCallLowering<OpTy>>(converter, f32Func, f64Func,
                                           f32ApproxFunc);
}

void mlir::populateGpuSubgroupReduceOpLoweringPattern(
    LLVMTypeConverter &converter, RewritePatternSet &patterns) {
  patterns.add<GPUSubgroupReduceOpLowering>(converter);
}

void mlir::populateGpuToNVVMConversionPatterns(LLVMTypeConverter &converter,
                                               RewritePatternSet &patterns) {
  populateWithGenerated(patterns);
  patterns.add<GPUPrintfOpToVPrintfLowering>(converter);
  patterns.add<
      gpu::index_lowering::OpLowering<gpu::ThreadIdOp, NVVM::ThreadIdXOp,
                                      NVVM::ThreadIdYOp, NVVM::ThreadIdZOp>,
      gpu::index_lowering::OpLowering<gpu::BlockDimOp, NVVM::BlockDimXOp,
                                      NVVM::BlockDimYOp, NVVM::BlockDimZOp>,
      gpu::index_lowering::OpLowering<gpu::ClusterIdOp, NVVM::ClusterIdXOp,
                                      NVVM::ClusterIdYOp, NVVM::ClusterIdZOp>,
      gpu::index_lowering::OpLowering<gpu::ClusterDimOp, NVVM::ClusterDimXOp,
                                      NVVM::ClusterDimYOp, NVVM::ClusterDimZOp>,
      gpu::index_lowering::OpLowering<
          gpu::ClusterBlockIdOp, NVVM::BlockInClusterIdXOp,
          NVVM::BlockInClusterIdYOp, NVVM::BlockInClusterIdZOp>,
      gpu::index_lowering::OpLowering<gpu::ClusterDimOp, NVVM::ClusterDimXOp,
                                      NVVM::ClusterDimYOp, NVVM::ClusterDimZOp>,
      gpu::index_lowering::OpLowering<gpu::BlockIdOp, NVVM::BlockIdXOp,
                                      NVVM::BlockIdYOp, NVVM::BlockIdZOp>,
      gpu::index_lowering::OpLowering<gpu::GridDimOp, NVVM::GridDimXOp,
                                      NVVM::GridDimYOp, NVVM::GridDimZOp>,
      GPULaneIdOpToNVVM, GPUShuffleOpLowering, GPUReturnOpLowering>(converter);

  patterns.add<GPUDynamicSharedMemoryOpLowering>(
      converter, NVVM::kSharedMemoryAlignmentBit);

  // Explicitly drop memory space when lowering private memory
  // attributions since NVVM models it as `alloca`s in the default
  // memory space and does not support `alloca`s with addrspace(5).
  patterns.add<GPUFuncOpLowering>(
      converter,
      GPUFuncOpLoweringOptions{
          /*allocaAddrSpace=*/0,
          /*workgroupAddrSpace=*/
          static_cast<unsigned>(NVVM::NVVMMemorySpace::kSharedMemorySpace),
          StringAttr::get(&converter.getContext(),
                          NVVM::NVVMDialect::getKernelFuncAttrName()),
          StringAttr::get(&converter.getContext(),
                          NVVM::NVVMDialect::getMaxntidAttrName())});

  populateOpPatterns<arith::RemFOp>(converter, patterns, "__nv_fmodf",
                                    "__nv_fmod");
  populateOpPatterns<math::AbsFOp>(converter, patterns, "__nv_fabsf",
                                   "__nv_fabs");
  populateOpPatterns<math::AcosOp>(converter, patterns, "__nv_acosf",
                                   "__nv_acos");
  populateOpPatterns<math::AcoshOp>(converter, patterns, "__nv_acoshf",
                                    "__nv_acosh");
  populateOpPatterns<math::AsinOp>(converter, patterns, "__nv_asinf",
                                   "__nv_asin");
  populateOpPatterns<math::AsinhOp>(converter, patterns, "__nv_asinhf",
                                    "__nv_asinh");
  populateOpPatterns<math::AtanOp>(converter, patterns, "__nv_atanf",
                                   "__nv_atan");
  populateOpPatterns<math::Atan2Op>(converter, patterns, "__nv_atan2f",
                                    "__nv_atan2");
  populateOpPatterns<math::AtanhOp>(converter, patterns, "__nv_atanhf",
                                    "__nv_atanh");
  populateOpPatterns<math::CbrtOp>(converter, patterns, "__nv_cbrtf",
                                   "__nv_cbrt");
  populateOpPatterns<math::CeilOp>(converter, patterns, "__nv_ceilf",
                                   "__nv_ceil");
  populateOpPatterns<math::CopySignOp>(converter, patterns, "__nv_copysignf",
                                       "__nv_copysign");
  populateOpPatterns<math::CosOp>(converter, patterns, "__nv_cosf", "__nv_cos",
                                  "__nv_fast_cosf");
  populateOpPatterns<math::CoshOp>(converter, patterns, "__nv_coshf",
                                   "__nv_cosh");
  populateOpPatterns<math::ErfOp>(converter, patterns, "__nv_erff", "__nv_erf");
  populateOpPatterns<math::ExpOp>(converter, patterns, "__nv_expf", "__nv_exp",
                                  "__nv_fast_expf");
  populateOpPatterns<math::Exp2Op>(converter, patterns, "__nv_exp2f",
                                   "__nv_exp2");
  populateOpPatterns<math::ExpM1Op>(converter, patterns, "__nv_expm1f",
                                    "__nv_expm1");
  populateOpPatterns<math::FloorOp>(converter, patterns, "__nv_floorf",
                                    "__nv_floor");
  populateOpPatterns<math::FmaOp>(converter, patterns, "__nv_fmaf", "__nv_fma");
  populateOpPatterns<math::LogOp>(converter, patterns, "__nv_logf", "__nv_log",
                                  "__nv_fast_logf");
  populateOpPatterns<math::Log10Op>(converter, patterns, "__nv_log10f",
                                    "__nv_log10", "__nv_fast_log10f");
  populateOpPatterns<math::Log1pOp>(converter, patterns, "__nv_log1pf",
                                    "__nv_log1p");
  populateOpPatterns<math::Log2Op>(converter, patterns, "__nv_log2f",
                                   "__nv_log2", "__nv_fast_log2f");
  populateOpPatterns<math::PowFOp>(converter, patterns, "__nv_powf", "__nv_pow",
                                   "__nv_fast_powf");
  populateOpPatterns<math::RoundOp>(converter, patterns, "__nv_roundf",
                                    "__nv_round");
  populateOpPatterns<math::RoundEvenOp>(converter, patterns, "__nv_rintf",
                                        "__nv_rint");
  populateOpPatterns<math::RsqrtOp>(converter, patterns, "__nv_rsqrtf",
                                    "__nv_rsqrt");
  populateOpPatterns<math::SinOp>(converter, patterns, "__nv_sinf", "__nv_sin",
                                  "__nv_fast_sinf");
  populateOpPatterns<math::SinhOp>(converter, patterns, "__nv_sinhf",
                                   "__nv_sinh");
  populateOpPatterns<math::SqrtOp>(converter, patterns, "__nv_sqrtf",
                                   "__nv_sqrt");
  populateOpPatterns<math::TanOp>(converter, patterns, "__nv_tanf", "__nv_tan",
                                  "__nv_fast_tanf");
  populateOpPatterns<math::TanhOp>(converter, patterns, "__nv_tanhf",
                                   "__nv_tanh");
}
