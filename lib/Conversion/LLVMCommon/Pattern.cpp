//===- Pattern.cpp - Conversion pattern to the LLVM dialect ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Conversion/LLVMCommon/Pattern.h"
#include "mlir/Dialect/LLVMIR/FunctionCallUtils.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/BuiltinAttributes.h"

using namespace mlir;

//===----------------------------------------------------------------------===//
// ConvertToLLVMPattern
//===----------------------------------------------------------------------===//

ConvertToLLVMPattern::ConvertToLLVMPattern(
    StringRef rootOpName, MLIRContext *context,
    const LLVMTypeConverter &typeConverter, PatternBenefit benefit)
    : ConversionPattern(typeConverter, rootOpName, benefit, context) {}

const LLVMTypeConverter *ConvertToLLVMPattern::getTypeConverter() const {
  return static_cast<const LLVMTypeConverter *>(
      ConversionPattern::getTypeConverter());
}

LLVM::LLVMDialect &ConvertToLLVMPattern::getDialect() const {
  return *getTypeConverter()->getDialect();
}

Type ConvertToLLVMPattern::getIndexType() const {
  return getTypeConverter()->getIndexType();
}

Type ConvertToLLVMPattern::getIntPtrType(unsigned addressSpace) const {
  return IntegerType::get(&getTypeConverter()->getContext(),
                          getTypeConverter()->getPointerBitwidth(addressSpace));
}

Type ConvertToLLVMPattern::getVoidType() const {
  return LLVM::LLVMVoidType::get(&getTypeConverter()->getContext());
}

Type ConvertToLLVMPattern::getVoidPtrType() const {
  return LLVM::LLVMPointerType::get(&getTypeConverter()->getContext());
}

Value ConvertToLLVMPattern::createIndexAttrConstant(OpBuilder &builder,
                                                    Location loc,
                                                    Type resultType,
                                                    int64_t value) {
  return builder.create<LLVM::ConstantOp>(loc, resultType,
                                          builder.getIndexAttr(value));
}

Value ConvertToLLVMPattern::getStridedElementPtr(
    Location loc, MemRefType type, Value memRefDesc, ValueRange indices,
    ConversionPatternRewriter &rewriter) const {

  auto [strides, offset] = getStridesAndOffset(type);

  MemRefDescriptor memRefDescriptor(memRefDesc);
  // Use a canonical representation of the start address so that later
  // optimizations have a longer sequence of instructions to CSE.
  // If we don't do that we would sprinkle the memref.offset in various
  // position of the different address computations.
  Value base =
      memRefDescriptor.bufferPtr(rewriter, loc, *getTypeConverter(), type);

  Type indexType = getIndexType();
  Value index;
  for (int i = 0, e = indices.size(); i < e; ++i) {
    Value increment = indices[i];
    if (strides[i] != 1) { // Skip if stride is 1.
      Value stride =
          ShapedType::isDynamic(strides[i])
              ? memRefDescriptor.stride(rewriter, loc, i)
              : createIndexAttrConstant(rewriter, loc, indexType, strides[i]);
      increment = rewriter.create<LLVM::MulOp>(loc, increment, stride);
    }
    index =
        index ? rewriter.create<LLVM::AddOp>(loc, index, increment) : increment;
  }

  Type elementPtrType = memRefDescriptor.getElementPtrType();
  return index ? rewriter.create<LLVM::GEPOp>(
                     loc, elementPtrType,
                     getTypeConverter()->convertType(type.getElementType()),
                     base, index)
               : base;
}

// Check if the MemRefType `type` is supported by the lowering. We currently
// only support memrefs with identity maps.
bool ConvertToLLVMPattern::isConvertibleAndHasIdentityMaps(
    MemRefType type) const {
  if (!typeConverter->convertType(type.getElementType()))
    return false;
  return type.getLayout().isIdentity();
}

