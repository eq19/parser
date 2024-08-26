//===- LowerGpuOpsToROCDLOps.cpp - MLIR GPU to ROCDL lowering passes ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass to generate ROCDLIR operations for higher-level
// GPU operations.
//
//===----------------------------------------------------------------------===//

#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Conversion/GPUToROCDL/GPUToROCDLPass.h"
#include "mlir/Dialect/Arith/Transforms/Passes.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/Passes.h"

#include "mlir/Conversion/AMDGPUToROCDL/AMDGPUToROCDL.h"
#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/GPUCommon/GPUCommonPass.h"
#include "mlir/Conversion/LLVMCommon/ConversionTarget.h"
#include "mlir/Conversion/LLVMCommon/LoweringOptions.h"
#include "mlir/Conversion/LLVMCommon/Pattern.h"
#include "mlir/Conversion/LLVMCommon/TypeConverter.h"
#include "mlir/Conversion/MathToROCDL/MathToROCDL.h"
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Conversion/VectorToLLVM/ConvertVectorToLLVM.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/GPU/Transforms/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/ROCDLDialect.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/Support/FormatVariadic.h"

#include "../GPUCommon/GPUOpsLowering.h"
#include "../GPUCommon/IndexIntrinsicsOpLowering.h"
#include "../GPUCommon/OpToFuncCallLowering.h"

namespace mlir {
#define GEN_PASS_DEF_CONVERTGPUOPSTOROCDLOPS
#include "mlir/Conversion/Passes.h.inc"
} // namespace mlir

using namespace mlir;

/// Returns true if the given `gpu.func` can be safely called using the bare
/// pointer calling convention.
static bool canBeCalledWithBarePointers(gpu::GPUFuncOp func) {
  bool canBeBare = true;
  for (Type type : func.getArgumentTypes())
    if (auto memrefTy = dyn_cast<BaseMemRefType>(type))
      canBeBare &= LLVMTypeConverter::canConvertToBarePtr(memrefTy);
  return canBeBare;
}

Value getLaneId(ConversionPatternRewriter &rewriter, Location loc,
                const unsigned indexBitwidth) {
  auto int32Type = IntegerType::get(rewriter.getContext(), 32);
  Value zero = rewriter.create<arith::ConstantIntOp>(loc, 0, 32);
  Value minus1 = rewriter.create<arith::ConstantIntOp>(loc, -1, 32);
  Value mbcntLo = rewriter.create<ROCDL::MbcntLoOp>(loc, int32Type,
                                                    ValueRange{minus1, zero});
  Value laneId = rewriter.create<ROCDL::MbcntHiOp>(loc, int32Type,
                                                   ValueRange{minus1, mbcntLo});
  return laneId;
}
static constexpr StringLiteral amdgcnDataLayout =
    "e-p:64:64-p1:64:64-p2:32:32-p3:32:32-p4:64:64-p5:32:32-p6:32:32"
    "-p7:160:256:256:32-p8:128:128-p9:192:256:256:32-i64:64-v16:16-v24:32-v32:"
    "32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-v2048:2048-n32:"
    "64-S32-A5-G1-ni:7:8:9";

namespace {
struct GPULaneIdOpToROCDL : ConvertOpToLLVMPattern<gpu::LaneIdOp> {
  using ConvertOpToLLVMPattern<gpu::LaneIdOp>::ConvertOpToLLVMPattern;

