/*
 * Copyright (c) 2019, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cudf/types.hpp>

#include <memory>

namespace cudf {
namespace experimental {

/** 
 * @brief Rolling window aggregation operations
 */
enum class rolling_operator {
  SUM,       ///< Computes the sum of all values in the window
  MIN,       ///< Computes minimum value in the window
  MAX,       ///< Computes maximum value in the window
  MEAN,       ///< Computes arithmetic mean of all values in the window
  COUNT,     ///< Computes the number of values in the window
  NUMBA_UDF, ///< A user-defined aggregation operation defined in PTX code generated by `numba`
  CUDA_UDF,  ///< A generic aggregation operation defined in CUDA code
};

/**
 * @brief  Applies a fixed-size rolling window function to the values in a column.
 *
 * This function aggregates values in a window around each element i of the input column, and
 * invalidates the bit mask for element i if there are not enough observations. The window size is
 * static (the same for each element). This matches Pandas' API for DataFrame.rolling with a few
 * notable differences:
 * - instead of the center flag it uses the forward window size to allow for more flexible windows.
 *   The total window size = window + forward_window. Element i uses elements
 *   [i-window+1, i+forward_window] to do the window computation.
 * - instead of storing NA/NaN for output rows that do not meet the minimum number of observations
 *   this function updates the valid bitmask of the column to indicate which elements are valid.
 * 
 * The returned column for `op == COUNT` always has `INT32` type. All other operators return a 
 * column of the same type as the input. Therefore it is suggested to convert integer column types
 * (especially low-precision integers) to `FLOAT32` or `FLOAT64` before doing a rolling `MEAN`.
 *
 * @param[in] input_col The input column
 * @param[in] window The static rolling window size.
 * @param[in] forward_window The static window size in the forward direction.
 * @param[in] min_periods Minimum number of observations in window required to have a value,
 *                        otherwise element `i` is null.
 * @param[in] op The rolling window aggregation type (SUM, MAX, MIN, etc.)
 *
 * @returns   A nullable output column containing the rolling window results
 **/
std::unique_ptr<column> rolling_window(column_view const& input,
                                       size_type window,
                                       size_type forward_window,
                                       size_type min_periods,
                                       rolling_operator op,
                                       rmm::mr::device_memory_resource* mr =
                                        rmm::mr::get_default_resource());

/**
 * @brief  Applies a variable-size rolling window function to the values in a column.
 *
 * This function aggregates values in a window around each element i of the input column, and
 * invalidates the bit mask for element i if there are not enough observations. The window size is
 * dynamic (varying for each element). This matches Pandas' API for DataFrame.rolling with a few
 * notable differences:
 * - instead of the center flag it uses the forward window size to allow for more flexible windows. 
 *   The total window size for element i = window[i] + forward_window[i]. Element i uses elements
 *   [i-window[i]+1, i+forward_window[i]] to do the window computation.
 * - instead of storing NA/NaN for output rows that do not meet the minimum number of observations
 *   this function updates the valid bitmask of the column to indicate which elements are valid.
 * - support for dynamic rolling windows, i.e. window size can be specified for each element using
 *   an additional array.
 * 
 * The returned column for `op == COUNT` always has INT32 type. All other operators return a 
 * column of the same type as the input. Therefore it is suggested to convert integer column types
 * (especially low-precision integers) to `FLOAT32` or `FLOAT64` before doing a rolling `MEAN`.
 * 
 * @throws cudf::logic_error if window column type is not INT32
 *
 * @param[in] input_col The input column
 * @param[in] window A non-nullable column of INT32 window sizes. window[i] specifies window size
 *                   for element i.
 * @param[in] forward_window A non-nullable column of INT32 window sizes in the forward direction.
 *                           forward_window[i] specifies window size for element i.
 * @param[in] min_periods Minimum number of observations in window required to have a value,
 *                        otherwise element `i` is null.
 * @param[in] op The rolling window aggregation type (sum, max, min, etc.)
 *
 * @returns   A nullable output column containing the rolling window results
 **/
std::unique_ptr<column> rolling_window(column_view const& input,
                                       column_view const& window,
                                       column_view const& forward_window,
                                       size_type min_periods,
                                       rolling_operator op,
                                       rmm::mr::device_memory_resource* mr =
                                        rmm::mr::get_default_resource());

