// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
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

#ifndef EIGEN_TRIANGULAR_SOLVER_MATRIX_H
#define EIGEN_TRIANGULAR_SOLVER_MATRIX_H

// if the rhs is row major, let's transpose the product
template <typename Scalar, typename Index, int Side, int Mode, bool Conjugate, int TriStorageOrder>
struct ei_triangular_solve_matrix<Scalar,Index,Side,Mode,Conjugate,TriStorageOrder,RowMajor>
{
  static EIGEN_DONT_INLINE void run(
    Index size, Index cols,
    const Scalar*  tri, Index triStride,
    Scalar* _other, Index otherStride)
  {
    ei_triangular_solve_matrix<
      Scalar, Index, Side==OnTheLeft?OnTheRight:OnTheLeft,
      (Mode&UnitDiag) | ((Mode&Upper) ? Lower : Upper),
      NumTraits<Scalar>::IsComplex && Conjugate,
      TriStorageOrder==RowMajor ? ColMajor : RowMajor, ColMajor>
      ::run(size, cols, tri, triStride, _other, otherStride);
  }
};

/* Optimized triangular solver with multiple right hand side and the triangular matrix on the left
 */
template <typename Scalar, typename Index, int Mode, bool Conjugate, int TriStorageOrder>
struct ei_triangular_solve_matrix<Scalar,Index,OnTheLeft,Mode,Conjugate,TriStorageOrder,ColMajor>
{
  static EIGEN_DONT_INLINE void run(
    Index size, Index otherSize,
    const Scalar* _tri, Index triStride,
    Scalar* _other, Index otherStride)
  {
    Index cols = otherSize;
    ei_const_blas_data_mapper<Scalar, Index, TriStorageOrder> tri(_tri,triStride);
    ei_blas_data_mapper<Scalar, Index, ColMajor> other(_other,otherStride);

    typedef ei_product_blocking_traits<Scalar> Blocking;
    enum {
      SmallPanelWidth   = EIGEN_PLAIN_ENUM_MAX(Blocking::mr,Blocking::nr),
      IsLower = (Mode&Lower) == Lower
    };

    Index kc = size; // cache block size along the K direction
    Index mc = size;  // cache block size along the M direction
    Index nc = cols;  // cache block size along the N direction
    computeProductBlockingSizes<Scalar,Scalar,4>(kc, mc, nc);

    Scalar* blockA = ei_aligned_stack_new(Scalar, kc*mc);
    std::size_t sizeB = kc*Blocking::PacketSize*Blocking::nr + kc*cols;
    Scalar* allocatedBlockB = ei_aligned_stack_new(Scalar, sizeB);
    Scalar* blockB = allocatedBlockB + kc*Blocking::PacketSize*Blocking::nr;

    ei_conj_if<Conjugate> conj;
    ei_gebp_kernel<Scalar, Index, Blocking::mr, Blocking::nr, ei_conj_helper<Conjugate,false> > gebp_kernel;
    ei_gemm_pack_lhs<Scalar, Index, Blocking::mr,TriStorageOrder> pack_lhs;
    ei_gemm_pack_rhs<Scalar, Index, Blocking::nr, ColMajor, true> pack_rhs;

    for(Index k2=IsLower ? 0 : size;
        IsLower ? k2<size : k2>0;
        IsLower ? k2+=kc : k2-=kc)
    {
      const Index actual_kc = std::min(IsLower ? size-k2 : k2, kc);

      // We have selected and packed a big horizontal panel R1 of rhs. Let B be the packed copy of this panel,
      // and R2 the remaining part of rhs. The corresponding vertical panel of lhs is split into
      // A11 (the triangular part) and A21 the remaining rectangular part.
      // Then the high level algorithm is:
      //  - B = R1                    => general block copy (done during the next step)
      //  - R1 = L1^-1 B              => tricky part
      //  - update B from the new R1  => actually this has to be performed continuously during the above step
      //  - R2 = L2 * B               => GEPP

      // The tricky part: compute R1 = L1^-1 B while updating B from R1
      // The idea is to split L1 into multiple small vertical panels.
      // Each panel can be split into a small triangular part A1 which is processed without optimization,
      // and the remaining small part A2 which is processed using gebp with appropriate block strides
      {
        // for each small vertical panels of lhs
        for (Index k1=0; k1<actual_kc; k1+=SmallPanelWidth)
        {
          Index actualPanelWidth = std::min<Index>(actual_kc-k1, SmallPanelWidth);
          // tr solve
          for (Index k=0; k<actualPanelWidth; ++k)
          {
            // TODO write a small kernel handling this (can be shared with trsv)
            Index i  = IsLower ? k2+k1+k : k2-k1-k-1;
            Index s  = IsLower ? k2+k1 : i+1;
            Index rs = actualPanelWidth - k - 1; // remaining size

            Scalar a = (Mode & UnitDiag) ? Scalar(1) : Scalar(1)/conj(tri(i,i));
            for (Index j=0; j<cols; ++j)
            {
              if (TriStorageOrder==RowMajor)
              {
                Scalar b = 0;
                const Scalar* l = &tri(i,s);
                Scalar* r = &other(s,j);
                for (Index i3=0; i3<k; ++i3)
                  b += conj(l[i3]) * r[i3];

                other(i,j) = (other(i,j) - b)*a;
              }
              else
              {
                Index s = IsLower ? i+1 : i-rs;
                Scalar b = (other(i,j) *= a);
                Scalar* r = &other(s,j);
                const Scalar* l = &tri(s,i);
                for (Index i3=0;i3<rs;++i3)
                  r[i3] -= b * conj(l[i3]);
              }
            }
          }

          Index lengthTarget = actual_kc-k1-actualPanelWidth;
          Index startBlock   = IsLower ? k2+k1 : k2-k1-actualPanelWidth;
          Index blockBOffset = IsLower ? k1 : lengthTarget;

          // update the respective rows of B from other
          pack_rhs(blockB, _other+startBlock, otherStride, -1, actualPanelWidth, cols, actual_kc, blockBOffset);

          // GEBP
          if (lengthTarget>0)
          {
            Index startTarget  = IsLower ? k2+k1+actualPanelWidth : k2-actual_kc;

            pack_lhs(blockA, &tri(startTarget,startBlock), triStride, actualPanelWidth, lengthTarget);

            gebp_kernel(_other+startTarget, otherStride, blockA, blockB, lengthTarget, actualPanelWidth, cols,
                        actualPanelWidth, actual_kc, 0, blockBOffset);
          }
        }
      }

      // R2 = A2 * B => GEPP
      {
        Index start = IsLower ? k2+kc : 0;
        Index end   = IsLower ? size : k2-kc;
        for(Index i2=start; i2<end; i2+=mc)
        {
          const Index actual_mc = std::min(mc,end-i2);
          if (actual_mc>0)
          {
            pack_lhs(blockA, &tri(i2, IsLower ? k2 : k2-kc), triStride, actual_kc, actual_mc);

            gebp_kernel(_other+i2, otherStride, blockA, blockB, actual_mc, actual_kc, cols);
          }
        }
      }
    }

    ei_aligned_stack_delete(Scalar, blockA, kc*mc);
    ei_aligned_stack_delete(Scalar, allocatedBlockB, sizeB);
  }
};