  LogicalResult
  matchAndRewrite(gpu::LaneIdOp op, gpu::LaneIdOp::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto loc = op->getLoc();
    MLIRContext *context = rewriter.getContext();
    // convert to:  %mlo = call @llvm.amdgcn.mbcnt.lo(-1, 0)
    // followed by: %lid = call @llvm.amdgcn.mbcnt.hi(-1, %mlo)

    Type intTy = IntegerType::get(context, 32);
    Value zero = rewriter.create<arith::ConstantIntOp>(loc, 0, 32);
    Value minus1 = rewriter.create<arith::ConstantIntOp>(loc, -1, 32);
    Value mbcntLo =
        rewriter.create<ROCDL::MbcntLoOp>(loc, intTy, ValueRange{minus1, zero});
    Value laneId = rewriter.create<ROCDL::MbcntHiOp>(
        loc, intTy, ValueRange{minus1, mbcntLo});
    // Truncate or extend the result depending on the index bitwidth specified
    // by the LLVMTypeConverter options.
    const unsigned indexBitwidth = getTypeConverter()->getIndexTypeBitwidth();
    if (indexBitwidth > 32) {
      laneId = rewriter.create<LLVM::SExtOp>(
          loc, IntegerType::get(context, indexBitwidth), laneId);
    } else if (indexBitwidth < 32) {
      laneId = rewriter.create<LLVM::TruncOp>(
          loc, IntegerType::get(context, indexBitwidth), laneId);
    }
    rewriter.replaceOp(op, {laneId});
    return success();
  }
};

struct GPUShuffleOpLowering : public ConvertOpToLLVMPattern<gpu::ShuffleOp> {
  using ConvertOpToLLVMPattern<gpu::ShuffleOp>::ConvertOpToLLVMPattern;

