// RUN: mlir-opt %s --pass-pipeline="builtin.module(llvm.func(sroa))" --split-input-file | FileCheck %s

// CHECK-LABEL: llvm.func @basic_struct
llvm.func @basic_struct() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr inbounds %1[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, i32)>
  // CHECK: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %3 = llvm.load %2 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %3 : i32
}

// -----

// CHECK-LABEL: llvm.func @basic_array
llvm.func @basic_array() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x i32
  %1 = llvm.alloca %0 x !llvm.array<10 x i32> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr inbounds %1[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i32>
  // CHECK: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %3 = llvm.load %2 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %3 : i32
}

// -----

// CHECK-LABEL: llvm.func @multi_level_direct
llvm.func @multi_level_direct() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, struct<"bar", (i8, array<10 x array<10 x i32>>, i8)>)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr inbounds %1[0, 2, 1, 5, 8] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, struct<"bar", (i8, array<10 x array<10 x i32>>, i8)>)>
  // CHECK: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %3 = llvm.load %2 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %3 : i32
}

// -----

// The first application of SROA would generate a GEP with indices [0, 0]. This
// test ensures this GEP is not eliminated during the first application. Even
// though doing it would be correct, it would prevent the second application
// of SROA to eliminate the array. GEPs should be eliminated only when they are
// truly trivial (with indices [0]).

// CHECK-LABEL: llvm.func @multi_level_direct_two_applications
llvm.func @multi_level_direct_two_applications() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, array<10 x i32>, i8)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr inbounds %1[0, 2, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, array<10 x i32>, i8)>
  // CHECK: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %3 = llvm.load %2 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %3 : i32
}

// -----

// CHECK-LABEL: llvm.func @multi_level_indirect
llvm.func @multi_level_indirect() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, struct<"bar", (i8, array<10 x array<10 x i32>>, i8)>)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr inbounds %1[0, 2, 1, 5] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, struct<"bar", (i8, array<10 x array<10 x i32>>, i8)>)>
  %3 = llvm.getelementptr inbounds %2[0, 8] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i32>
  // CHECK: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %4 = llvm.load %3 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %4 : i32
}

// -----

// This verifies that a nested GEP's users are checked properly. In this case
// the load goes over the bounds of the memory slot and thus should block the
// splitting of the alloca.

// CHECK-LABEL: llvm.func @nested_access_over_slot_bound
llvm.func @nested_access_over_slot_bound() -> i64 {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x !llvm.struct<(i32, struct<(
  %1 = llvm.alloca %0 x !llvm.struct<(i32, struct<(array<10 x i32>)>, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  // CHECK: %[[GEP0:.*]] = llvm.getelementptr inbounds %[[ALLOCA]]
  %2 = llvm.getelementptr inbounds %1[0, 1, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i32, struct<(array<10 x i32>)>, i32)>
  // CHECK: %[[GEP1:.*]] = llvm.getelementptr inbounds %[[GEP0]]
  %3 = llvm.getelementptr inbounds %2[0, 9] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<10 x i32>
  // CHECK: %[[RES:.*]] = llvm.load %[[GEP1]]
  %4 = llvm.load %3 : !llvm.ptr -> i64
  // CHECK: llvm.return %[[RES]] : i64
  llvm.return %4 : i64
}

// -----

// CHECK-LABEL: llvm.func @resolve_alias
// CHECK-SAME: (%[[ARG:.*]]: i32)
llvm.func @resolve_alias(%arg: i32) -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr %1[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, i32)>
  %3 = llvm.getelementptr inbounds %1[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, i32)>
  // CHECK: llvm.store %[[ARG]], %[[ALLOCA]]
  llvm.store %arg, %2 : i32, !llvm.ptr
  // CHECK: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %4 = llvm.load %3 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %4 : i32
}

// -----

// CHECK-LABEL: llvm.func @no_non_single_support
llvm.func @no_non_single_support() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant
  %0 = llvm.mlir.constant(2 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  // CHECK-NOT: = llvm.alloca
  %2 = llvm.getelementptr inbounds %1[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, i32)>
  %3 = llvm.load %2 : !llvm.ptr -> i32
  llvm.return %3 : i32
}

// -----

// CHECK-LABEL: llvm.func @no_pointer_indexing
llvm.func @no_pointer_indexing() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  // CHECK-NOT: = llvm.alloca
  %2 = llvm.getelementptr %1[1, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, i32)>
  %3 = llvm.load %2 : !llvm.ptr -> i32
  llvm.return %3 : i32
}

// -----

// CHECK-LABEL: llvm.func @no_direct_use
llvm.func @no_direct_use() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  // CHECK-NOT: = llvm.alloca
  %2 = llvm.getelementptr %1[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, i32)>
  %3 = llvm.load %2 : !llvm.ptr -> i32
  llvm.call @use(%1) : (!llvm.ptr) -> ()
  llvm.return %3 : i32
}

llvm.func @use(!llvm.ptr)

// -----