/**
 * @brief  Applies a fixed-size user-defined rolling window function to the values in a column.
 *
 * This function aggregates values in a window around each element i of the input column with a user
 * defined aggregator, and invalidates the bit mask for element i if there are not enough
 * observations. The window size is  static (the same for each element). This matches Pandas' API
 * for DataFrame.rolling with a few notable differences:
 * - instead of the center flag it uses the forward window size to allow for more flexible windows.
 *   The total window size = window + forward_window. Element i uses elements
 *   [i-window+1, i+forward_window] to do the window computation.
 * - instead of storing NA/NaN for output rows that do not meet the minimum number of observations
 *   this function updates the valid bitmask of the column to indicate which elements are valid.
 *
 * This function is asynchronous with respect to the GPU, i.e. the call will return before the
 * operation is completed on the GPU (unless built in debug).
 * 
 * Currently the handling of the null values is only partially implemented: it acts as if every
 * element of the input column is valid, i.e. the validity of the individual elements in the input
 * column is not checked when the number of (valid) observations are counted and the aggregator is
 * applied.
 *
 * @param[in] input_col The input column
 * @param[in] window The static rolling window size.
 * @param[in] forward_window The static window size in the forward direction.
 * @param[in] min_periods Minimum number of observations in window required to have a value,
 *                        otherwise element `i` is null.
 * @param[in] udf A CUDA string or a PTX string compiled by numba that contains the implementation
 *                of the user defined aggregator function
 * @param[in] op The user-defined rolling window aggregation type:  NUMBA_UDF (PTX string compiled
 *               by numba) or CUDA_UDF (CUDA string)
 * @param[in] output_type Output type of the user-defined aggregator (only used for NUMBA_UDF)
 * 
 * @returns   A nullable output column containing the rolling window results
 **/
std::unique_ptr<column> rolling_window(column_view const& input,
                                       size_type window,
                                       size_type forward_window,
                                       size_type min_periods,
                                       std::string const& udf,
                                       rolling_operator op,
                                       data_type output_type,
                                       rmm::mr::device_memory_resource* mr =
                                        rmm::mr::get_default_resource());

/**
 * @brief  Applies a variable-size user-defined rolling window function to the values in a column.
 *
 * This function aggregates values in a window around each element i of the input column with a user
 * defined aggregator, and invalidates the bit mask for element i if there are not enough
 * observations. The window size is dynamic (varying for each element). This matches Pandas' API
 * for DataFrame.rolling with a few notable differences:
 * - instead of the center flag it uses the forward window size to allow for more flexible windows.
 *   The total window size for element i = window[i] + forward_window[i]. Element i uses elements
 *   [i-window[i]+1, i+forward_window[i]] to do the window computation.
 * - instead of storing NA/NaN for output rows that do not meet the minimum number of observations
 *   this function updates the valid bitmask of the column to indicate which elements are valid.
 *
 * This function is asynchronous with respect to the GPU, i.e. the call will return before the
 * operation is completed on the GPU (unless built in debug).
 * 
 * Currently the handling of the null values is only partially implemented: it acts as if every
 * element of the input column is valid, i.e. the validity of the individual elements in the input
 * column is not checked when the number of (valid) observations are counted and the aggregator is
 * applied.
 *
 * @throws cudf::logic_error if window column type is not INT32
 * 
 * @param[in] input_col The input column
 * @param[in] window A non-nullable column of INT32 window sizes. window[i] specifies window size
 *                   for element i.
 * @param[in] forward_window A non-nullable column of INT32 window sizes in the forward direction.
 *                           forward_window[i] specifies window size for element i.
 * @param[in] min_periods Minimum number of observations in window required to have a value,
 *                        otherwise element `i` is null.
 * @param[in] udf A CUDA string or a PTX string compiled by numba that contains the implementation
 *                of the user defined aggregator function
 * @param[in] op The user-defined rolling window aggregation type:  NUMBA_UDF (PTX string compiled
 *               by numba) or CUDA_UDF (CUDA string)
 * @param[in] output_type Output type of the user-defined aggregator (only used for NUMBA_UDF)
 * 
 * @returns   A nullable output column containing the rolling window results
 **/
std::unique_ptr<column> rolling_window(column_view const& input,
                                       column_view const& window,
                                       column_view const& forward_window,
                                       size_type min_periods,
                                       std::string const& udf,
                                       rolling_operator op,
                                       data_type output_type,
                                       rmm::mr::device_memory_resource* mr =
                                        rmm::mr::get_default_resource());

} // namespace experimental 
} // namespace cudf
