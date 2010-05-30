// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2006-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2009 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_PARTIALLU_H
#define EIGEN_PARTIALLU_H

/** \ingroup LU_Module
  *
  * \class PartialPivLU
  *
  * \brief LU decomposition of a matrix with partial pivoting, and related features
  *
  * \param MatrixType the type of the matrix of which we are computing the LU decomposition
  *
  * This class represents a LU decomposition of a \b square \b invertible matrix, with partial pivoting: the matrix A
  * is decomposed as A = PLU where L is unit-lower-triangular, U is upper-triangular, and P
  * is a permutation matrix.
  *
  * Typically, partial pivoting LU decomposition is only considered numerically stable for square invertible
  * matrices. Thus LAPACK's dgesv and dgesvx require the matrix to be square and invertible. The present class
  * does the same. It will assert that the matrix is square, but it won't (actually it can't) check that the
  * matrix is invertible: it is your task to check that you only use this decomposition on invertible matrices.
  *
  * The guaranteed safe alternative, working for all matrices, is the full pivoting LU decomposition, provided
  * by class FullPivLU.
  *
  * This is \b not a rank-revealing LU decomposition. Many features are intentionally absent from this class,
  * such as rank computation. If you need these features, use class FullPivLU.
  *
  * This LU decomposition is suitable to invert invertible matrices. It is what MatrixBase::inverse() uses
  * in the general case.
  * On the other hand, it is \b not suitable to determine whether a given matrix is invertible.
  *
  * The data of the LU decomposition can be directly accessed through the methods matrixLU(), permutationP().
  *
  * \sa MatrixBase::partialPivLu(), MatrixBase::determinant(), MatrixBase::inverse(), MatrixBase::computeInverse(), class FullPivLU
  */
template<typename _MatrixType> class PartialPivLU
{
  public:

    typedef _MatrixType MatrixType;
    enum {
      RowsAtCompileTime = MatrixType::RowsAtCompileTime,
      ColsAtCompileTime = MatrixType::ColsAtCompileTime,
      Options = MatrixType::Options,
      MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
      MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime
    };
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef typename ei_traits<MatrixType>::StorageKind StorageKind;
    typedef typename ei_index<StorageKind>::type Index;
    typedef typename ei_plain_col_type<MatrixType, Index>::type PermutationVectorType;
    typedef PermutationMatrix<RowsAtCompileTime, MaxRowsAtCompileTime> PermutationType;


    /**
    * \brief Default Constructor.
    *
    * The default constructor is useful in cases in which the user intends to
    * perform decompositions via PartialPivLU::compute(const MatrixType&).
    */
    PartialPivLU();

    /** \brief Default Constructor with memory preallocation
      *
      * Like the default constructor but with preallocation of the internal data
      * according to the specified problem \a size.
      * \sa PartialPivLU()
      */
    PartialPivLU(Index size);

    /** Constructor.
      *
      * \param matrix the matrix of which to compute the LU decomposition.
      *
      * \warning The matrix should have full rank (e.g. if it's square, it should be invertible).
      * If you need to deal with non-full rank, use class FullPivLU instead.
      */
    PartialPivLU(const MatrixType& matrix);

    PartialPivLU& compute(const MatrixType& matrix);

    /** \returns the LU decomposition matrix: the upper-triangular part is U, the
      * unit-lower-triangular part is L (at least for square matrices; in the non-square
      * case, special care is needed, see the documentation of class FullPivLU).
      *
      * \sa matrixL(), matrixU()
      */
    inline const MatrixType& matrixLU() const
    {
      ei_assert(m_isInitialized && "PartialPivLU is not initialized.");
      return m_lu;
    }

    /** \returns the permutation matrix P.
      */
    inline const PermutationType& permutationP() const
    {
      ei_assert(m_isInitialized && "PartialPivLU is not initialized.");
      return m_p;
    }

    /** This method returns the solution x to the equation Ax=b, where A is the matrix of which
      * *this is the LU decomposition.
      *
      * \param b the right-hand-side of the equation to solve. Can be a vector or a matrix,
      *          the only requirement in order for the equation to make sense is that
      *          b.rows()==A.rows(), where A is the matrix of which *this is the LU decomposition.
      *
      * \returns the solution.
      *
      * Example: \include PartialPivLU_solve.cpp
      * Output: \verbinclude PartialPivLU_solve.out
      *
      * Since this PartialPivLU class assumes anyway that the matrix A is invertible, the solution
      * theoretically exists and is unique regardless of b.
      *
      * \sa TriangularView::solve(), inverse(), computeInverse()
      */
    template<typename Rhs>
    inline const ei_solve_retval<PartialPivLU, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      ei_assert(m_isInitialized && "PartialPivLU is not initialized.");
      return ei_solve_retval<PartialPivLU, Rhs>(*this, b.derived());
    }

    /** \returns the inverse of the matrix of which *this is the LU decomposition.
      *
      * \warning The matrix being decomposed here is assumed to be invertible. If you need to check for
      *          invertibility, use class FullPivLU instead.
      *
      * \sa MatrixBase::inverse(), LU::inverse()
      */
    inline const ei_solve_retval<PartialPivLU,typename MatrixType::IdentityReturnType> inverse() const
    {
      ei_assert(m_isInitialized && "PartialPivLU is not initialized.");
      return ei_solve_retval<PartialPivLU,typename MatrixType::IdentityReturnType>
               (*this, MatrixType::Identity(m_lu.rows(), m_lu.cols()));
    }

    /** \returns the determinant of the matrix of which
      * *this is the LU decomposition. It has only linear complexity
      * (that is, O(n) where n is the dimension of the square matrix)
      * as the LU decomposition has already been computed.
      *
      * \note For fixed-size matrices of size up to 4, MatrixBase::determinant() offers
      *       optimized paths.
      *
      * \warning a determinant can be very big or small, so for matrices
      * of large enough dimension, there is a risk of overflow/underflow.
      *
      * \sa MatrixBase::determinant()
      */
    typename ei_traits<MatrixType>::Scalar determinant() const;

    MatrixType reconstructedMatrix() const;

    inline Index rows() const { return m_lu.rows(); }
    inline Index cols() const { return m_lu.cols(); }

  protected:
    MatrixType m_lu;
    PermutationType m_p;
    PermutationVectorType m_rowsTranspositions;
    Index m_det_p;
    bool m_isInitialized;
};