Type ConvertToLLVMPattern::getElementPtrType(MemRefType type) const {
  auto addressSpace = getTypeConverter()->getMemRefAddressSpace(type);
  if (failed(addressSpace))
    return {};
  return LLVM::LLVMPointerType::get(type.getContext(), *addressSpace);
}

void ConvertToLLVMPattern::getMemRefDescriptorSizes(
    Location loc, MemRefType memRefType, ValueRange dynamicSizes,
    ConversionPatternRewriter &rewriter, SmallVectorImpl<Value> &sizes,
    SmallVectorImpl<Value> &strides, Value &size, bool sizeInBytes) const {
  assert(isConvertibleAndHasIdentityMaps(memRefType) &&
         "layout maps must have been normalized away");
  assert(count(memRefType.getShape(), ShapedType::kDynamic) ==
             static_cast<ssize_t>(dynamicSizes.size()) &&
         "dynamicSizes size doesn't match dynamic sizes count in memref shape");

  sizes.reserve(memRefType.getRank());
  unsigned dynamicIndex = 0;
  Type indexType = getIndexType();
  for (int64_t size : memRefType.getShape()) {
    sizes.push_back(
        size == ShapedType::kDynamic
            ? dynamicSizes[dynamicIndex++]
            : createIndexAttrConstant(rewriter, loc, indexType, size));
  }

  // Strides: iterate sizes in reverse order and multiply.
  int64_t stride = 1;
  Value runningStride = createIndexAttrConstant(rewriter, loc, indexType, 1);
  strides.resize(memRefType.getRank());
  for (auto i = memRefType.getRank(); i-- > 0;) {
    strides[i] = runningStride;

    int64_t staticSize = memRefType.getShape()[i];
    if (staticSize == 0)
      continue;
    bool useSizeAsStride = stride == 1;
    if (staticSize == ShapedType::kDynamic)
      stride = ShapedType::kDynamic;
    if (stride != ShapedType::kDynamic)
      stride *= staticSize;

    if (useSizeAsStride)
      runningStride = sizes[i];
    else if (stride == ShapedType::kDynamic)
      runningStride =
          rewriter.create<LLVM::MulOp>(loc, runningStride, sizes[i]);
    else
      runningStride = createIndexAttrConstant(rewriter, loc, indexType, stride);
  }
  if (sizeInBytes) {
    // Buffer size in bytes.
    Type elementType = typeConverter->convertType(memRefType.getElementType());
    auto elementPtrType = LLVM::LLVMPointerType::get(rewriter.getContext());
    Value nullPtr = rewriter.create<LLVM::ZeroOp>(loc, elementPtrType);
    Value gepPtr = rewriter.create<LLVM::GEPOp>(
        loc, elementPtrType, elementType, nullPtr, runningStride);
    size = rewriter.create<LLVM::PtrToIntOp>(loc, getIndexType(), gepPtr);
  } else {
    size = runningStride;
  }
}

Value ConvertToLLVMPattern::getSizeInBytes(
    Location loc, Type type, ConversionPatternRewriter &rewriter) const {
  // Compute the size of an individual element. This emits the MLIR equivalent
  // of the following sizeof(...) implementation in LLVM IR:
  //   %0 = getelementptr %elementType* null, %indexType 1
  //   %1 = ptrtoint %elementType* %0 to %indexType
  // which is a common pattern of getting the size of a type in bytes.
  Type llvmType = typeConverter->convertType(type);
  auto convertedPtrType = LLVM::LLVMPointerType::get(rewriter.getContext());
  auto nullPtr = rewriter.create<LLVM::ZeroOp>(loc, convertedPtrType);
  auto gep = rewriter.create<LLVM::GEPOp>(loc, convertedPtrType, llvmType,
                                          nullPtr, ArrayRef<LLVM::GEPArg>{1});
  return rewriter.create<LLVM::PtrToIntOp>(loc, getIndexType(), gep);
}

