// RUN: mlir-opt --lower-host-to-llvm %s -verify-diagnostics

module {
// expected-error @+1 {{The module must contain exactly one entry point function}}
  spirv.module Logical GLSL450 {
  }
}