  /// Lowers a shuffle to the corresponding ROCDL ops.
  ///
  /// Use the `width` argument to see if src lane is participating.
  /// If not the dstLane would be itself.
  ///
  ///  Shuffle with DS Bpermute:
  ///   let shflMode = [xor, up, down, idx]
  ///   let width = 32(usually warpsize), step = [1, 2, 4, 8, 16, ... , width].
  ///   1. curLaneId = using mbcnt.lo + mbcnt.hi
  ///   2. widthOrZeroIfOutside = (curLaneId + width) & -width
  ///   3. dstLane = shflMode(curLaneId, step)
  ///   4. isActiveSrcLane = dstLane < isActiveSrcLane
  ///   5. dstLane = isActiveSrcLane ? dstLane : curLaneId
  ///   6. dwordAlignedDstLane = dstLane * 4 or dstLane << 2.
  ///   7. bpermute(dwordAlignedDstLane, shfl_value).
  ///
  LogicalResult
  matchAndRewrite(gpu::ShuffleOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    Location loc = op->getLoc();
    // TODO: Add support for non 32-bit shuffle values.
    if (adaptor.getValue().getType().getIntOrFloatBitWidth() != 32)
      return failure();
    const unsigned indexBitwidth = getTypeConverter()->getIndexTypeBitwidth();
    Value srcLaneId = getLaneId(rewriter, loc, indexBitwidth);

    auto int32Type = IntegerType::get(rewriter.getContext(), 32);
    Value width = adaptor.getWidth();
    Value zero = rewriter.create<LLVM::ConstantOp>(loc, int32Type, 0);
    Value negwidth = rewriter.create<LLVM::SubOp>(loc, int32Type, zero, width);
    Value add = rewriter.create<LLVM::AddOp>(loc, int32Type, srcLaneId, width);
    Value widthOrZeroIfOutside =
        rewriter.create<LLVM::AndOp>(loc, int32Type, add, negwidth);
    Value dstLane;
    // TODO: Add support for gpu::ShuffleMode::UP and gpu::ShuffleMode::DOWN.
    // TODO: Use ds_swizzle for XOR when step/offsets are constants for better
    // perf.
    switch (op.getMode()) {
    case gpu::ShuffleMode::XOR:
      dstLane = rewriter.create<LLVM::XOrOp>(loc, int32Type, srcLaneId,
                                             adaptor.getOffset());
      break;
    case gpu::ShuffleMode::IDX:
      dstLane = adaptor.getOffset();
      break;
    default:
      return failure();
    }
    Value isActiveSrcLane = rewriter.create<LLVM::ICmpOp>(
        loc, LLVM::ICmpPredicate::slt, dstLane, widthOrZeroIfOutside);
    Value selectDstLane = rewriter.create<LLVM::SelectOp>(loc, isActiveSrcLane,
                                                          dstLane, srcLaneId);
    Value two = rewriter.create<LLVM::ConstantOp>(loc, int32Type, 2);
    Value dwordAlignedDstLane =
        rewriter.create<LLVM::ShlOp>(loc, int32Type, selectDstLane, two);
    Value initShflValue = adaptor.getValue();
    if (adaptor.getValue().getType().isF32()) {
      initShflValue =
          rewriter.create<LLVM::BitcastOp>(loc, int32Type, initShflValue);
    }
    Value shflValue = rewriter.create<ROCDL::DsBpermuteOp>(
        loc, int32Type, dwordAlignedDstLane, initShflValue);
    if (adaptor.getValue().getType().isF32()) {
      shflValue = rewriter.create<LLVM::BitcastOp>(
          loc, adaptor.getValue().getType(), shflValue);
    }
    rewriter.replaceOp(op, {shflValue, isActiveSrcLane});
    return success();
  }
};

/// Import the GPU Ops to ROCDL Patterns.
#include "GPUToROCDL.cpp.inc"

// A pass that replaces all occurrences of GPU device operations with their
// corresponding ROCDL equivalent.
//
// This pass only handles device code and is not meant to be run on GPU host
// code.
struct LowerGpuOpsToROCDLOpsPass
    : public impl::ConvertGpuOpsToROCDLOpsBase<LowerGpuOpsToROCDLOpsPass> {
  LowerGpuOpsToROCDLOpsPass() = default;
  LowerGpuOpsToROCDLOpsPass(const std::string &chipset, unsigned indexBitwidth,
                            bool useBarePtrCallConv,
                            gpu::amd::Runtime runtime) {
    if (this->chipset.getNumOccurrences() == 0)
      this->chipset = chipset;
    if (this->indexBitwidth.getNumOccurrences() == 0)
      this->indexBitwidth = indexBitwidth;
    if (this->useBarePtrCallConv.getNumOccurrences() == 0)
      this->useBarePtrCallConv = useBarePtrCallConv;
    if (this->runtime.getNumOccurrences() == 0)
      this->runtime = runtime;
  }

  void runOnOperation() override {
    gpu::GPUModuleOp m = getOperation();
    MLIRContext *ctx = m.getContext();

    auto llvmDataLayout = m->getAttrOfType<StringAttr>(
        LLVM::LLVMDialect::getDataLayoutAttrName());
    if (!llvmDataLayout) {
      llvmDataLayout = StringAttr::get(ctx, amdgcnDataLayout);
      m->setAttr(LLVM::LLVMDialect::getDataLayoutAttrName(), llvmDataLayout);
    }
    // Request C wrapper emission.
    for (auto func : m.getOps<func::FuncOp>()) {
      func->setAttr(LLVM::LLVMDialect::getEmitCWrapperAttrName(),
                    UnitAttr::get(ctx));
    }

    FailureOr<amdgpu::Chipset> maybeChipset = amdgpu::Chipset::parse(chipset);
    if (failed(maybeChipset)) {
      emitError(UnknownLoc::get(ctx), "Invalid chipset name: " + chipset);
      return signalPassFailure();
    }

    /// Customize the bitwidth used for the device side index computations.
    LowerToLLVMOptions options(
        ctx, DataLayout(cast<DataLayoutOpInterface>(m.getOperation())));
    options.dataLayout = llvm::DataLayout(llvmDataLayout.getValue());
    if (indexBitwidth != kDeriveIndexBitwidthFromDataLayout)
      options.overrideIndexBitwidth(indexBitwidth);

    if (useBarePtrCallConv) {
      options.useBarePtrCallConv = true;
      WalkResult canUseBarePointers =
          m.walk([](gpu::GPUFuncOp func) -> WalkResult {
            if (canBeCalledWithBarePointers(func))
              return WalkResult::advance();
            return WalkResult::interrupt();
          });
      if (canUseBarePointers.wasInterrupted()) {
        emitError(UnknownLoc::get(ctx),
                  "bare pointer calling convention requires all memrefs to "
                  "have static shape and use the identity map");
        return signalPassFailure();
      }
    }

    // Apply in-dialect lowering. In-dialect lowering will replace
    // ops which need to be lowered further, which is not supported by a
    // single conversion pass.
    {
      RewritePatternSet patterns(ctx);
      populateGpuRewritePatterns(patterns);
      arith::populateExpandBFloat16Patterns(patterns);
      (void)applyPatternsAndFoldGreedily(m, std::move(patterns));
    }

    LLVMTypeConverter converter(ctx, options);
    populateGpuMemorySpaceAttributeConversions(
        converter, [](gpu::AddressSpace space) {
          switch (space) {
          case gpu::AddressSpace::Global:
            return 1;
          case gpu::AddressSpace::Workgroup:
            return 3;
          case gpu::AddressSpace::Private:
            return 5;
          }
          llvm_unreachable("unknown address space enum value");
          return 0;
        });

    RewritePatternSet llvmPatterns(ctx);

    mlir::arith::populateArithToLLVMConversionPatterns(converter, llvmPatterns);
    populateAMDGPUToROCDLConversionPatterns(converter, llvmPatterns,
                                            *maybeChipset);
    populateVectorToLLVMConversionPatterns(converter, llvmPatterns);
    cf::populateControlFlowToLLVMConversionPatterns(converter, llvmPatterns);
    populateFuncToLLVMConversionPatterns(converter, llvmPatterns);
    populateFinalizeMemRefToLLVMConversionPatterns(converter, llvmPatterns);
    populateGpuToROCDLConversionPatterns(converter, llvmPatterns, runtime);
    LLVMConversionTarget target(getContext());
    configureGpuToROCDLConversionLegality(target);
    if (failed(applyPartialConversion(m, target, std::move(llvmPatterns))))
      signalPassFailure();
    auto *rocdlDialect = getContext().getLoadedDialect<ROCDL::ROCDLDialect>();
    auto reqdWorkGroupSizeAttrHelper =
        rocdlDialect->getReqdWorkGroupSizeAttrHelper();
    auto flatWorkGroupSizeAttrHelper =
        rocdlDialect->getFlatWorkGroupSizeAttrHelper();
    // Manually rewrite known block size attributes so the LLVMIR translation
    // infrastructure can pick them up.
    m.walk([&](LLVM::LLVMFuncOp op) {
      if (reqdWorkGroupSizeAttrHelper.isAttrPresent(op)) {
        auto blockSizes = reqdWorkGroupSizeAttrHelper.getAttr(op);
        // Also set up the rocdl.flat_work_group_size attribute to prevent
        // conflicting metadata.
        uint32_t flatSize = 1;
        for (uint32_t size : blockSizes.asArrayRef()) {
          flatSize *= size;
        }
        StringAttr flatSizeAttr =
            StringAttr::get(ctx, Twine(flatSize) + "," + Twine(flatSize));
        flatWorkGroupSizeAttrHelper.setAttr(op, flatSizeAttr);
      }
    });
  }
};

} // namespace

