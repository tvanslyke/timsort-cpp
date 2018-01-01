#ifndef ITER_H
#define ITER_H

template <class It>
using iterator_difference_type_t = typename std::iterator_traits<It>::difference_type;

template <class It>
using iterator_value_type_t = typename std::iterator_traits<It>::value_type;

#endif /* ITER_H */
