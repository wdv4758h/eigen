// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2014 Benoit Steiner <benoit.steiner.goog@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_CXX11_TENSOR_TENSOR_BROADCASTING_H
#define EIGEN_CXX11_TENSOR_TENSOR_BROADCASTING_H

namespace Eigen {

/** \class TensorBroadcasting
  * \ingroup CXX11_Tensor_Module
  *
  * \brief Tensor broadcasting class.
  *
  *
  */
namespace internal {
template<typename Broadcast, typename XprType>
struct traits<TensorBroadcastingOp<Broadcast, XprType> > : public traits<XprType>
{
  typedef typename XprType::Scalar Scalar;
  typedef traits<XprType> XprTraits;
  typedef typename packet_traits<Scalar>::type Packet;
  typedef typename XprTraits::StorageKind StorageKind;
  typedef typename XprTraits::Index Index;
  typedef typename XprType::Nested Nested;
  typedef typename remove_reference<Nested>::type _Nested;
};

template<typename Broadcast, typename XprType>
struct eval<TensorBroadcastingOp<Broadcast, XprType>, Eigen::Dense>
{
  typedef const TensorBroadcastingOp<Broadcast, XprType>& type;
};

template<typename Broadcast, typename XprType>
struct nested<TensorBroadcastingOp<Broadcast, XprType>, 1, typename eval<TensorBroadcastingOp<Broadcast, XprType> >::type>
{
  typedef TensorBroadcastingOp<Broadcast, XprType> type;
};

}  // end namespace internal



template<typename Broadcast, typename XprType>
class TensorBroadcastingOp : public TensorBase<TensorBroadcastingOp<Broadcast, XprType>, ReadOnlyAccessors>
{
  public:
  typedef typename Eigen::internal::traits<TensorBroadcastingOp>::Scalar Scalar;
  typedef typename Eigen::internal::traits<TensorBroadcastingOp>::Packet Packet;
  typedef typename Eigen::NumTraits<Scalar>::Real RealScalar;
  typedef typename XprType::CoeffReturnType CoeffReturnType;
  typedef typename XprType::PacketReturnType PacketReturnType;
  typedef typename Eigen::internal::nested<TensorBroadcastingOp>::type Nested;
  typedef typename Eigen::internal::traits<TensorBroadcastingOp>::StorageKind StorageKind;
  typedef typename Eigen::internal::traits<TensorBroadcastingOp>::Index Index;

  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE TensorBroadcastingOp(const XprType& expr, const Broadcast& broadcast)
      : m_xpr(expr), m_broadcast(broadcast) {}

    EIGEN_DEVICE_FUNC
    const Broadcast& broadcast() const { return m_broadcast; }

    EIGEN_DEVICE_FUNC
    const typename internal::remove_all<typename XprType::Nested>::type&
    expression() const { return m_xpr; }

  protected:
    typename XprType::Nested m_xpr;
    const Broadcast m_broadcast;
};


// Eval as rvalue
template<typename Broadcast, typename ArgType, typename Device>
struct TensorEvaluator<const TensorBroadcastingOp<Broadcast, ArgType>, Device>
{
  typedef TensorBroadcastingOp<Broadcast, ArgType> XprType;
  typedef typename XprType::Index Index;
  static const int NumDims = internal::array_size<typename TensorEvaluator<ArgType, Device>::Dimensions>::value;
  typedef DSizes<Index, NumDims> Dimensions;
  typedef typename XprType::Scalar Scalar;
  typedef typename TensorEvaluator<ArgType, Device>::Dimensions InputDimensions;

  enum {
    IsAligned = false,
    PacketAccess = TensorEvaluator<ArgType, Device>::PacketAccess,
  };

  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE TensorEvaluator(const XprType& op, const Device& device)
    : m_impl(op.expression(), device)
  {
    const typename TensorEvaluator<ArgType, Device>::Dimensions& input_dims = m_impl.dimensions();
    const Broadcast& broadcast = op.broadcast();
    for (int i = 0; i < NumDims; ++i) {
      eigen_assert(input_dims[i] > 0);
      m_dimensions[i] = input_dims[i] * broadcast[i];
    }

    m_inputStrides[0] = 1;
    m_outputStrides[0] = 1;
    for (int i = 1; i < NumDims; ++i) {
      m_inputStrides[i] = m_inputStrides[i-1] * input_dims[i-1];
      m_outputStrides[i] = m_outputStrides[i-1] * m_dimensions[i-1];
    }
  }