void mlir::configureGpuToROCDLConversionLegality(ConversionTarget &target) {
  target.addIllegalOp<func::FuncOp>();
  target.addLegalDialect<::mlir::LLVM::LLVMDialect>();
  target.addLegalDialect<ROCDL::ROCDLDialect>();
  target.addIllegalDialect<gpu::GPUDialect>();
  target.addIllegalOp<LLVM::CosOp, LLVM::ExpOp, LLVM::Exp2Op, LLVM::FAbsOp,
                      LLVM::FCeilOp, LLVM::FFloorOp, LLVM::FRemOp, LLVM::LogOp,
                      LLVM::Log10Op, LLVM::Log2Op, LLVM::PowOp, LLVM::SinOp,
                      LLVM::SqrtOp>();

  // TODO: Remove once we support replacing non-root ops.
  target.addLegalOp<gpu::YieldOp, gpu::GPUModuleOp>();
}

template <typename OpTy>
static void populateOpPatterns(LLVMTypeConverter &converter,
                               RewritePatternSet &patterns, StringRef f32Func,
                               StringRef f64Func) {
  patterns.add<ScalarizeVectorOpLowering<OpTy>>(converter);
  patterns.add<OpToFuncCallLowering<OpTy>>(converter, f32Func, f64Func);
}

