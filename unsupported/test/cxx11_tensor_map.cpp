// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2014 Benoit Steiner <benoit.steiner.goog@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "main.h"

#include <Eigen/CXX11/Tensor>

using Eigen::Tensor;
using Eigen::RowMajor;

static void test_1d()
{
  Tensor<int, 1> vec1(6);
  Tensor<int, 1, RowMajor> vec2(6);

  TensorMap<Tensor<const int, 1>> vec3(vec1.data(), 6);
  TensorMap<Tensor<const int, 1, RowMajor>> vec4(vec2.data(), 6);

  vec1(0) = 4;  vec2(0) = 0;
  vec1(1) = 8;  vec2(1) = 1;
  vec1(2) = 15; vec2(2) = 2;
  vec1(3) = 16; vec2(3) = 3;
  vec1(4) = 23; vec2(4) = 4;
  vec1(5) = 42; vec2(5) = 5;

  VERIFY_IS_EQUAL(vec1.size(), 6);
  VERIFY_IS_EQUAL(vec1.dimension(0), 6);

  VERIFY_IS_EQUAL(vec3(0), 4);
  VERIFY_IS_EQUAL(vec3(1), 8);
  VERIFY_IS_EQUAL(vec3(2), 15);
  VERIFY_IS_EQUAL(vec3(3), 16);
  VERIFY_IS_EQUAL(vec3(4), 23);
  VERIFY_IS_EQUAL(vec3(5), 42);

  VERIFY_IS_EQUAL(vec4(0), 0);
  VERIFY_IS_EQUAL(vec4(1), 1);
  VERIFY_IS_EQUAL(vec4(2), 2);
  VERIFY_IS_EQUAL(vec4(3), 3);
  VERIFY_IS_EQUAL(vec4(4), 4);
  VERIFY_IS_EQUAL(vec4(5), 5);
}

static void test_2d()
{
  Tensor<int, 2> mat1(2,3);
  Tensor<int, 2, RowMajor> mat2(2,3);

  mat1(0,0) = 0;
  mat1(0,1) = 1;
  mat1(0,2) = 2;
  mat1(1,0) = 3;
  mat1(1,1) = 4;
  mat1(1,2) = 5;

  mat2(0,0) = 0;
  mat2(0,1) = 1;
  mat2(0,2) = 2;
  mat2(1,0) = 3;
  mat2(1,1) = 4;
  mat2(1,2) = 5;

  TensorMap<Tensor<const int, 2>> mat3(mat1.data(), 2, 3);
  TensorMap<Tensor<const int, 2, RowMajor>> mat4(mat2.data(), 2, 3);

  VERIFY_IS_EQUAL(mat3.size(), 6);
  VERIFY_IS_EQUAL(mat3.dimension(0), 2);
  VERIFY_IS_EQUAL(mat3.dimension(1), 3);

  VERIFY_IS_EQUAL(mat4.size(), 6);
  VERIFY_IS_EQUAL(mat4.dimension(0), 2);
  VERIFY_IS_EQUAL(mat4.dimension(1), 3);

  VERIFY_IS_EQUAL(mat3(0,0), 0);
  VERIFY_IS_EQUAL(mat3(0,1), 1);
  VERIFY_IS_EQUAL(mat3(0,2), 2);
  VERIFY_IS_EQUAL(mat3(1,0), 3);
  VERIFY_IS_EQUAL(mat3(1,1), 4);
  VERIFY_IS_EQUAL(mat3(1,2), 5);

  VERIFY_IS_EQUAL(mat4(0,0), 0);
  VERIFY_IS_EQUAL(mat4(0,1), 1);
  VERIFY_IS_EQUAL(mat4(0,2), 2);
  VERIFY_IS_EQUAL(mat4(1,0), 3);
  VERIFY_IS_EQUAL(mat4(1,1), 4);
  VERIFY_IS_EQUAL(mat4(1,2), 5);
}

static void test_3d()
{
  Tensor<int, 3> mat1(2,3,7);
  Tensor<int, 3, RowMajor> mat2(2,3,7);

  int val = 0;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 7; ++k) {
        mat1(i,j,k) = val;
        mat2(i,j,k) = val;
        val++;
      }
    }
  }

  TensorMap<Tensor<const int, 3>> mat3(mat1.data(), 2, 3, 7);
  TensorMap<Tensor<const int, 3, RowMajor>> mat4(mat2.data(), 2, 3, 7);

  VERIFY_IS_EQUAL(mat3.size(), 2*3*7);
  VERIFY_IS_EQUAL(mat3.dimension(0), 2);
  VERIFY_IS_EQUAL(mat3.dimension(1), 3);
  VERIFY_IS_EQUAL(mat3.dimension(2), 7);

  VERIFY_IS_EQUAL(mat4.size(), 2*3*7);
  VERIFY_IS_EQUAL(mat4.dimension(0), 2);
  VERIFY_IS_EQUAL(mat4.dimension(1), 3);
  VERIFY_IS_EQUAL(mat4.dimension(2), 7);

  val = 0;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 7; ++k) {
        VERIFY_IS_EQUAL(mat3(i,j,k), val);
        VERIFY_IS_EQUAL(mat4(i,j,k), val);
        val++;
      }
    }
  }
}


void test_cxx11_tensor_map()
{
  CALL_SUBTEST(test_1d());
  CALL_SUBTEST(test_2d());
  CALL_SUBTEST(test_3d());
}
