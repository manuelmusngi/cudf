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

#include <cudf/column/column_view.hpp>
#include <cudf/column/column.hpp>

#include <rmm/thrust_rmm_allocator.h>

namespace cudf {

/**---------------------------------------------------------------------------*
 * @brief Given a column-view of strings type, an instance of this class
 * provides a wrapper on this compound column for strings operations.
 *---------------------------------------------------------------------------**/
class strings_column_view : private column_view
{
public:
    strings_column_view( column_view strings_column );
    strings_column_view( strings_column_view&& strings_view ) = default;
    strings_column_view( const strings_column_view& strings_view ) = default;
    ~strings_column_view() = default;
    strings_column_view& operator=(strings_column_view const&) = default;
    strings_column_view& operator=(strings_column_view&&) = default;

    static constexpr size_type offsets_column_index{0};
    static constexpr size_type chars_column_index{1};

    using column_view::size;
    using column_view::null_mask;
    using column_view::null_count;

    /**---------------------------------------------------------------------------*
    * @brief Returns the parent column.
    *---------------------------------------------------------------------------**/
    column_view parent() const;

    /**---------------------------------------------------------------------------*
    * @brief Returns the internal column of offsets
    *---------------------------------------------------------------------------**/
    column_view offsets() const;

    /**---------------------------------------------------------------------------*
    * @brief Returns the internal column of chars
    *---------------------------------------------------------------------------**/
    column_view chars() const;

};

namespace strings
{

/**---------------------------------------------------------------------------*
 * @brief Prints the strings to stdout.
 *
 * @param strings Strings instance for this operation.
 * @param start Index of first string to print.
 * @param end Index of last string to print. Specify -1 for all strings.
 * @param max_width Maximum number of characters to print per string.
 *        Specify -1 to print all characters.
 * @param delimiter The chars to print between each string.
 *        Default is new-line character.
 *---------------------------------------------------------------------------**/
void print( strings_column_view strings,
            size_type start=0, size_type end=-1,
            size_type max_width=-1, const char* delimiter = "\n" );

/**---------------------------------------------------------------------------*
 * @brief Create output per Arrow strings format.
 * The return pair is the array of chars and the array of offsets.
 *
 * @param strings Strings instance for this operation.
 * @param stream CUDA stream to use kernels in this method.
 * @param mr Resource for allocating device memory.
 * @return Pair containing a contiguous array of chars and an array of offsets.
 *---------------------------------------------------------------------------**/
std::pair<rmm::device_vector<char>, rmm::device_vector<size_type>>
    create_offsets( strings_column_view strings,
                    cudaStream_t stream=0,
                    rmm::mr::device_memory_resource* mr = rmm::mr::get_default_resource() );

/**---------------------------------------------------------------------------*
 * @brief Returns a new strings column created from a subset of
 * of this instance's strings column.
 *
 * ```
 * s1 = ["a", "b", "c", "d", "e", "f"]
 * s2 = sublist( s1, 2 )
 * s2 is ["c", "d", "e", "f"]
 * ```
 *
 * @param strings Strings instance for this operation.
 * @param start Index of first string to use.
 * @param end Index of last string to use.
 *        Default -1 indicates the last element.
 * @param step Increment value between indexes.
 *        Default step is 1.
 * @param stream CUDA stream to use kernels in this method.
 * @param mr Resource for allocating device memory.
 * @return New strings column of size (end-start)/step.
 *---------------------------------------------------------------------------**/
std::unique_ptr<cudf::column> sublist( strings_column_view strings,
                                       size_type start, size_type end=-1,
                                       size_type step=1,
                                       cudaStream_t stream=0,
                                       rmm::mr::device_memory_resource* mr = rmm::mr::get_default_resource() );

/**---------------------------------------------------------------------------*
 * @brief Returns a new strings column using the specified indices to select
 * elements from the specified strings column.
 *
 * ```
 * s1 = ["a", "b", "c", "d", "e", "f"]
 * map = [0, 2]
 * s2 = gather( s1, map )
 * s2 is ["a", "c"]
 * ```
 *
 * @param strings Strings instance for this operation.
 * @param gather_map The indices with which to select strings for the new column.
 *        Values must be within [0,size()) range.
 * @param stream CUDA stream to use kernels in this method.
 * @param mr Resource for allocating device memory.
 * @return New strings column of size indices.size()
 *---------------------------------------------------------------------------**/
std::unique_ptr<cudf::column> gather( strings_column_view strings,
                                      cudf::column_view gather_map,
                                      cudaStream_t stream=0,
                                      rmm::mr::device_memory_resource* mr = rmm::mr::get_default_resource() );

/**---------------------------------------------------------------------------*
 * @brief Sort types for the sort method.
 *---------------------------------------------------------------------------**/
enum sort_type {
    none=0,    ///< no sorting
    length=1,  ///< sort by string length
    name=2     ///< sort by characters code-points
};

/**---------------------------------------------------------------------------*
 * @brief Returns a new strings column that is a sorted version of the
 * strings in this instance.
 *
 * @param strings Strings instance for this operation.
 * @param stype Specify what attribute of the string to sort on.
 * @param order Sort strings in ascending or descending order.
 * @param null_order Sort nulls to the beginning or the end of the new column.
 * @param stream CUDA stream to use kernels in this method.
 * @param mr Resource for allocating device memory.
 * @return New strings column with sorted elements of this instance.
 *---------------------------------------------------------------------------**/
std::unique_ptr<cudf::column> sort( strings_column_view strings,
                                    sort_type stype,
                                    cudf::order order=cudf::order::ASCENDING,
                                    cudf::null_order null_order=cudf::null_order::BEFORE,
                                    cudaStream_t stream=0,
                                    rmm::mr::device_memory_resource* mr = rmm::mr::get_default_resource() );

/**
 * @brief Returns new instance using the provided map values and strings.
 * The map values specify the location in the new strings instance.
 * Missing values pass through from the column at those positions.
 *
 * ```
 * s1 = ["a", "b", "c", "d"]
 * s2 = ["e", "f"]
 * map = [1, 3]
 * s3 = scatter( s1, s2, m1 )
 * s3 is ["a", "e", "c", "f"]
 * ```
 *
 * @param strings Strings instance for this operation.
 * @param values The instance for which to retrieve the strings
 *        specified in map column.
 * @param scatter_map The 0-based index values to retrieve from the
 *        strings parameter. Number of values must equal the number
 *        of elements in strings pararameter (strings.size()).
 * @param stream CUDA stream to use kernels in this method.
 * @param mr Resource for allocating device memory.
 * @return New instance with the specified strings.
 */
std::unique_ptr<cudf::column> scatter( strings_column_view strings,
                                       strings_column_view values,
                                       cudf::column_view scatter_map,
                                       cudaStream_t stream=0,
                                       rmm::mr::device_memory_resource* mr = rmm::mr::get_default_resource() );
/**
 * @brief Returns new instance using the provided index values and a
 * single string. The map values specify where to place the string
 * in the new strings instance. Missing values pass through from
 * the column at those positions.
 *
 * ```
 * s1 = ["a", "b", "c", "d"]
 * map = [1, 3]
 * s2 = scatter( s1, "e", m1 )
 * s2 is ["a", "e", "c", "e"]
 * ```
 *
 * @param strings Strings instance for this operation.
 * @param value Null-terminated encoded string in host memory to use with
 *        the scatter_map.
 * @param scatter_map The 0-based index values to place the given string.
 * @param stream CUDA stream to use kernels in this method.
 * @param mr Resource for allocating device memory.
 * @return New instance with the specified strings.
 */
std::unique_ptr<cudf::column> scatter( strings_column_view strings,
                                       const char* value,
                                       cudf::column_view scatter_map,
                                       cudaStream_t stream=0,
                                       rmm::mr::device_memory_resource* mr = rmm::mr::get_default_resource() );

/**---------------------------------------------------------------------------*
 * @brief Row-wise concatenates the given list of strings columns.
 *
 * ```
 * s1 = ['aa', null, '', 'aa']
 * s2 = ['', 'bb', 'bb', null]
 * r = concatenate(s1,s2)
 * r is ['aa', null, 'bb', null]
 * ```
 *
 * @param strings_columns List of string columns to concatenate.
 * @param separator Null-terminated CPU string that should appear between each instance.
 *        Default is empty string.
 * @param narep Null-terminated CPU string that should be used in place of any null strings found.
 *        Default of null means any null operand produces a null result.
 * @param stream CUDA stream to use kernels in this method.
 * @param mr Resource for allocating device memory.
 * @return New column with concatenated results
 *---------------------------------------------------------------------------**/
std::unique_ptr<cudf::column> concatenate( std::vector<strings_column_view>& strings_columns,
                                           const char* separator="",
                                           const char* narep=nullptr,
                                           cudaStream_t stream=0,
                                           rmm::mr::device_memory_resource* mr = rmm::mr::get_default_resource() );

/**---------------------------------------------------------------------------*
 * @brief Concatenates all strings in the column into one new string.
 *
 * ```
 * s = ['aa', null, '', 'zz' ]
 * r = join_strings(s,':','_')
 * r is ['aa:_::zz']
 * ```
 *
 * @param strings Strings for this operation.
 * @param separator Null-terminated CPU string that should appear between each string.
 * @param narep Null-terminated CPU string that should represent any null strings found.
 * @param stream CUDA stream to use kernels in this method.
 * @param mr Resource for allocating device memory.
 * @return New column containing one string.
 *---------------------------------------------------------------------------**/
std::unique_ptr<cudf::column> join_strings( strings_column_view strings,
                                            const char* separator="",
                                            const char* narep=nullptr,
                                            cudaStream_t stream=0,
                                            rmm::mr::device_memory_resource* mr = rmm::mr::get_default_resource() );

} // namespace strings
} // namespace cudf
