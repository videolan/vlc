// Copyright (C) 2017-2022 Basile Fraboni
// Copyright (C) 2014 Ivan Kutskir (for the original fast blur implementation)
// All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license. For further details please refer
// to : https://mit-license.org/
//
#pragma once

#include <cmath>

//!
//! \file fast_gaussian_blur_template.h
//! \author Basile Fraboni
//! \date 2017 - 2022
//!
//! \brief This contains a C++ implementation of a fast Gaussian blur algorithm in linear time.
//!
//! The image buffer is supposed to be of size `w * h * c`, where `h` is height, `w` is width,
//! and `c` is the number of channels.
//! The default implementation only supports up to 4 channels images, but one can easily add support for any number of channels
//! using either specific template cases or a generic function that takes the number of channels as an explicit parameter.
//! This implementation is focused on learning and readability more than on performance.
//! The fast blur algorithm is performed with several box blur passes over an image.
//! The filter converges towards a true Gaussian blur after several passes thanks to the theorem central limit.
//! In practice, three passes (biquadratic filter) are sufficient for good quality results.
//! For further details please refer to:
//!     - http://blog.ivank.net/fastest-gaussian-blur.html
//!     - https://www.peterkovesi.com/papers/FastGaussianSmoothing.pdf
//!     - https://github.com/bfraboni/FastGaussianBlur
//!
//! **Note:** The fast gaussian blur algorithm is not accurate on image boundaries.
//! It performs a diffusion of the signal with several passes, each pass depending
//! on the output of the preceding one. Some of the diffused signal is lost near borders and results in a slight
//! loss of accuracy for next pass. This problem can be solved by increasing the image support of
//! half the box kernel extent at each pass of the algorithm. The added padding would in this case
//! capture the diffusion and make the next pass accurate.
//! On contrary true Gaussian blur does not suffer this problem since the whole diffusion process
//! is performed in one pass only.
//! The extra padding is not performed in this implementation, however we provide several border
//! policies resulting in dfferent approximations and accuracies.
//!

//!
//! \brief Enumeration that decribes border policies for filters.
//!
//! For a detailed description of border policies please refer to:
//! - https://en.wikipedia.org/wiki/Kernel_(image_processing)#Edge_Handling
//! - https://www.intel.com/content/www/us/en/develop/documentation/ipp-dev-reference/top/volume-2-image-processing/filtering-functions-2/user-defined-border-types.html
//! - https://docs.opencv.org/3.4/d2/de8/group__core__array.html#ga209f2f4869e304c82d07739337eae7c5
//! - http://iihm.imag.fr/Docs/java/jai1_0guide/Image-enhance.doc.html
//!
enum Border
{
    kExtend,
    kKernelCrop,
    kMirror,
    kWrap,
};

//!
//! Sliding kernel accumulation has 4 cases:
//! 1. left side out and right side in
//! 2. left side in and right side in
//! 3. left side in and right side out
//! 4. left side out and right side out
//!
//! Small (S) kernels corresponds to kernels with radius < width; r < w
//! Mid   (M) kernels corresponds to kernels with kernel size < width; 2r+1 < w
//! Large (L) kernels corresponds to kernels with radius > width; r > w
//!
//! The fast version for (S) results in 3 loops for cases 1, 2 and 3.
//! The fast version for (M) results in 3 loops for cases 1, 4 and 3.
//! The fast version for (L) results in 1 loop for cases 4.
//!
enum Kernel
{
    kSmall,
    kMid,
    kLarge,
};