/* Optimized triangular solver with multiple left hand sides and the trinagular matrix on the right
 */
template <typename Scalar, typename Index, int Mode, bool Conjugate, int TriStorageOrder>
struct ei_triangular_solve_matrix<Scalar,Index,OnTheRight,Mode,Conjugate,TriStorageOrder,ColMajor>
{
  static EIGEN_DONT_INLINE void run(
    Index size, Index otherSize,
    const Scalar* _tri, Index triStride,
    Scalar* _other, Index otherStride)
  {
    Index rows = otherSize;
    ei_const_blas_data_mapper<Scalar, Index, TriStorageOrder> rhs(_tri,triStride);
    ei_blas_data_mapper<Scalar, Index, ColMajor> lhs(_other,otherStride);

    typedef ei_product_blocking_traits<Scalar> Blocking;
    enum {
      RhsStorageOrder   = TriStorageOrder,
      SmallPanelWidth   = EIGEN_PLAIN_ENUM_MAX(Blocking::mr,Blocking::nr),
      IsLower = (Mode&Lower) == Lower
    };

//     Index kc = std::min<Index>(Blocking::Max_kc/4,size); // cache block size along the K direction
//     Index mc = std::min<Index>(Blocking::Max_mc,size);   // cache block size along the M direction
    // check that !!!!
    Index kc = size; // cache block size along the K direction
    Index mc = size;  // cache block size along the M direction
    Index nc = rows;  // cache block size along the N direction
    computeProductBlockingSizes<Scalar,Scalar,4>(kc, mc, nc);

    Scalar* blockA = ei_aligned_stack_new(Scalar, kc*mc);
    std::size_t sizeB = kc*Blocking::PacketSize*Blocking::nr + kc*size;
    Scalar* allocatedBlockB = ei_aligned_stack_new(Scalar, sizeB);
    Scalar* blockB = allocatedBlockB + kc*Blocking::PacketSize*Blocking::nr;

    ei_conj_if<Conjugate> conj;
    ei_gebp_kernel<Scalar, Index, Blocking::mr, Blocking::nr, ei_conj_helper<false,Conjugate> > gebp_kernel;
    ei_gemm_pack_rhs<Scalar, Index, Blocking::nr,RhsStorageOrder> pack_rhs;
    ei_gemm_pack_rhs<Scalar, Index, Blocking::nr,RhsStorageOrder,true> pack_rhs_panel;
    ei_gemm_pack_lhs<Scalar, Index, Blocking::mr, ColMajor, false, true> pack_lhs_panel;

    for(Index k2=IsLower ? size : 0;
        IsLower ? k2>0 : k2<size;
        IsLower ? k2-=kc : k2+=kc)
    {
      const Index actual_kc = std::min(IsLower ? k2 : size-k2, kc);
      Index actual_k2 = IsLower ? k2-actual_kc : k2 ;

      Index startPanel = IsLower ? 0 : k2+actual_kc;
      Index rs = IsLower ? actual_k2 : size - actual_k2 - actual_kc;
      Scalar* geb = blockB+actual_kc*actual_kc;

      if (rs>0) pack_rhs(geb, &rhs(actual_k2,startPanel), triStride, -1, actual_kc, rs);

      // triangular packing (we only pack the panels off the diagonal,
      // neglecting the blocks overlapping the diagonal
      {
        for (Index j2=0; j2<actual_kc; j2+=SmallPanelWidth)
        {
          Index actualPanelWidth = std::min<Index>(actual_kc-j2, SmallPanelWidth);
          Index actual_j2 = actual_k2 + j2;
          Index panelOffset = IsLower ? j2+actualPanelWidth : 0;
          Index panelLength = IsLower ? actual_kc-j2-actualPanelWidth : j2;

          if (panelLength>0)
          pack_rhs_panel(blockB+j2*actual_kc,
                         &rhs(actual_k2+panelOffset, actual_j2), triStride, -1,
                         panelLength, actualPanelWidth,
                         actual_kc, panelOffset);
        }
      }

      for(Index i2=0; i2<rows; i2+=mc)
      {
        const Index actual_mc = std::min(mc,rows-i2);

        // triangular solver kernel
        {
          // for each small block of the diagonal (=> vertical panels of rhs)
          for (Index j2 = IsLower
                      ? (actual_kc - ((actual_kc%SmallPanelWidth) ? Index(actual_kc%SmallPanelWidth)
                                                                  : Index(SmallPanelWidth)))
                      : 0;
               IsLower ? j2>=0 : j2<actual_kc;
               IsLower ? j2-=SmallPanelWidth : j2+=SmallPanelWidth)
          {
            Index actualPanelWidth = std::min<Index>(actual_kc-j2, SmallPanelWidth);
            Index absolute_j2 = actual_k2 + j2;
            Index panelOffset = IsLower ? j2+actualPanelWidth : 0;
            Index panelLength = IsLower ? actual_kc - j2 - actualPanelWidth : j2;

            // GEBP
            if(panelLength>0)
            {
              gebp_kernel(&lhs(i2,absolute_j2), otherStride,
                          blockA, blockB+j2*actual_kc,
                          actual_mc, panelLength, actualPanelWidth,
                          actual_kc, actual_kc, // strides
                          panelOffset, panelOffset, // offsets
                          allocatedBlockB);  // workspace
            }

            // unblocked triangular solve
            for (Index k=0; k<actualPanelWidth; ++k)
            {
              Index j = IsLower ? absolute_j2+actualPanelWidth-k-1 : absolute_j2+k;

              Scalar* r = &lhs(i2,j);
              for (Index k3=0; k3<k; ++k3)
              {
                Scalar b = conj(rhs(IsLower ? j+1+k3 : absolute_j2+k3,j));
                Scalar* a = &lhs(i2,IsLower ? j+1+k3 : absolute_j2+k3);
                for (Index i=0; i<actual_mc; ++i)
                  r[i] -= a[i] * b;
              }
              Scalar b = (Mode & UnitDiag) ? Scalar(1) : Scalar(1)/conj(rhs(j,j));
              for (Index i=0; i<actual_mc; ++i)
                r[i] *= b;
            }

            // pack the just computed part of lhs to A
            pack_lhs_panel(blockA, _other+absolute_j2*otherStride+i2, otherStride,
                           actualPanelWidth, actual_mc,
                           actual_kc, j2);
          }
        }

        if (rs>0)
          gebp_kernel(_other+i2+startPanel*otherStride, otherStride, blockA, geb,
                      actual_mc, actual_kc, rs,
                      -1, -1, 0, 0, allocatedBlockB);
      }
    }

    ei_aligned_stack_delete(Scalar, blockA, kc*mc);
    ei_aligned_stack_delete(Scalar, allocatedBlockB, sizeB);
  }
};

#endif // EIGEN_TRIANGULAR_SOLVER_MATRIX_H