template<typename MatrixType>
PartialPivLU<MatrixType>::PartialPivLU()
  : m_lu(),
    m_p(),
    m_rowsTranspositions(),
    m_det_p(0),
    m_isInitialized(false)
{
}

template<typename MatrixType>
PartialPivLU<MatrixType>::PartialPivLU(Index size)
  : m_lu(size, size),
    m_p(size),
    m_rowsTranspositions(size),
    m_det_p(0),
    m_isInitialized(false)
{
}

template<typename MatrixType>
PartialPivLU<MatrixType>::PartialPivLU(const MatrixType& matrix)
  : m_lu(matrix.rows(), matrix.rows()),
    m_p(matrix.rows()),
    m_rowsTranspositions(matrix.rows()),
    m_det_p(0),
    m_isInitialized(false)
{
  compute(matrix);
}

/** \internal This is the blocked version of ei_fullpivlu_unblocked() */
template<typename Scalar, int StorageOrder>
struct ei_partial_lu_impl
{
  // FIXME add a stride to Map, so that the following mapping becomes easier,
  // another option would be to create an expression being able to automatically
  // warp any Map, Matrix, and Block expressions as a unique type, but since that's exactly
  // a Map + stride, why not adding a stride to Map, and convenient ctors from a Matrix,
  // and Block.
  typedef Map<Matrix<Scalar, Dynamic, Dynamic, StorageOrder> > MapLU;
  typedef Block<MapLU, Dynamic, Dynamic> MatrixType;
  typedef Block<MatrixType,Dynamic,Dynamic> BlockType;
  typedef typename MatrixType::RealScalar RealScalar;
  typedef typename MatrixType::Index Index;