Value ConvertToLLVMPattern::getNumElements(
    Location loc, MemRefType memRefType, ValueRange dynamicSizes,
    ConversionPatternRewriter &rewriter) const {
  assert(count(memRefType.getShape(), ShapedType::kDynamic) ==
             static_cast<ssize_t>(dynamicSizes.size()) &&
         "dynamicSizes size doesn't match dynamic sizes count in memref shape");

  Type indexType = getIndexType();
  Value numElements = memRefType.getRank() == 0
                          ? createIndexAttrConstant(rewriter, loc, indexType, 1)
                          : nullptr;
  unsigned dynamicIndex = 0;

  // Compute the total number of memref elements.
  for (int64_t staticSize : memRefType.getShape()) {
    if (numElements) {
      Value size =
          staticSize == ShapedType::kDynamic
              ? dynamicSizes[dynamicIndex++]
              : createIndexAttrConstant(rewriter, loc, indexType, staticSize);
      numElements = rewriter.create<LLVM::MulOp>(loc, numElements, size);
    } else {
      numElements =
          staticSize == ShapedType::kDynamic
              ? dynamicSizes[dynamicIndex++]
              : createIndexAttrConstant(rewriter, loc, indexType, staticSize);
    }
  }
  return numElements;
}

/// Creates and populates the memref descriptor struct given all its fields.
MemRefDescriptor ConvertToLLVMPattern::createMemRefDescriptor(
    Location loc, MemRefType memRefType, Value allocatedPtr, Value alignedPtr,
    ArrayRef<Value> sizes, ArrayRef<Value> strides,
    ConversionPatternRewriter &rewriter) const {
  auto structType = typeConverter->convertType(memRefType);
  auto memRefDescriptor = MemRefDescriptor::undef(rewriter, loc, structType);

  // Field 1: Allocated pointer, used for malloc/free.
  memRefDescriptor.setAllocatedPtr(rewriter, loc, allocatedPtr);

  // Field 2: Actual aligned pointer to payload.
  memRefDescriptor.setAlignedPtr(rewriter, loc, alignedPtr);

  // Field 3: Offset in aligned pointer.
  Type indexType = getIndexType();
  memRefDescriptor.setOffset(
      rewriter, loc, createIndexAttrConstant(rewriter, loc, indexType, 0));

  // Fields 4: Sizes.
  for (const auto &en : llvm::enumerate(sizes))
    memRefDescriptor.setSize(rewriter, loc, en.index(), en.value());

  // Field 5: Strides.
  for (const auto &en : llvm::enumerate(strides))
    memRefDescriptor.setStride(rewriter, loc, en.index(), en.value());

  return memRefDescriptor;
}

