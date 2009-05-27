// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009 Hauke Heibel <hauke.heibel@gmail.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_UMEYAMA_H
#define EIGEN_UMEYAMA_H

// This file requires the user to include 
// * Eigen/Core
// * Eigen/LU 
// * Eigen/SVD
// * Eigen/Array

#ifndef EIGEN_PARSED_BY_DOXYGEN

// These helpers are required since it allows to use mixed types as parameters
// for the Umeyama. The problem with mixed parameters is that the return type
// cannot trivially be deduced when float and double types are mixed.
namespace
{
  // Compile time return type deduction for different MatrixBase types.
  // Different means here different alignment and parameters but the same underlying
  // real scalar type.
  template<typename MatrixType, typename OtherMatrixType>
  struct ei_umeyama_transform_matrix_type
  {    
    enum {
      MinRowsAtCompileTime = EIGEN_ENUM_MIN(MatrixType::RowsAtCompileTime, OtherMatrixType::RowsAtCompileTime),

      // When possible we want to choose some small fixed size value since the result
      // is likely to fit on the stack.
      HomogeneousDimension = EIGEN_ENUM_MIN(MinRowsAtCompileTime+1, Dynamic)
    };

    typedef Matrix<typename ei_traits<MatrixType>::Scalar,
      HomogeneousDimension,
      HomogeneousDimension,
      AutoAlign | (ei_traits<MatrixType>::Flags & RowMajorBit ? RowMajor : ColMajor),
      HomogeneousDimension, 
      HomogeneousDimension
    > type;
  };
}

#endif

/**
* \geometry_module \ingroup Geometry_Module
*
* \brief Returns the transformation between two point sets.
*
* The algorithm is based on:
* "Least-squares estimation of transformation parameters between two point patterns",
* Shinji Umeyama, PAMI 1991, DOI: 10.1109/34.88573
*
* It estimates parameters \f$ c, \mathbf{R}, \f$ and \f$ \mathbf{t} \f$ such that
* \f{align*}
*   \frac{1}{n} \sum_{i=1}^n \vert\vert y_i - (c\mathbf{R}x_i + \mathbf{t}) \vert\vert_2^2
* \f}
* is minimized.
*
* The algorithm is based on the analysis of the covariance matrix
* \f$ \Sigma_{\mathbf{x}\mathbf{y}} \in \mathbb{R}^{d \times d} \f$
* of the input point sets \f$ \mathbf{x} \f$ and \f$ \mathbf{y} \f$ where 
* \f$d\f$ is corresponding to the dimension (which is typically small).
* The analysis is involving the SVD having a complexity of \f$O(d^3)\f$
* though the actual computational effort lies in the covariance
* matrix computation which has an asymptotic lower bound of \f$O(dm)\f$ when 
* the input point sets have dimension \f$d \times m\f$.
*
* Currently the method is working only for floating point matrices.
*
* \todo Should the return type of umeyama() become a Transform?
*
* \param src Source points \f$ \mathbf{x} = \left( x_1, \hdots, x_n \right) \f$.
* \param dst Destination points \f$ \mathbf{y} = \left( y_1, \hdots, y_n \right) \f$.
* \param with_scaling Sets \f$ c=1 \f$ when <code>false</code> is passed.
* \return The homogeneous transformation 
* \f{align*}
*   T = \begin{bmatrix} c\mathbf{R} & \mathbf{t} \\ \mathbf{0} & 1 \end{bmatrix}
* \f}
* minimizing the resudiual above. This transformation is always returned as an 
* Eigen::Matrix.
*/
template <typename Derived, typename OtherDerived>
typename ei_umeyama_transform_matrix_type<Derived, OtherDerived>::type
umeyama(const MatrixBase<Derived>& src, const MatrixBase<OtherDerived>& dst, bool with_scaling = true)
{
  typedef typename ei_umeyama_transform_matrix_type<Derived, OtherDerived>::type TransformationMatrixType;
  typedef typename ei_traits<TransformationMatrixType>::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;

  EIGEN_STATIC_ASSERT(!NumTraits<Scalar>::IsComplex, NUMERIC_TYPE_MUST_BE_REAL)
  EIGEN_STATIC_ASSERT((ei_is_same_type<Scalar, typename ei_traits<OtherDerived>::Scalar>::ret),
    YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

  enum { Dimension = EIGEN_ENUM_MIN(Derived::RowsAtCompileTime, OtherDerived::RowsAtCompileTime) };

  typedef Matrix<Scalar, Dimension, 1> VectorType;
  typedef typename ei_plain_matrix_type<Derived>::type MatrixType;
  typedef typename ei_plain_matrix_type_row_major<Derived>::type RowMajorMatrixType;

  const int m = src.rows(); // dimension
  const int n = src.cols(); // number of measurements

  // required for demeaning ...
  const RealScalar one_over_n = 1 / static_cast<RealScalar>(n);

  // computation of mean
  const VectorType src_mean = src.rowwise().sum() * one_over_n;
  const VectorType dst_mean = dst.rowwise().sum() * one_over_n;

  // demeaning of src and dst points
  RowMajorMatrixType src_demean(m,n);
  RowMajorMatrixType dst_demean(m,n);
  for (int i=0; i<n; ++i)
  {
    src_demean.col(i) = src.col(i) - src_mean;
    dst_demean.col(i) = dst.col(i) - dst_mean;
  }

  // Eq. (36)-(37)
  const Scalar src_var = src_demean.rowwise().squaredNorm().sum() * one_over_n;
  // const Scalar dst_var = dst_demean.rowwise().squaredNorm().sum() * one_over_n;

  // Eq. (38)
  const MatrixType sigma = (dst_demean*src_demean.transpose()).lazy() * one_over_n;

  SVD<MatrixType> svd(sigma);

  // Initialize the resulting transformation with an identity matrix...
  TransformationMatrixType Rt = TransformationMatrixType::Identity(m+1,m+1);

  // Eq. (39)
  VectorType S = VectorType::Ones(m);
  if (sigma.determinant()<0) S(m-1) = -1;

  // Eq. (40) and (43)
  const VectorType& d = svd.singularValues();
  int rank = 0; for (int i=0; i<m; ++i) if (!ei_isMuchSmallerThan(d.coeff(i),d.coeff(0))) ++rank;
  if (rank == m-1) {
    if ( svd.matrixU().determinant() * svd.matrixV().determinant() > 0 ) {
      Rt.block(0,0,m,m) = (svd.matrixU()*svd.matrixV().transpose()).lazy();
    } else {
      const Scalar s = S(m-1); S(m-1) = -1;
      Rt.block(0,0,m,m) = (svd.matrixU() * S.asDiagonal() * svd.matrixV().transpose()).lazy();
      S(m-1) = s;
    }
  } else {
    Rt.block(0,0,m,m) = (svd.matrixU() * S.asDiagonal() * svd.matrixV().transpose()).lazy();
  }

  // Eq. (42)
  const Scalar c = 1/src_var * svd.singularValues().dot(S);

  // Eq. (41)
  // TODO: lazyness does not make much sense over here, right?
  Rt.col(m).segment(0,m) = dst_mean - c*Rt.block(0,0,m,m)*src_mean;

  if (with_scaling) Rt.block(0,0,m,m) *= c;

  return Rt;
}

#endif // EIGEN_UMEYAMA_H