  /** \internal performs the LU decomposition in-place of the matrix \a lu
    * using an unblocked algorithm.
    *
    * In addition, this function returns the row transpositions in the
    * vector \a row_transpositions which must have a size equal to the number
    * of columns of the matrix \a lu, and an integer \a nb_transpositions
    * which returns the actual number of transpositions.
    *
    * \returns false if some pivot is exactly zero, in which case the matrix is left with
    *          undefined coefficients (to avoid generating inf/nan values). Returns true
    *          otherwise.
    */
  static bool unblocked_lu(MatrixType& lu, Index* row_transpositions, Index& nb_transpositions)
  {
    const Index rows = lu.rows();
    const Index size = std::min(lu.rows(),lu.cols());
    nb_transpositions = 0;
    for(Index k = 0; k < size; ++k)
    {
      Index row_of_biggest_in_col;
      RealScalar biggest_in_corner
        = lu.col(k).tail(rows-k).cwiseAbs().maxCoeff(&row_of_biggest_in_col);
      row_of_biggest_in_col += k;

      if(biggest_in_corner == 0) // the pivot is exactly zero: the matrix is singular
      {
        // end quickly, avoid generating inf/nan values. Although in this unblocked_lu case
        // the result is still valid, there's no need to boast about it because
        // the blocked_lu code can't guarantee the same.
        // before exiting, make sure to initialize the still uninitialized row_transpositions
        // in a sane state without destroying what we already have.
        for(Index i = k; i < size; i++)
          row_transpositions[i] = i;
        return false;
      }

      row_transpositions[k] = row_of_biggest_in_col;

      if(k != row_of_biggest_in_col)
      {
        lu.row(k).swap(lu.row(row_of_biggest_in_col));
        ++nb_transpositions;
      }

      if(k<rows-1)
      {
        Index rrows = rows-k-1;
        Index rsize = size-k-1;
        lu.col(k).tail(rrows) /= lu.coeff(k,k);
        lu.bottomRightCorner(rrows,rsize).noalias() -= lu.col(k).tail(rrows) * lu.row(k).tail(rsize);
      }
    }
    return true;
  }

  /** \internal performs the LU decomposition in-place of the matrix represented
    * by the variables \a rows, \a cols, \a lu_data, and \a lu_stride using a
    * recursive, blocked algorithm.
    *
    * In addition, this function returns the row transpositions in the
    * vector \a row_transpositions which must have a size equal to the number
    * of columns of the matrix \a lu, and an integer \a nb_transpositions
    * which returns the actual number of transpositions.
    *
    * \returns false if some pivot is exactly zero, in which case the matrix is left with
    *          undefined coefficients (to avoid generating inf/nan values). Returns true
    *          otherwise.
    *
    * \note This very low level interface using pointers, etc. is to:
    *   1 - reduce the number of instanciations to the strict minimum
    *   2 - avoid infinite recursion of the instanciations with Block<Block<Block<...> > >
    */
  static bool blocked_lu(Index rows, Index cols, Scalar* lu_data, Index luStride, Index* row_transpositions, Index& nb_transpositions, Index maxBlockSize=256)
  {
    MapLU lu1(lu_data,StorageOrder==RowMajor?rows:luStride,StorageOrder==RowMajor?luStride:cols);
    MatrixType lu(lu1,0,0,rows,cols);

    const Index size = std::min(rows,cols);

    // if the matrix is too small, no blocking:
    if(size<=16)
    {
      return unblocked_lu(lu, row_transpositions, nb_transpositions);
    }

    // automatically adjust the number of subdivisions to the size
    // of the matrix so that there is enough sub blocks:
    Index blockSize;
    {
      blockSize = size/8;
      blockSize = (blockSize/16)*16;
      blockSize = std::min(std::max(blockSize,Index(8)), maxBlockSize);
    }

    nb_transpositions = 0;
    for(Index k = 0; k < size; k+=blockSize)
    {
      Index bs = std::min(size-k,blockSize); // actual size of the block
      Index trows = rows - k - bs; // trailing rows
      Index tsize = size - k - bs; // trailing size

      // partition the matrix:
      //        A00 | A01 | A02
      // lu  =  A10 | A11 | A12
      //        A20 | A21 | A22
      BlockType A_0(lu,0,0,rows,k);
      BlockType A_2(lu,0,k+bs,rows,tsize);
      BlockType A11(lu,k,k,bs,bs);
      BlockType A12(lu,k,k+bs,bs,tsize);
      BlockType A21(lu,k+bs,k,trows,bs);
      BlockType A22(lu,k+bs,k+bs,trows,tsize);

      Index nb_transpositions_in_panel;
      // recursively calls the blocked LU algorithm with a very small
      // blocking size:
      if(!blocked_lu(trows+bs, bs, &lu.coeffRef(k,k), luStride,
                   row_transpositions+k, nb_transpositions_in_panel, 16))
      {
        // end quickly with undefined coefficients, just avoid generating inf/nan values.
        // before exiting, make sure to initialize the still uninitialized row_transpositions
        // in a sane state without destroying what we already have.
        for(Index i=k; i<size; ++i)
          row_transpositions[i] = i;
        return false;
      }
      nb_transpositions += nb_transpositions_in_panel;

      // update permutations and apply them to A10
      for(Index i=k; i<k+bs; ++i)
      {
        Index piv = (row_transpositions[i] += k);
        A_0.row(i).swap(A_0.row(piv));
      }

      if(trows)
      {
        // apply permutations to A_2
        for(Index i=k;i<k+bs; ++i)
          A_2.row(i).swap(A_2.row(row_transpositions[i]));

        // A12 = A11^-1 A12
        A11.template triangularView<UnitLower>().solveInPlace(A12);

        A22.noalias() -= A21 * A12;
      }
    }
    return true;
  }
};