// CHECK-LABEL: llvm.func @direct_promotable_use_is_fine
llvm.func @direct_promotable_use_is_fine() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr %1[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, i32)>
  // CHECK: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %3 = llvm.load %2 : !llvm.ptr -> i32
  // This is a direct use of the slot but it can be removed because it implements PromotableOpInterface.
  llvm.intr.lifetime.start 2, %1 : !llvm.ptr
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %3 : i32
}

// -----

// CHECK-LABEL: llvm.func @direct_promotable_use_is_fine_on_accessor
llvm.func @direct_promotable_use_is_fine_on_accessor() -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, f64, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr %1[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"foo", (i32, f64, i32)>
  // This does not provide side-effect info but it can be removed because it implements PromotableOpInterface.
  %3 = llvm.intr.invariant.start 2, %2 : !llvm.ptr
  // CHECK: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %4 = llvm.load %2 : !llvm.ptr -> i32
  // This does not provide side-effect info but it can be removed because it implements PromotableOpInterface.
  llvm.intr.invariant.end %3, 2, %2 : !llvm.ptr
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %4 : i32
}

// -----

// CHECK-LABEL: llvm.func @no_dynamic_indexing
// CHECK-SAME: (%[[ARG:.*]]: i32)
llvm.func @no_dynamic_indexing(%arg: i32) -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x !llvm.array<10 x i32> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %1 = llvm.alloca %0 x !llvm.array<10 x i32> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  // CHECK-NOT: = llvm.alloca
  // CHECK: %[[GEP:.*]] = llvm.getelementptr %[[ALLOCA]][0, %[[ARG]]]
  %2 = llvm.getelementptr %1[0, %arg] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.array<10 x i32>
  // CHECK: %[[RES:.*]] = llvm.load %[[GEP]]
  %3 = llvm.load %2 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %3 : i32
}

// -----

// CHECK-LABEL: llvm.func @no_nested_dynamic_indexing
// CHECK-SAME: (%[[ARG:.*]]: i32)
llvm.func @no_nested_dynamic_indexing(%arg: i32) -> i32 {
  // CHECK: %[[SIZE:.*]] = llvm.mlir.constant(1 : i32)
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %[[SIZE]] x !llvm.struct<(array<10 x i32>, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  %1 = llvm.alloca %0 x !llvm.struct<(array<10 x i32>, i32)> {alignment = 8 : i64} : (i32) -> !llvm.ptr
  // CHECK-NOT: = llvm.alloca
  // CHECK: %[[GEP:.*]] = llvm.getelementptr %[[ALLOCA]][0, 0, %[[ARG]]]
  %2 = llvm.getelementptr %1[0, 0, %arg] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(array<10 x i32>, i32)>
  // CHECK: %[[RES:.*]] = llvm.load %[[GEP]]
  %3 = llvm.load %2 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %3 : i32
}

// -----

// CHECK-LABEL: llvm.func @store_first_field
llvm.func @store_first_field(%arg: i32) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, i32, i32)> : (i32) -> !llvm.ptr
  // CHECK-NEXT: llvm.store %{{.*}}, %[[ALLOCA]] : i32
  llvm.store %arg, %1 : i32, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @store_first_field_different_type
// CHECK-SAME: (%[[ARG:.*]]: f32)
llvm.func @store_first_field_different_type(%arg: f32) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, i32, i32)> : (i32) -> !llvm.ptr
  // CHECK-NEXT: llvm.store %[[ARG]], %[[ALLOCA]] : f32
  llvm.store %arg, %1 : f32, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @store_sub_field
// CHECK-SAME: (%[[ARG:.*]]: f32)
llvm.func @store_sub_field(%arg: f32) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i64
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i64, i32)> : (i32) -> !llvm.ptr
  // CHECK-NEXT: llvm.store %[[ARG]], %[[ALLOCA]] : f32
  llvm.store %arg, %1 : f32, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @load_first_field
llvm.func @load_first_field() -> i32 {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, i32, i32)> : (i32) -> !llvm.ptr
  // CHECK-NEXT: %[[RES:.*]] = llvm.load %[[ALLOCA]] : !llvm.ptr -> i32
  %2 = llvm.load %1 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %2 : i32
}

// -----

// CHECK-LABEL: llvm.func @load_first_field_different_type
llvm.func @load_first_field_different_type() -> f32 {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, i32, i32)> : (i32) -> !llvm.ptr
  // CHECK-NEXT: %[[RES:.*]] = llvm.load %[[ALLOCA]] : !llvm.ptr -> f32
  %2 = llvm.load %1 : !llvm.ptr -> f32
  // CHECK: llvm.return %[[RES]] : f32
  llvm.return %2 : f32
}

// -----

// CHECK-LABEL: llvm.func @load_sub_field
llvm.func @load_sub_field() -> i32 {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i64 : (i32) -> !llvm.ptr
  %1 = llvm.alloca %0 x !llvm.struct<(i64, i32)> : (i32) -> !llvm.ptr
  // CHECK-NEXT: %[[RES:.*]] = llvm.load %[[ALLOCA]]
  %res = llvm.load %1 : !llvm.ptr -> i32
  // CHECK: llvm.return %[[RES]] : i32
  llvm.return %res : i32
}

// -----