//!
//! \brief This function performs a single separable horizontal box blur pass with border extend policy.
//! Templated by buffer data type T, buffer number of channels C.
//! Faster version for kernels that are smaller than the image width (r <= w).
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] r            box dimension
//!
template<typename T, int C, Kernel kernel = kSmall>
inline void horizontal_blur_extend(const T * in, T * out, const int w, const int h, const int r)
{
    // change the local variable types depending on the template type for faster calculations
    using calc_type = std::conditional_t<std::is_integral_v<T>, int, float>;

    const float iarr = 1.f / (r+r+1);
    #pragma omp parallel for
    for(int i=0; i<h; i++)
    {
        const int begin = i*w;
        const int end = begin+w;
        calc_type fv[C], lv[C], acc[C];                 // first value, last value, sliding accumulator

        // init fv, lv, acc by extending outside the image buffer
        for(int ch=0; ch<C; ++ch)
        {
            fv[ch] =  in[begin*C+ch];
            lv[ch] =  in[(end-1)*C+ch];
            acc[ch] = (r+1)*fv[ch];
        }

        if constexpr(kernel == kLarge)
        {
            // initial acucmulation
            for(int j=0; j<r; j++)
            for(int ch=0; ch<C; ++ch)
            {
                // prefilling the accumulator with the last value seems slower than/equal to this ternary
                acc[ch] += j < w ? in[(begin+j)*C+ch] : lv[ch];
            }

            for(int ti = begin; ti < end; ti++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += lv[ch] - fv[ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
        else if constexpr(kernel == kMid)
        {
            // current index, left index, right index
            int ti = begin, li = begin-r-1, ri = begin+r;

            // initial acucmulation
            for(int j=ti; j<ri; j++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[j*C+ch];
            }

            // 1. left side out and right side in
            for(; ri<end; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[ri*C+ch] - fv[ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 4. left side out and right side out
            for(; li<begin; ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += lv[ch] - fv[ch]; //! mid kernels
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 3. left side in and right side out
            for(; ti<end; ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += lv[ch] - in[li*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
        else if constexpr(kernel == kSmall)
        {
            // current index, left index, right index
            int ti = begin, li = begin-r-1, ri = begin+r;

            // initial acucmulation
            for(int j=ti; j<ri; j++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[j*C+ch];
            }

            // 1. left side out and right side in
            for(; li<begin; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[ri*C+ch] - fv[ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 2. left side in and right side in
            for(; ri<end; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[ri*C+ch] - in[li*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 3. left side in and right side out
            for(; ti<end; ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += lv[ch] - in[li*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
    }
}

//!
//! \brief This function performs a single separable horizontal box blur pass with kernel crop border policy.
//! Templated by buffer data type T, buffer number of channels C.
//! Faster version for kernels that are smaller than the image width (r <= w).
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] r            box dimension
//!
template<typename T, int C, Kernel kernel = kSmall>
inline void horizontal_blur_kernel_crop(const T * in, T * out, const int w, const int h, const int r)
{
    // change the local variable types depending on the template type for faster calculations
    using calc_type = std::conditional_t<std::is_integral_v<T>, int, float>;

    const float iarr = 1.f / (r+r+1);
    const float iwidth = 1.f / w;
    #pragma omp parallel for
    for(int i=0; i<h; i++)
    {
        const int begin = i*w;
        const int end = begin+w;
        calc_type acc[C] = { 0 };

        if constexpr(kernel == kLarge)
        {
            // initial acucmulation
            for(int j=begin; j<end; j++)
            for(int ch=0; ch < C; ++ch)
            {
                acc[ch] += in[j*C+ch];
            }

            // this is constant
            for(int j=begin; j<end; j++)
            for(int ch=0; ch < C; ++ch)
            {
                out[j*C+ch] = acc[ch]*iwidth + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
        else if constexpr(kernel == kMid)
        {
            // current index, left index, right index
            int ti = begin, li = begin-r-1, ri = begin+r;

            // initial acucmulation
            for(int j=ti; j<ri; j++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[j*C+ch];
            }

            // 1. left side out and right side in
            for(; ri<end; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[ri*C+ch];
                // assert(acc[ch] >= 0);
                const float inorm = 1.f / float(ri+1-begin);
                out[ti*C+ch] = acc[ch]*inorm + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 4. left side out and right side out
            for(; li<begin; ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                out[ti*C+ch] = acc[ch]*iwidth + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 3. left side in and right side out
            for(; ti<end; ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] -= in[li*C+ch];
                // assert(acc[ch] >= 0);
                const float inorm = 1.f / float(end-li-1);
                out[ti*C+ch] = acc[ch]*inorm + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
        else if constexpr(kernel == kSmall)
        {
            // current index, left index, right index
            int ti = begin, li = begin-r-1, ri = begin+r;

            // initial acucmulation
            for(int j=ti; j<ri; j++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[j*C+ch];
            }

            // 1. left side out and right side in
            for(; li<begin; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[ri*C+ch];
                // assert(acc[ch] >= 0);
                const float inorm = 1.f / float(ri+1-begin);
                out[ti*C+ch] = acc[ch]*inorm + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 2. left side in and right side in
            for(; ri<end; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[ri*C+ch] - in[li*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 3. left side in and right side out
            for(; ti<end; ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] -= in[li*C+ch];
                // assert(acc[ch] >= 0);
                const float inorm = 1.f / float(end-li-1);
                out[ti*C+ch] = acc[ch]*inorm + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
    }
}

//! Helper to compute array indices for mirror and wrap border policies.
struct Index
{
    //! wrap index
    static inline int wrap(const int begin, const int end, const int index)
    {
        const int length = end-begin;
        const int repeat = std::abs(index / length)+1;
        const int value = index + repeat * length;
        return begin+(value%length);
    }

    //! mirror without repetition index
    static inline int mirror(const int begin, const int end, const int index)
    {
        if(index >= begin && index < end)
            return index;

        const int length = end-begin, last = end-1, slength = length-1;
        const int pindex = index < begin ? last-index+slength : index-begin;
        const int repeat = pindex / slength;
        const int mod = pindex % slength;
        return repeat%2 ? slength-mod+begin : mod+begin;
    }
};

//!
//! \brief This function performs a single separable horizontal box blur pass with mirror border policy.
//! Templated by buffer data type T, buffer number of channels C.
//! Faster version for kernels that are smaller than the image width (r < w).
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] r            box dimension
//!
//! \todo Rework this one at some point.
template<typename T, int C, Kernel kernel = kSmall>
inline void horizontal_blur_mirror(const T* in, T* out, const int w, const int h, const int r)
{
    // change the local variable types depending on the template type for faster calculations
    using calc_type = std::conditional_t<std::is_integral_v<T>, int, float>;

    const double iarr = 1.f/(r+r+1);
    #pragma omp parallel for
    for (int i = 0; i < h; i++)
    {
        const int begin = i*w;
        const int end = begin+w;
        calc_type acc[C] = { 0 };

        // current index, left index, right index
        int ti = begin, li = begin-r-1, ri = begin+r;

        if constexpr(kernel == kLarge) // generic but slow
        {
            // initial acucmulation
            for(int j=li; j<ri; j++)
            for(int ch=0; ch<C; ++ch)
            {
                const int id = Index::mirror(begin, end, j); // mirrored id
                acc[ch] += in[id*C+ch];
            }

            // perform filtering
            for(int j=0; j<w; j++, ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                const int rid = Index::mirror(begin, end, ri); // right mirrored id
                const int lid = Index::mirror(begin, end, li); // left mirrored id
                acc[ch] += in[rid*C+ch] - in[lid*C+ch];
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
        else if constexpr(kernel == kMid)
        {
            for(int j=li; j<begin; j++)
            for(int ch=0; ch<C; ++ch)
            {
                const int lid = 2 * begin - j; // mirrored id
                acc[ch] += in[lid*C+ch];
            }

            for(int j=begin; j<ri; j++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[j*C+ch];
            }

            // 1. left side out and right side in
            for(; ri<end; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                const int lid = 2 * begin - li; // left mirrored id
                acc[ch] += in[ri*C+ch] - in[lid*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 4. left side out and right side out
            for(; li<begin; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                const int rid = 2 * end - 2 - ri;   // right mirrored id
                const int lid = 2 * begin - li;     // left mirrored id
                acc[ch] += in[rid*C+ch] - in[lid*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 3. left side in and right side out
            for(; ti<end; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                const int rid = 2*end-2-ri; // right mirrored id
                acc[ch] += in[rid*C+ch] - in[li*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
        else if constexpr(kernel == kSmall)
        {
            for(int j=li; j<begin; j++)
            for(int ch=0; ch<C; ++ch)
            {
                const int lid = 2 * begin - j; // mirrored id
                acc[ch] += in[lid*C+ch];
            }

            for(int j=begin; j<ri; j++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[j*C+ch];
            }

            // 1. left side out and right side in
            for(; li<begin; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                const int lid = 2 * begin - li; // left mirrored id
                acc[ch] += in[ri*C+ch] - in[lid*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 2. left side in and right side in
            for(; ri<end; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                acc[ch] += in[ri*C+ch] - in[li*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }

            // 3. left side in and right side out
            for(; ti<end; ri++, ti++, li++)
            for(int ch=0; ch<C; ++ch)
            {
                const int rid = 2*end-2-ri; // right mirrored id
                acc[ch] += in[rid*C+ch] - in[li*C+ch];
                // assert(acc[ch] >= 0);
                out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
            }
        }
    }
}

//!
//! \brief This function performs a single separable horizontal box blur pass with mirror border policy.
//! Templated by buffer data type T, buffer number of channels C.
//! Generic version for kernels that are larger than the image width (r >= w).
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] r            box dimension
//!
//! \todo Make a faster version for small kernels.
template<typename T, int C>
inline void horizontal_blur_wrap(const T* in, T* out, const int w, const int h, const int r)
{
    // change the local variable types depending on the template type for faster calculations
    using calc_type = std::conditional_t<std::is_integral_v<T>, int, float>;

    const float iarr = 1.f / (r+r+1);
    #pragma omp parallel for
    for(int i=0; i<h; i++)
    {
        const int begin = i*w;
        const int end = begin+w;
        int ti = begin, li = begin-r-1, ri = begin+r;   // current index, left index, right index
        calc_type acc[C] = { 0 };                       // sliding accumulator

        // initial acucmulation
        for(int j=li; j<ri; j++)
        for(int ch=0; ch<C; ++ch)
        {
            const int id = Index::wrap(begin, end, j); // wrapped id
            acc[ch] += in[id*C+ch];
        }

        // perform filtering
        for(int j=0; j<w; j++, ri++, ti++, li++)
        for(int ch=0; ch<C; ++ch)
        {
            const int rid = Index::wrap(begin, end, ri); // right wrapped id
            const int lid = Index::wrap(begin, end, li); // left wrapped id
            acc[ch] += in[rid*C+ch] - in[lid*C+ch];
            out[ti*C+ch] = acc[ch]*iarr + (std::is_integral_v<T> ? 0.5f : 0.f); // fixes darkening with integer types
        }
    }
}

//!
//! \brief Utility template dispatcher function for horizontal_blur.
//! Templated by buffer data type T, buffer number of channels C, and border policy P.
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] r            box dimension
//!
template<typename T, int C, Border P = kMirror>
inline void horizontal_blur(const T * in, T * out, const int w, const int h, const int r)
{
    if constexpr(P == kExtend)
    {
        if( r < w/2 )       horizontal_blur_extend<T,C,Kernel::kSmall>(in, out, w, h, r);
        else if( r < w )    horizontal_blur_extend<T,C,Kernel::kMid  >(in, out, w, h, r);
        else                horizontal_blur_extend<T,C,Kernel::kLarge>(in, out, w, h, r);
    }
    else if constexpr(P == kKernelCrop)
    {
        if( r < w/2 )       horizontal_blur_kernel_crop<T,C,Kernel::kSmall>(in, out, w, h, r);
        else if( r < w )    horizontal_blur_kernel_crop<T,C,Kernel::kMid  >(in, out, w, h, r);
        else                horizontal_blur_kernel_crop<T,C,Kernel::kLarge>(in, out, w, h, r);
    }
    else if constexpr(P == kMirror)
    {
        if( r < w/2 )       horizontal_blur_mirror<T,C,Kernel::kSmall>(in, out, w, h, r);
        else if( r < w )    horizontal_blur_mirror<T,C,Kernel::kMid  >(in, out, w, h, r);
        else                horizontal_blur_mirror<T,C,Kernel::kLarge>(in, out, w, h, r);
    }
    else if constexpr(P == kWrap)
    {
        horizontal_blur_wrap<T,C>(in, out, w, h, r);
    }
}

//!
//! \brief Utility template dispatcher function for horizontal_blur. Templated by buffer data type T and border policy P.
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] c            image channels
//! \param[in] r            box dimension
//!
template<typename T, Border P = kMirror>
inline void horizontal_blur(const T * in, T * out, const int w, const int h, const int c, const int r)
{
    switch(c)
    {
        case 1: horizontal_blur<T,1,P>(in, out, w, h, r); break;
        case 2: horizontal_blur<T,2,P>(in, out, w, h, r); break;
        case 3: horizontal_blur<T,3,P>(in, out, w, h, r); break;
        case 4: horizontal_blur<T,4,P>(in, out, w, h, r); break;
        default: printf("horizontal_blur over %d channels is not supported yet. Add a specific case if possible or fall back to the generic version.\n", c); break;
        // default: horizontal_blur<T>(in, out, w, h, c, r); break;
    }
}

//!
//! \brief This function performs a 2D tranposition of an image.
//!
//! The transposition is done per
//! block to reduce the number of cache misses and improve cache coherency for large image buffers.
//! Templated by buffer data type T and buffer number of channels C.
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//!
template<typename T, int C>
inline void flip_block(const T * in, T * out, const int w, const int h)
{
    constexpr int block = 256/C;
    #pragma omp parallel for collapse(2)
    for(int x= 0; x < w; x+= block)
    for(int y= 0; y < h; y+= block)
    {
        const T * p = in + y*w*C + x*C;
        T * q = out + y*C + x*h*C;

        const int blockx= std::min(w, x+block) - x;
        const int blocky= std::min(h, y+block) - y;
        for(int xx= 0; xx < blockx; xx++)
        {
            for(int yy= 0; yy < blocky; yy++)
            {
                for(int k= 0; k < C; k++)
                    q[k]= p[k];
                p+= w*C;
                q+= C;
            }
            p+= -blocky*w*C + C;
            q+= -blocky*C + h*C;
        }
    }
}
//!
//! \brief Utility template dispatcher function for flip_block. Templated by buffer data type T.
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] c            image channels
//!
template<typename T>
inline void flip_block(const T * in, T * out, const int w, const int h, const int c)
{
    switch(c)
    {
        case 1: flip_block<T,1>(in, out, w, h); break;
        case 2: flip_block<T,2>(in, out, w, h); break;
        case 3: flip_block<T,3>(in, out, w, h); break;
        case 4: flip_block<T,4>(in, out, w, h); break;
        default: printf("flip_block over %d channels is not supported yet. Add a specific case if possible or fall back to the generic version.\n", c); break;
        // default: flip_block<T>(in, out, w, h, c); break;
    }
}

//!
//! \brief This function converts the standard deviation of
//! Gaussian blur into a box radius for each box blur pass.
//! Returns the approximate sigma value achieved with the N box blur passes.
//!
//! For further details please refer to :
//! - https://www.peterkovesi.com/papers/FastGaussianSmoothing.pdf
//!
//! \param[out] boxes   box radiis for kernel sizes of 2*boxes[i]+1
//! \param[in] sigma    Gaussian standard deviation
//! \param[in] n        number of box blur pass
//!
inline float sigma_to_box_radius(int boxes[], const float sigma, const int n)
{
    // ideal filter width
    float wi = std::sqrt((12*sigma*sigma/n)+1);
    int wl = wi; // no need std::floor
    if(wl%2==0) wl--;
    int wu = wl+2;

    float mi = (12*sigma*sigma - n*wl*wl - 4*n*wl - 3*n)/(-4*wl - 4);
    int m = mi+0.5f; // avoid std::round by adding 0.5f and cast to integer type

    for(int i=0; i<n; i++)
        boxes[i] = ((i < m ? wl : wu) - 1) / 2;

    return std::sqrt((m*wl*wl+(n-m)*wu*wu-n)/12.f);
}

//!
//! \brief This function performs a fast Gaussian blur. Templated by buffer data type T and number of passes N.
//!
//! Applying several times box blur tends towards a true Gaussian blur (thanks TCL). Three passes are sufficient
//! for good results. Templated by buffer data type T and number of passes N. The input buffer is also used
//! as temporary and modified during the process hence it can not be constant.
//!
//! Usually the process should alternate between horizontal and vertical passes
//! as much times as we want box blur passes. However thanks to box blur properties
//! the separable passes can be performed in any order without changing the result.
//! Hence for performance purposes the algorithm is:
//! - apply N times horizontal blur (horizontal passes)
//! - flip the image buffer (transposition)
//! - apply N times horizontal blur (vertical passes)
//! - flip the image buffer (transposition)
//!
//! We provide two version of the function:
//! - generic N passes (in which more std::swap are used)
//! - specialized 3 passes only
//!
//! \param[in,out] in       source buffer reference ptr
//! \param[in,out] out      target buffer reference ptr
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] c            image channels
//! \param[in] sigma        Gaussian standard deviation
//!
template<typename T, unsigned int N, Border P>
inline void fast_gaussian_blur(T *& in, T *& out, const int w, const int h, const int c, const float sigma)
{
    // compute box kernel sizes
    int boxes[N];
    sigma_to_box_radius(boxes, sigma, N);

    // perform N horizontal blur passes
    for(unsigned int i = 0; i < N; ++i)
    {
        horizontal_blur<T,P>(in, out, w, h, c, boxes[i]);
        std::swap(in, out);
    }

    // flip buffer
    flip_block(in, out, w, h, c);
    std::swap(in, out);

    // perform N horizontal blur passes on flipped image
    for(unsigned int i = 0; i < N; ++i)
    {
        horizontal_blur<T,P>(in, out, h, w, c, boxes[i]);
        std::swap(in, out);
    }

    // flip buffer
    flip_block(in, out, h, w, c);
}

// specialized 3 passes
template<typename T, Border P>
inline void fast_gaussian_blur(T *& in, T *& out, const int w, const int h, const int c, const float sigma)
{
    // compute box kernel sizes
    int boxes[3];
    sigma_to_box_radius(boxes, sigma, 3);

    // perform 3 horizontal blur passes
    horizontal_blur<T,P>(in, out, w, h, c, boxes[0]);
    horizontal_blur<T,P>(out, in, w, h, c, boxes[1]);
    horizontal_blur<T,P>(in, out, w, h, c, boxes[2]);

    // flip buffer
    flip_block(out, in, w, h, c);

    // perform 3 horizontal blur passes on flipped image
    horizontal_blur<T,P>(in, out, h, w, c, boxes[0]);
    horizontal_blur<T,P>(out, in, h, w, c, boxes[1]);
    horizontal_blur<T,P>(in, out, h, w, c, boxes[2]);

    // flip buffer
    flip_block(out, in, h, w, c);

    // swap pointers to get result in the ouput buffer
    std::swap(in, out);
}

//!
//! \brief Utility template dispatcher function for fast_gaussian_blur. Templated by buffer data type T and border policy P.
//!
//! This is the main exposed function and the one that should be used in programs.
//!
//! \param[in,out] in       source buffer reference ptr
//! \param[in,out] out      target buffer reference ptr
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] c            image channels
//! \param[in] sigma        Gaussian standard deviation
//! \param[in] n            number of passes, should be > 0
//!
template<typename T, Border P = kMirror>
void fast_gaussian_blur(T *& in, T *& out, const int w, const int h, const int c, const float sigma, const unsigned int n)
{
    switch(n)
    {
        case 1: fast_gaussian_blur<T,1,P>(in, out, w, h, c, sigma); break;
        case 2: fast_gaussian_blur<T,2,P>(in, out, w, h, c, sigma); break;
        case 3: fast_gaussian_blur<T,  P>(in, out, w, h, c, sigma); break; // specialized 3 passes version
        case 4: fast_gaussian_blur<T,4,P>(in, out, w, h, c, sigma); break;
        case 5: fast_gaussian_blur<T,5,P>(in, out, w, h, c, sigma); break;
        case 6: fast_gaussian_blur<T,6,P>(in, out, w, h, c, sigma); break;
        case 7: fast_gaussian_blur<T,7,P>(in, out, w, h, c, sigma); break;
        case 8: fast_gaussian_blur<T,8,P>(in, out, w, h, c, sigma); break;
        case 9: fast_gaussian_blur<T,9,P>(in, out, w, h, c, sigma); break;
        case 10: fast_gaussian_blur<T,10,P>(in, out, w, h, c, sigma); break;
        default: printf("fast_gaussian_blur with %d passes is not supported yet. Add a specific case if possible or fall back to the generic version.\n", n); break;
        // default: fast_gaussian_blur<T,10>(in, out, w, h, c, sigma, n); break;
    }
}

//!
//! \brief Utility template dispatcher function for fast_gaussian_blur. Templated by buffer data type.
//!
//! This is the main exposed function and the one that should be used in programs.
//!
//! \param[in,out] in       source buffer reference ptr
//! \param[in,out] out      target buffer reference ptr
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] c            image channels
//! \param[in] sigma        Gaussian standard deviation
//! \param[in] n            number of passes, should be > 0
//! \param[in] p            border policy {kExtend, kMirror, kKernelCrop, kWrap}
//!
template<typename T>
void fast_gaussian_blur(T *& in, T *& out, const int w, const int h, const int c, const float sigma, const unsigned int n, const Border p)
{
    switch(p)
    {
        case kExtend:       fast_gaussian_blur<T, kExtend>       (in, out, w, h, c, sigma, n); break;
        case kMirror:       fast_gaussian_blur<T, kMirror>       (in, out, w, h, c, sigma, n); break;
        case kKernelCrop:   fast_gaussian_blur<T, kKernelCrop>   (in, out, w, h, c, sigma, n); break;
        case kWrap:         fast_gaussian_blur<T, kWrap>         (in, out, w, h, c, sigma, n); break;
    }
}