/** \internal performs the LU decomposition with partial pivoting in-place.
  */
template<typename MatrixType, typename IntVector>
void ei_partial_lu_inplace(MatrixType& lu, IntVector& row_transpositions, typename MatrixType::Index& nb_transpositions)
{
  ei_assert(lu.cols() == row_transpositions.size());
  ei_assert((&row_transpositions.coeffRef(1)-&row_transpositions.coeffRef(0)) == 1);

  ei_partial_lu_impl
    <typename MatrixType::Scalar, MatrixType::Flags&RowMajorBit?RowMajor:ColMajor>
    ::blocked_lu(lu.rows(), lu.cols(), &lu.coeffRef(0,0), lu.outerStride(), &row_transpositions.coeffRef(0), nb_transpositions);
}

template<typename MatrixType>
PartialPivLU<MatrixType>& PartialPivLU<MatrixType>::compute(const MatrixType& matrix)
{
  m_lu = matrix;

  ei_assert(matrix.rows() == matrix.cols() && "PartialPivLU is only for square (and moreover invertible) matrices");
  const Index size = matrix.rows();

  m_rowsTranspositions.resize(size);

  Index nb_transpositions;
  ei_partial_lu_inplace(m_lu, m_rowsTranspositions, nb_transpositions);
  m_det_p = (nb_transpositions%2) ? -1 : 1;

  m_p.setIdentity(size);
  for(Index k = size-1; k >= 0; --k)
    m_p.applyTranspositionOnTheRight(k, m_rowsTranspositions.coeff(k));

  m_isInitialized = true;
  return *this;
}

template<typename MatrixType>
typename ei_traits<MatrixType>::Scalar PartialPivLU<MatrixType>::determinant() const
{
  ei_assert(m_isInitialized && "PartialPivLU is not initialized.");
  return Scalar(m_det_p) * m_lu.diagonal().prod();
}

/** \returns the matrix represented by the decomposition,
 * i.e., it returns the product: P^{-1} L U.
 * This function is provided for debug purpose. */
template<typename MatrixType>
MatrixType PartialPivLU<MatrixType>::reconstructedMatrix() const
{
  ei_assert(m_isInitialized && "LU is not initialized.");
  // LU
  MatrixType res = m_lu.template triangularView<UnitLower>().toDenseMatrix()
                 * m_lu.template triangularView<Upper>();

  // P^{-1}(LU)
  res = m_p.inverse() * res;

  return res;
}

/***** Implementation of solve() *****************************************************/

template<typename _MatrixType, typename Rhs>
struct ei_solve_retval<PartialPivLU<_MatrixType>, Rhs>
  : ei_solve_retval_base<PartialPivLU<_MatrixType>, Rhs>
{
  EIGEN_MAKE_SOLVE_HELPERS(PartialPivLU<_MatrixType>,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    /* The decomposition PA = LU can be rewritten as A = P^{-1} L U.
    * So we proceed as follows:
    * Step 1: compute c = Pb.
    * Step 2: replace c by the solution x to Lx = c.
    * Step 3: replace c by the solution x to Ux = c.
    */

    ei_assert(rhs().rows() == dec().matrixLU().rows());

    // Step 1
    dst = dec().permutationP() * rhs();

    // Step 2
    dec().matrixLU().template triangularView<UnitLower>().solveInPlace(dst);

    // Step 3
    dec().matrixLU().template triangularView<Upper>().solveInPlace(dst);
  }
};

/******** MatrixBase methods *******/

/** \lu_module
  *
  * \return the partial-pivoting LU decomposition of \c *this.
  *
  * \sa class PartialPivLU
  */
template<typename Derived>
inline const PartialPivLU<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::partialPivLu() const
{
  return PartialPivLU<PlainObject>(eval());
}

/** \lu_module
  *
  * Synonym of partialPivLu().
  *
  * \return the partial-pivoting LU decomposition of \c *this.
  *
  * \sa class PartialPivLU
  */
template<typename Derived>
inline const PartialPivLU<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::lu() const
{
  return PartialPivLU<PlainObject>(eval());
}

#endif // EIGEN_PARTIALLU_H