  typedef typename XprType::CoeffReturnType CoeffReturnType;
  typedef typename XprType::PacketReturnType PacketReturnType;

  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Dimensions& dimensions() const { return m_dimensions; }

  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE bool evalSubExprsIfNeeded(Scalar* /*data*/) {
    m_impl.evalSubExprsIfNeeded(NULL);
    return true;
  }

  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void cleanup() {
    m_impl.cleanup();
  }

  // TODO: attempt to speed this up. The integer divisions and modulo are slow
  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE CoeffReturnType coeff(Index index) const
  {
    Index inputIndex = 0;
    for (int i = NumDims - 1; i > 0; --i) {
      const Index idx = index / m_outputStrides[i];
      if (internal::index_statically_eq<InputDimensions>()(i, 1)) {
        eigen_assert(idx % m_impl.dimensions()[i] == 0);
      } else {
        inputIndex += (idx % m_impl.dimensions()[i]) * m_inputStrides[i];
      }
      index -= idx * m_outputStrides[i];
    }
    if (internal::index_statically_eq<Broadcast>()(0, 1)) {
      eigen_assert(index < m_impl.dimensions()[0]);
      inputIndex += index;
    } else {
      inputIndex += (index % m_impl.dimensions()[0]);
    }
    return m_impl.coeff(inputIndex);
  }

  // Ignore the LoadMode and always use unaligned loads since we can't guarantee
  // the alignment at compile time.
  template<int LoadMode>
  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE PacketReturnType packet(Index index) const
  {
    const int packetSize = internal::unpacket_traits<PacketReturnType>::size;
    EIGEN_STATIC_ASSERT(packetSize > 1, YOU_MADE_A_PROGRAMMING_MISTAKE)
    eigen_assert(index+packetSize-1 < dimensions().TotalSize());

    const Index originalIndex = index;

    Index inputIndex = 0;
    for (int i = NumDims - 1; i > 0; --i) {
      const Index idx = index / m_outputStrides[i];
      if (internal::index_statically_eq<InputDimensions>()(i, 1)) {
         eigen_assert(idx % m_impl.dimensions()[i] == 0);
      } else {
        inputIndex += (idx % m_impl.dimensions()[i]) * m_inputStrides[i];
      }
      index -= idx * m_outputStrides[i];
    }
    Index innermostLoc;
    if (internal::index_statically_eq<Broadcast>()(0, 1)) {
      eigen_assert(index < m_impl.dimensions()[0]);
      innermostLoc = index;
    } else {
      innermostLoc = index % m_impl.dimensions()[0];
    }
    inputIndex += innermostLoc;

    // Todo: this could be extended to the second dimension if we're not
    // broadcasting alongside the first dimension, and so on.
    if (innermostLoc + packetSize <= m_impl.dimensions()[0]) {
      return m_impl.template packet<Unaligned>(inputIndex);
    } else {
      EIGEN_ALIGN_DEFAULT typename internal::remove_const<CoeffReturnType>::type values[packetSize];
      values[0] = m_impl.coeff(inputIndex);
      for (int i = 1; i < packetSize; ++i) {
        values[i] = coeff(originalIndex+i);
      }
      PacketReturnType rslt = internal::pload<PacketReturnType>(values);
      return rslt;
    }
  }

  Scalar* data() const { return NULL; }

 protected:
  Dimensions m_dimensions;
  array<Index, NumDims> m_outputStrides;
  array<Index, NumDims> m_inputStrides;
  TensorEvaluator<ArgType, Device> m_impl;
};


} // end namespace Eigen

#endif // EIGEN_CXX11_TENSOR_TENSOR_BROADCASTING_H