LogicalResult ConvertToLLVMPattern::copyUnrankedDescriptors(
    OpBuilder &builder, Location loc, TypeRange origTypes,
    SmallVectorImpl<Value> &operands, bool toDynamic) const {
  assert(origTypes.size() == operands.size() &&
         "expected as may original types as operands");

  // Find operands of unranked memref type and store them.
  SmallVector<UnrankedMemRefDescriptor> unrankedMemrefs;
  SmallVector<unsigned> unrankedAddressSpaces;
  for (unsigned i = 0, e = operands.size(); i < e; ++i) {
    if (auto memRefType = dyn_cast<UnrankedMemRefType>(origTypes[i])) {
      unrankedMemrefs.emplace_back(operands[i]);
      FailureOr<unsigned> addressSpace =
          getTypeConverter()->getMemRefAddressSpace(memRefType);
      if (failed(addressSpace))
        return failure();
      unrankedAddressSpaces.emplace_back(*addressSpace);
    }
  }

  if (unrankedMemrefs.empty())
    return success();

  // Compute allocation sizes.
  SmallVector<Value> sizes;
  UnrankedMemRefDescriptor::computeSizes(builder, loc, *getTypeConverter(),
                                         unrankedMemrefs, unrankedAddressSpaces,
                                         sizes);

  // Get frequently used types.
  Type indexType = getTypeConverter()->getIndexType();

  // Find the malloc and free, or declare them if necessary.
  auto module = builder.getInsertionPoint()->getParentOfType<ModuleOp>();
  LLVM::LLVMFuncOp freeFunc, mallocFunc;
  if (toDynamic)
    mallocFunc = LLVM::lookupOrCreateMallocFn(module, indexType);
  if (!toDynamic)
    freeFunc = LLVM::lookupOrCreateFreeFn(module);

  unsigned unrankedMemrefPos = 0;
  for (unsigned i = 0, e = operands.size(); i < e; ++i) {
    Type type = origTypes[i];
    if (!isa<UnrankedMemRefType>(type))
      continue;
    Value allocationSize = sizes[unrankedMemrefPos++];
    UnrankedMemRefDescriptor desc(operands[i]);

    // Allocate memory, copy, and free the source if necessary.
    Value memory =
        toDynamic
            ? builder.create<LLVM::CallOp>(loc, mallocFunc, allocationSize)
                  .getResult()
            : builder.create<LLVM::AllocaOp>(loc, getVoidPtrType(),
                                             IntegerType::get(getContext(), 8),
                                             allocationSize,
                                             /*alignment=*/0);
    Value source = desc.memRefDescPtr(builder, loc);
    builder.create<LLVM::MemcpyOp>(loc, memory, source, allocationSize, false);
    if (!toDynamic)
      builder.create<LLVM::CallOp>(loc, freeFunc, source);

    // Create a new descriptor. The same descriptor can be returned multiple
    // times, attempting to modify its pointer can lead to memory leaks
    // (allocated twice and overwritten) or double frees (the caller does not
    // know if the descriptor points to the same memory).
    Type descriptorType = getTypeConverter()->convertType(type);
    if (!descriptorType)
      return failure();
    auto updatedDesc =
        UnrankedMemRefDescriptor::undef(builder, loc, descriptorType);
    Value rank = desc.rank(builder, loc);
    updatedDesc.setRank(builder, loc, rank);
    updatedDesc.setMemRefDescPtr(builder, loc, memory);

    operands[i] = updatedDesc;
  }

  return success();
}

//===----------------------------------------------------------------------===//
// Detail methods
//===----------------------------------------------------------------------===//

void LLVM::detail::setNativeProperties(Operation *op,
                                       IntegerOverflowFlags overflowFlags) {
  if (auto iface = dyn_cast<IntegerOverflowFlagsInterface>(op))
    iface.setOverflowFlags(overflowFlags);
}

/// Replaces the given operation "op" with a new operation of type "targetOp"
/// and given operands.
LogicalResult LLVM::detail::oneToOneRewrite(
    Operation *op, StringRef targetOp, ValueRange operands,
    ArrayRef<NamedAttribute> targetAttrs,
    const LLVMTypeConverter &typeConverter, ConversionPatternRewriter &rewriter,
    IntegerOverflowFlags overflowFlags) {
  unsigned numResults = op->getNumResults();

  SmallVector<Type> resultTypes;
  if (numResults != 0) {
    resultTypes.push_back(
        typeConverter.packOperationResults(op->getResultTypes()));
    if (!resultTypes.back())
      return failure();
  }

  // Create the operation through state since we don't know its C++ type.
  Operation *newOp =
      rewriter.create(op->getLoc(), rewriter.getStringAttr(targetOp), operands,
                      resultTypes, targetAttrs);

  setNativeProperties(newOp, overflowFlags);

  // If the operation produced 0 or 1 result, return them immediately.
  if (numResults == 0)
    return rewriter.eraseOp(op), success();
  if (numResults == 1)
    return rewriter.replaceOp(op, newOp->getResult(0)), success();

  // Otherwise, it had been converted to an operation producing a structure.
  // Extract individual results from the structure and return them as list.
  SmallVector<Value, 4> results;
  results.reserve(numResults);
  for (unsigned i = 0; i < numResults; ++i) {
    results.push_back(rewriter.create<LLVM::ExtractValueOp>(
        op->getLoc(), newOp->getResult(0), i));
  }
  rewriter.replaceOp(op, results);
  return success();
}
