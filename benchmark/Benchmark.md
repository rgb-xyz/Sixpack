Benchmark Results
=================

The number of calculated Kerr metric tensors per second (median of 3 measurements):

|Build Config           |AMD Ryzen 7950X3D           |AMD Ryzen 1950X           |Intel i7 4790S²         |
|-----------------------|---------------------------:|-------------------------:|-----------------------:|
|                       |(16x2 cores, 16+128MB cache)|(16x2 cores, 8+32MB cache)|(4x2 cores, 1+8MB cache)|
|MSVC 17.6 (x64 SSE2)   |               1,361,721,860|               848,954,906|             116,071,602|
|MSVC 17.6 (x64 AVX)¹   |               1,757,331,608|               780,061,911|             155,088,830|
|MSVC 17.6 (x64 AVX2)   |               2,182,598,879|               960,752,290|             174,448,950|
|MSVC 17.6 (x64 AVX512) |               2,146,006,770|                         —|                       —|
|Clang 15.0 (x64 SSE2)  |               1,529,959,735|               946,717,683|             154,080,455|
|Clang 15.0 (x64 AVX)¹  |               1,606,843,962|               790,848,512|             127,587,962|
|Clang 15.0 (x64 AVX2)  |               1,630,006,954|               787,720,221|             141,630,537|
|Clang 15.0 (x64 AVX512)|               1,598,658,514|                         —|                       —|

¹MSVC automatically inserts vectorized versions of `sin`/`cos`/`pow`. Clang does not.\
²Extremely variable results due to thermal/power throttling.