void mlir::populateGpuToROCDLConversionPatterns(
    LLVMTypeConverter &converter, RewritePatternSet &patterns,
    mlir::gpu::amd::Runtime runtime) {
  using gpu::index_lowering::IndexKind;
  using gpu::index_lowering::IntrType;
  using mlir::gpu::amd::Runtime;
  auto *rocdlDialect =
      converter.getContext().getLoadedDialect<ROCDL::ROCDLDialect>();
  populateWithGenerated(patterns);
  patterns.add<
      gpu::index_lowering::OpLowering<gpu::ThreadIdOp, ROCDL::ThreadIdXOp,
                                      ROCDL::ThreadIdYOp, ROCDL::ThreadIdZOp>>(
      converter, IndexKind::Block, IntrType::Id);
  patterns.add<gpu::index_lowering::OpLowering<
      gpu::BlockIdOp, ROCDL::BlockIdXOp, ROCDL::BlockIdYOp, ROCDL::BlockIdZOp>>(
      converter, IndexKind::Grid, IntrType::Id);
  patterns.add<
      gpu::index_lowering::OpLowering<gpu::BlockDimOp, ROCDL::BlockDimXOp,
                                      ROCDL::BlockDimYOp, ROCDL::BlockDimZOp>>(
      converter, IndexKind::Block, IntrType::Dim);
  patterns.add<gpu::index_lowering::OpLowering<
      gpu::GridDimOp, ROCDL::GridDimXOp, ROCDL::GridDimYOp, ROCDL::GridDimZOp>>(
      converter, IndexKind::Grid, IntrType::Dim);
  patterns.add<GPUReturnOpLowering>(converter);
  patterns.add<GPUFuncOpLowering>(
      converter,
      GPUFuncOpLoweringOptions{
          /*allocaAddrSpace=*/ROCDL::ROCDLDialect::kPrivateMemoryAddressSpace,
          /*workgroupAddrSpace=*/ROCDL::ROCDLDialect::kSharedMemoryAddressSpace,
          rocdlDialect->getKernelAttrHelper().getName(),
          rocdlDialect->getReqdWorkGroupSizeAttrHelper().getName()});
  if (Runtime::HIP == runtime) {
    patterns.add<GPUPrintfOpToHIPLowering>(converter);
  } else if (Runtime::OpenCL == runtime) {
    // Use address space = 4 to match the OpenCL definition of printf()
    patterns.add<GPUPrintfOpToLLVMCallLowering>(converter, /*addressSpace=*/4);
  }
  // TODO: Add alignment for workgroup memory
  patterns.add<GPUDynamicSharedMemoryOpLowering>(converter);

  patterns.add<GPUShuffleOpLowering, GPULaneIdOpToROCDL>(converter);

  populateMathToROCDLConversionPatterns(converter, patterns);
}

std::unique_ptr<OperationPass<gpu::GPUModuleOp>>
mlir::createLowerGpuOpsToROCDLOpsPass(const std::string &chipset,
                                      unsigned indexBitwidth,
                                      bool useBarePtrCallConv,
                                      gpu::amd::Runtime runtime) {
  return std::make_unique<LowerGpuOpsToROCDLOpsPass>(
      chipset, indexBitwidth, useBarePtrCallConv, runtime);
}