// CHECK-LABEL: llvm.func @vector_store_type_mismatch
// CHECK-SAME: %[[ARG:.*]]: vector<4xi32>
llvm.func @vector_store_type_mismatch(%arg: vector<4xi32>) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x vector<4xf32>
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (vector<4xf32>)> : (i32) -> !llvm.ptr
  // CHECK-NEXT: llvm.store %[[ARG]], %[[ALLOCA]]
  llvm.store %arg, %1 : vector<4xi32>, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @store_to_memory
// CHECK-SAME: %[[ARG:.*]]: !llvm.ptr
llvm.func @store_to_memory(%arg: !llvm.ptr) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x !llvm.struct<
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (vector<4xf32>)> : (i32) -> !llvm.ptr
  // CHECK-NEXT: llvm.store %[[ALLOCA]], %[[ARG]]
  llvm.store %1, %arg : !llvm.ptr, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @type_mismatch_array_access
// CHECK-SAME: %[[ARG:.*]]: i32
llvm.func @type_mismatch_array_access(%arg: i32) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i32
  %1 = llvm.alloca %0 x !llvm.struct<(i32, i32, i32)> : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr %1[8] : (!llvm.ptr) -> !llvm.ptr, i8
  // CHECK-NEXT: llvm.store %[[ARG]], %[[ALLOCA]]
  llvm.store %arg, %2 : i32, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @type_mismatch_struct_access
// CHECK-SAME: %[[ARG:.*]]: i32
llvm.func @type_mismatch_struct_access(%arg: i32) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i32
  %1 = llvm.alloca %0 x !llvm.struct<(i32, i32, i32)> : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr %1[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i32, i32)>
  // CHECK-NEXT: llvm.store %[[ARG]], %[[ALLOCA]]
  llvm.store %arg, %2 : i32, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @index_in_final_padding
llvm.func @index_in_final_padding(%arg: i32) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x !llvm.struct<"foo", (i32, i8)>
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, i8)> : (i32) -> !llvm.ptr
  // CHECK: = llvm.getelementptr %[[ALLOCA]][7] : (!llvm.ptr) -> !llvm.ptr, i8
  %2 = llvm.getelementptr %1[7] : (!llvm.ptr) -> !llvm.ptr, i8
  llvm.store %arg, %2 : i32, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @index_out_of_bounds
llvm.func @index_out_of_bounds(%arg: i32) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x !llvm.struct<"foo", (i32, i32)>
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, i32)> : (i32) -> !llvm.ptr
  // CHECK: = llvm.getelementptr %[[ALLOCA]][9] : (!llvm.ptr) -> !llvm.ptr, i8
  %2 = llvm.getelementptr %1[9] : (!llvm.ptr) -> !llvm.ptr, i8
  llvm.store %arg, %2 : i32, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @index_in_padding
llvm.func @index_in_padding(%arg: i16) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x !llvm.struct<"foo", (i16, i32)>
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i16, i32)> : (i32) -> !llvm.ptr
  // CHECK: = llvm.getelementptr %[[ALLOCA]][2] : (!llvm.ptr) -> !llvm.ptr, i8
  %2 = llvm.getelementptr %1[2] : (!llvm.ptr) -> !llvm.ptr, i8
  llvm.store %arg, %2 : i16, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @index_not_in_padding_because_packed
// CHECK-SAME: %[[ARG:.*]]: i16
llvm.func @index_not_in_padding_because_packed(%arg: i16) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", packed (i16, i32)> : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr %1[2] : (!llvm.ptr) -> !llvm.ptr, i8
  // CHECK-NEXT: llvm.store %[[ARG]], %[[ALLOCA]]
  llvm.store %arg, %2 : i16, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @no_crash_on_negative_gep_index
// CHECK-SAME: %[[ARG:.*]]: f16
llvm.func @no_crash_on_negative_gep_index(%arg: f16) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x !llvm.struct<"foo", (i32, i32, i32)>
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, i32, i32)> : (i32) -> !llvm.ptr
  // CHECK: llvm.getelementptr %[[ALLOCA]][-1] : (!llvm.ptr) -> !llvm.ptr, f32
  %2 = llvm.getelementptr %1[-1] : (!llvm.ptr) -> !llvm.ptr, f32
  llvm.store %arg, %2 : f16, !llvm.ptr
  llvm.return
}

// -----

// CHECK-LABEL: llvm.func @out_of_bound_gep_array_access
// CHECK-SAME: %[[ARG:.*]]: i32
llvm.func @out_of_bound_gep_array_access(%arg: i32) {
  %0 = llvm.mlir.constant(1 : i32) : i32
  // CHECK: %[[ALLOCA:.*]] = llvm.alloca %{{.*}} x i32
  %1 = llvm.alloca %0 x !llvm.struct<"foo", (i32, i32)> : (i32) -> !llvm.ptr
  %2 = llvm.getelementptr %1[0, 4] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<4 x i8>
  // CHECK-NEXT: llvm.store %[[ARG]], %[[ALLOCA]]
  llvm.store %arg, %2 : i32, !llvm.ptr
  llvm.return
}
