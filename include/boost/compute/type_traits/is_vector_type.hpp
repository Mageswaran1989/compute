//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://kylelutz.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_TYPE_TRAITS_IS_VECTOR_TYPE_HPP
#define BOOST_COMPUTE_TYPE_TRAITS_IS_VECTOR_TYPE_HPP

#include <boost/compute/type_traits/vector_size.hpp>

namespace boost {
namespace compute {

/// Meta-function returning \c true if \p T is a vector type.
///
/// For example,
/// \code
/// is_vector_type<int>::value == false
/// is_vector_type<float4_>::value == true
/// \endcode
///
/// \see make_vector_type, vector_size
template<class T>
struct is_vector_type
{
    /// \internal_
    BOOST_STATIC_CONSTANT(bool, value = (vector_size<T>::value != 1));
};

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_TYPE_TRAITS_IS_VECTOR_TYPE_HPP
